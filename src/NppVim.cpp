//NppVim.cpp
#include <windows.h>
#include <map>
#include <string>
#include <fstream>
#include <shellapi.h>
#include <commctrl.h>
#include <vector>
#include <algorithm>
#pragma comment(lib, "Version.lib")

#include "../plugin/PluginInterface.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/menuCmdID.h"

#include "../include/NppVim.h"
#include "../include/NormalMode.h"
#include "../include/VisualMode.h"
#include "../include/CommandMode.h"
#include "../include/Utils.h"
#include "../plugin/resource.h"
#include "../include/Motion.h"
#include "../include/ConfigManager.h"
#include "../include/OptionRegistry.h"
#include "../include/MappingManager.h"
#include "../include/RcParser.h"
#include <algorithm>

HINSTANCE g_hInstance = nullptr;
NppData nppData;
VimState state;

NormalMode* g_normalMode = nullptr;
VisualMode* g_visualMode = nullptr;
CommandMode* g_commandMode = nullptr;

const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 8;
FuncItem funcItem[nbFunc];

#define COLOR_BG RGB(250, 250, 250)
#define COLOR_ACCENT RGB(0, 120, 212)
#define COLOR_DONATE RGB(0, 103, 184)
#define COLOR_TEXT RGB(32, 32, 32)
#define COLOR_TEXT_LIGHT RGB(96, 96, 96)

static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushAccent = NULL;
static HBRUSH g_hBrushDonate = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontButton = NULL;
static std::map<HWND, WNDPROC> origProcMap;
static WNDPROC g_origNppProc = nullptr;

VimConfig g_config;
HKL g_userLayout = NULL;
HKL g_englishLayout = NULL;

// Forward declarations
LRESULT CALLBACK ScintillaHookProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK NppHostHookProc(HWND, UINT, WPARAM, LPARAM);
void installScintillaHookFor(HWND hwnd);
void installNppHook();
void removeAllScintillaHooks();
void ensureScintillaHooks();
void toggleVimMode();
void showConfigDialog();
void about();
void loadConfig();
void saveConfig();
void initializeOptions();
void updateRelativeLineNumbers(HWND hwnd, bool force = false);

void installNppHook() {
    if (nppData._nppHandle && !g_origNppProc) {
        g_origNppProc = (WNDPROC)SetWindowLongPtr(nppData._nppHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(NppHostHookProc));
    }
}

void removeNppHook() {
    if (nppData._nppHandle && g_origNppProc) {
        SetWindowLongPtr(nppData._nppHandle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origNppProc));
        g_origNppProc = nullptr;
    }
}

LRESULT CALLBACK NppHostHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COMMAND) {
        int cmd = LOWORD(wParam);
        if (state.vimEnabled && (state.mode == NORMAL || state.mode == VISUAL)) {
            HWND hwndEdit = Utils::getCurrentScintillaHandle();
            if (cmd == IDM_EDIT_DUP_LINE && g_config.overrideCtrlD) {
                Motion::pageDown(hwndEdit);
                return 0;
            }
            if ((cmd == IDM_EDIT_UPPERCASE || cmd == IDM_EDIT_LOWERCASE) && g_config.overrideCtrlU) {
                Motion::pageUp(hwndEdit);
                return 0;
            }
            if (cmd == IDM_EDIT_REDO && g_config.overrideCtrlR) {
                ::SendMessage(hwndEdit, SCI_REDO, 0, 0);
                return 0;
            }
            if (cmd == IDM_SEARCH_FIND && g_config.overrideCtrlF) {
                Motion::pageDown(hwndEdit); 
                return 0;
            }
            if ((cmd == IDM_EDIT_BEGINENDSELECT_COLUMNMODE || cmd == IDM_SEARCH_GOTOMATCHINGBRACE) && g_config.overrideCtrlB) {
                Motion::pageUp(hwndEdit);
                return 0;
            }
            if (cmd == IDM_FILE_OPEN && g_config.overrideCtrlO) {
                if (g_normalMode) g_normalMode->jumpBackward(hwndEdit);
                return 0;
            }
            if ((cmd == IDM_SEARCH_FINDINCREMENT || cmd == IDM_EDIT_SPLIT_LINES) && g_config.overrideCtrlI) {
                if (g_normalMode) g_normalMode->jumpForward(hwndEdit);
                return 0;
            }
            if (cmd == IDM_EDIT_PASTE && g_config.overrideCtrlV) {
                // Visual Block mode (Vim Ctrl+V)
                if (state.mode == VISUAL && state.isBlockVisual) g_normalMode->enter(); 
                else g_visualMode->enterBlock(hwndEdit);
                return 0;
            }
            if (cmd == IDM_EDIT_SELECTALL && g_config.overrideCtrlA) {
                if (g_normalMode) g_normalMode->incrementNumber(hwndEdit, 1);
                return 0;
            }
            if (cmd == IDM_EDIT_CUT && g_config.overrideCtrlX) {
                if (g_normalMode) g_normalMode->decrementNumber(hwndEdit, 1);
                return 0;
            }
        }
    }
    return CallWindowProc(g_origNppProc, hwnd, msg, wParam, lParam);
}

// State for sequence detection
static char g_firstKey = 0;
static DWORD g_firstKeyTime = 0;

void setNppData(NppData data) {
    Utils::nppData = data;
}

bool isNativeLineNumberEnabled() {
    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPMAINMENU, 0);
    if (!hMenu) return false;
    UINT state = GetMenuState(hMenu, IDM_VIEW_LINENUMBER, MF_BYCOMMAND);
    return (state != (UINT)-1) && (state & MF_CHECKED);
}

void updateRelativeLineNumbers(HWND hwnd, bool force) {
    if (!hwnd || !state.vimEnabled) return;
    
    auto& reg = OptionRegistry::getInstance();
    bool relNum = std::get<bool>(reg.getOption("relativenumber"));
    bool absNum = std::get<bool>(reg.getOption("number"));
    
    struct MarginState {
        int currentLine = -1;
        int firstVisibleLine = -1;
        COLORREF lastBg = 0xFFFFFFFF;
        int lastWidth = -1;
        bool lastRelNum = false;
        bool lastAbsNum = false;
    };
    static std::map<HWND, MarginState> states;
    auto& s = states[hwnd];

    // Case 1: Everything Disabled (set nonu nornu)
    if (!relNum && !absNum) {
        if (s.lastRelNum || s.lastAbsNum || force) {
            // Restore native numbering margin type and clear our text
            ::SendMessage(hwnd, SCI_MARGINTEXTCLEARALL, 0, 0);
            ::SendMessage(hwnd, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
            ::SendMessage(nppData._nppHandle, NPPM_SETLINENUMBERWIDTHMODE, 0, LINENUMWIDTH_CONSTANT);
            
            // Sync with NPP native toggle if it's still 'on'
            if (isNativeLineNumberEnabled()) {
                ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_LINENUMBER);
            }
            
            s.lastRelNum = false;
            s.lastAbsNum = false;
            s.lastWidth = 0;
            s.currentLine = -1;
        }
        return;
    }

    // Case 2: Standard Absolute Numbers Only (set nu nornu)
    if (!relNum && absNum) {
        if (s.lastRelNum || !s.lastAbsNum || force) {
            ::SendMessage(hwnd, SCI_MARGINTEXTCLEARALL, 0, 0);
            ::SendMessage(hwnd, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
            ::SendMessage(nppData._nppHandle, NPPM_SETLINENUMBERWIDTHMODE, 0, LINENUMWIDTH_DYNAMIC);

            // Sync with NPP native toggle if it's 'off'
            if (!isNativeLineNumberEnabled()) {
                ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_LINENUMBER);
            }
            
            s.lastRelNum = false;
            s.lastAbsNum = true;
            s.lastWidth = 50;
            s.currentLine = -1;
        }
        return;
    }

    // Case 3: Relative numbering active (pure or hybrid)
    int currentLine = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0), 0);
    int firstVisibleLine = (int)::SendMessage(hwnd, SCI_GETFIRSTVISIBLELINE, 0, 0);
    
    // Sync theme colors to our margin
    COLORREF bg = (COLORREF)::SendMessage(hwnd, SCI_STYLEGETBACK, STYLE_LINENUMBER, 0);
    
    // Check if we even need to update margin configuration type/offset
    if (::SendMessage(hwnd, SCI_GETMARGINTYPEN, 0, 0) != SC_MARGIN_RTEXT || force) {
        ::SendMessage(hwnd, SCI_SETMARGINTYPEN, 0, SC_MARGIN_RTEXT);
        ::SendMessage(hwnd, SCI_SETMARGINBACKN, 0, bg);
        ::SendMessage(hwnd, SCI_MARGINSETSTYLEOFFSET, STYLE_LINENUMBER, 0);
        
        // As suggested: Use constant width mode to prevent N++ fighting us
        ::SendMessage(nppData._nppHandle, NPPM_SETLINENUMBERWIDTHMODE, 0, LINENUMWIDTH_CONSTANT);
        
        s.lastBg = bg;
        s.currentLine = -1; // Force text update
    }

    // Only update background if it changed
    if (s.lastBg != bg) {
        ::SendMessage(hwnd, SCI_SETMARGINBACKN, 0, bg);
        s.lastBg = bg;
    }

    // Dynamic width calculation - only update if it changed
    int lineCount = (int)::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
    int charWidth = (int)::SendMessage(hwnd, SCI_TEXTWIDTH, STYLE_LINENUMBER, (LPARAM)"9");
    int digits = (int)log10(max(1, lineCount)) + 1;
    int targetWidth = (digits + 1) * charWidth;
    if (s.lastWidth != targetWidth) {
        ::SendMessage(hwnd, SCI_SETMARGINWIDTHN, 0, targetWidth);
        s.lastWidth = targetWidth;
    }

    // Only update text if cursor moved or scrolled, unless forced
    if (!force && s.currentLine == currentLine && s.firstVisibleLine == firstVisibleLine && s.lastRelNum == relNum && s.lastAbsNum == absNum) return;
    s.currentLine = currentLine;
    s.firstVisibleLine = firstVisibleLine;
    s.lastRelNum = relNum;
    s.lastAbsNum = absNum;

    int displayLines = (int)::SendMessage(hwnd, SCI_LINESONSCREEN, 0, 0);
    for (int i = 0; i <= displayLines; i++) {
        int displayLine = firstVisibleLine + i;
        int docLine = (int)::SendMessage(hwnd, SCI_DOCLINEFROMVISIBLE, displayLine, 0);
        if (docLine < 0 || docLine >= lineCount) continue;

        int rel = abs(docLine - currentLine);
        char buf[32];
        
        if (rel == 0) {
            // Proper Vim behavior: Hybrid only if 'number' is also on
            if (absNum) sprintf(buf, "%d", docLine + 1);
            else sprintf(buf, "0");
        } else {
            sprintf(buf, "%d", rel);
        }
        
        ::SendMessage(hwnd, SCI_MARGINSETTEXT, docLine, (LPARAM)buf);
        ::SendMessage(hwnd, SCI_MARGINSETSTYLE, docLine, 0); // Offset 33 makes this STYLE_LINENUMBER
    }
}

void initializeOptions() {
    auto& reg = OptionRegistry::getInstance();
    
    reg.registerOption("number", OptionType::Bool, false, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) {
            updateRelativeLineNumbers(hwnd, true);
        }
    }, "Show line numbers");

    reg.registerOption("relativenumber", OptionType::Bool, false, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) updateRelativeLineNumbers(hwnd, true);
    }, "Show relative line numbers");

    reg.registerOption("hlsearch", OptionType::Bool, true, nullptr, "Highlight search matches");
    reg.registerOption("ignorecase", OptionType::Bool, false, nullptr, "Ignore case in search");
    reg.registerOption("smartcase", OptionType::Bool, false, nullptr, "Override ignorecase if pattern contains uppercase");
    reg.registerOption("clipboard", OptionType::String, std::string("unnamed"), nullptr, "Clipboard settings");

    // Vim-specific Options
    reg.registerOption("expandtab", OptionType::Bool, false, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) ::SendMessage(hwnd, SCI_SETUSETABS, std::get<bool>(v) ? 0 : 1, 0);
    }, "Use spaces instead of tabs");

    reg.registerOption("tabstop", OptionType::Number, 4, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) ::SendMessage(hwnd, SCI_SETTABWIDTH, std::get<int>(v), 0);
    }, "Number of spaces that a <Tab> in the file counts for");

    reg.registerOption("shiftwidth", OptionType::Number, 4, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) ::SendMessage(hwnd, SCI_SETINDENT, std::get<int>(v), 0);
    }, "Number of spaces to use for each step of (auto)indent");

    reg.registerOption("wrap", OptionType::Bool, false, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) ::SendMessage(hwnd, SCI_SETWRAPMODE, std::get<bool>(v) ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
    }, "Wrap long lines");

    reg.registerOption("cursorline", OptionType::Bool, false, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) ::SendMessage(hwnd, SCI_SETCARETSTICKY, std::get<bool>(v) ? 1 : 0, 0);
        // Note: CaretLine is typically configured in N++ settings directly, but we can try to toggle Scintilla's background caret line.
        if (hwnd) ::SendMessage(hwnd, SCI_SETCARETLINEVISIBLE, std::get<bool>(v) ? 1 : 0, 0);
    }, "Highlight the text line of the cursor");

    reg.registerOption("list", OptionType::Bool, false, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) ::SendMessage(hwnd, SCI_SETVIEWWS, std::get<bool>(v) ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE, 0);
    }, "Show whitespace characters");

    reg.registerOption("scrolloff", OptionType::Number, 0, [](const OptionValue& v) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) {
            int lines = std::get<int>(v);
            ::SendMessage(hwnd, SCI_SETYCARETPOLICY, CARET_SLOP | CARET_EVEN, lines);
        }
    }, "Minimal number of screen lines to keep above and below the cursor");

    reg.registerOption("keylayout", OptionType::Bool, false, [](const OptionValue& v) {
        g_config.enableKeyboardLayoutSwitching = std::get<bool>(v);
    }, "Automatically switch keyboard layout between English (Normal) and last used (Insert)");

    reg.registerOption("normallayout", OptionType::String, std::string("en-US"), [](const OptionValue& v) {
        g_config.normallayout = std::get<std::string>(v);
    }, "Keyboard layout for Normal mode");

    reg.registerOption("insertlayout", OptionType::String, std::string("system"), [](const OptionValue& v) {
        g_config.insertlayout = std::get<std::string>(v);
    }, "Keyboard layout for Insert mode");

    reg.registerOption("langmap", OptionType::String, std::string(""), [](const OptionValue& v) {
        Utils::parseLangmap(std::get<std::string>(v));
    }, "Translate characters in Normal/Visual modes");

    reg.registerOption("textwidth", OptionType::Number, 0, [](const OptionValue& v) {
        int width = 0;
        if (std::holds_alternative<int>(v)) {
            width = std::get<int>(v);
        } else if (std::holds_alternative<std::string>(v)) {
            try {
                width = std::stoi(std::get<std::string>(v));
            } catch (...) {
                width = 0;
            }
        } else if (std::holds_alternative<bool>(v)) {
            width = std::get<bool>(v) ? 80 : 0;
        }

        HWND mainWnd = nppData._scintillaMainHandle;
        HWND secondWnd = nppData._scintillaSecondHandle;

        if (width > 0) {
            if (mainWnd) {
                ::SendMessage(mainWnd, SCI_SETEDGECOLUMN, width, 0);
                ::SendMessage(mainWnd, SCI_SETEDGEMODE, EDGE_LINE, 0);
            }
            if (secondWnd) {
                ::SendMessage(secondWnd, SCI_SETEDGECOLUMN, width, 0);
                ::SendMessage(secondWnd, SCI_SETEDGEMODE, EDGE_LINE, 0);
            }
        } else {
            if (mainWnd) ::SendMessage(mainWnd, SCI_SETEDGEMODE, EDGE_NONE, 0);
            if (secondWnd) ::SendMessage(secondWnd, SCI_SETEDGEMODE, EDGE_NONE, 0);
        }
    }, "Maximum width of text that is being inserted");
}

void loadConfig() {
    ConfigManager::getInstance().loadConfig();
    ConfigManager::getInstance().ensureDefaultFiles();
    
    g_config.vimEnabled = ConfigManager::getInstance().isEnabled();
    
    ::SendMessage(nppData._nppHandle, NPPM_HIDESTATUSBAR, 0, ConfigManager::getInstance().isShowStatusBar() ? FALSE : TRUE);

    // Load RC file
    RcParser::getInstance().parseFile(ConfigManager::getInstance().getRcPath());
}

void saveConfig() {
    ConfigManager::getInstance().setEnabled(g_config.vimEnabled);
    ConfigManager::getInstance().saveConfig();
    ::SendMessage(nppData._nppHandle, NPPM_HIDESTATUSBAR, 0, ConfigManager::getInstance().isShowStatusBar() ? FALSE : TRUE);
}

void InitDialogResources() {
    if (!g_hBrushBg) {
        g_hBrushBg = CreateSolidBrush(COLOR_BG);
        g_hBrushAccent = CreateSolidBrush(COLOR_ACCENT);
        g_hBrushDonate = CreateSolidBrush(COLOR_DONATE);

        LOGFONT lf = {};
        lf.lfHeight = -22;
        lf.lfWeight = FW_SEMIBOLD;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        g_hFontTitle = CreateFontIndirect(&lf);

        lf.lfHeight = -16;
        lf.lfWeight = FW_NORMAL;
        g_hFontNormal = CreateFontIndirect(&lf);

        lf.lfHeight = -16;
        lf.lfWeight = FW_SEMIBOLD;
        g_hFontButton = CreateFontIndirect(&lf);
    }
}

void CleanupDialogResources() {
    if (g_hBrushBg) DeleteObject(g_hBrushBg);
    if (g_hBrushAccent) DeleteObject(g_hBrushAccent);
    if (g_hBrushDonate) DeleteObject(g_hBrushDonate);
    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontButton) DeleteObject(g_hFontButton);
}

void DrawModernButton(LPDRAWITEMSTRUCT pDIS, COLORREF bgColor, COLORREF textColor) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;

    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rc.left, rc.top, NULL);
    LineTo(hdc, rc.right - 1, rc.top);
    LineTo(hdc, rc.right - 1, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.bottom - 1);
    LineTo(hdc, rc.left, rc.top);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    TCHAR text[128];
    GetWindowText(pDIS->hwndItem, text, 128);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    SelectObject(hdc, g_hFontButton);
    DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

INT_PTR CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_INITDIALOG: {
        InitDialogResources();
        SendDlgItemMessage(hwnd, IDC_TITLE, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
        SendDlgItemMessage(hwnd, IDC_DESC1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendDlgItemMessage(hwnd, IDC_DESC2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        WCHAR path[MAX_PATH];
        GetModuleFileNameW((HMODULE)g_hInstance, path, MAX_PATH);
        DWORD handle = 0;
        DWORD size = GetFileVersionInfoSizeW(path, &handle);
        if (size) {
            std::vector<BYTE> data(size);
            if (GetFileVersionInfoW(path, 0, size, data.data())) {
                VS_FIXEDFILEINFO* info = nullptr;
                UINT len = 0;
                if (VerQueryValueW(data.data(), L"\\", (LPVOID*)&info, &len)) {
                    WCHAR version[64];
                    wsprintfW(version, L"Version: %d.%d.%d.%d",
                        HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                        HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
                    SetDlgItemTextW(hwnd, IDC_VERSION, version);
                }
            }
        }
        return TRUE;
    }
    case WM_CTLCOLORDLG: return (INT_PTR)g_hBrushBg;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == GetDlgItem(hwnd, IDC_TITLE)) SetTextColor(hdc, COLOR_ACCENT);
        else SetTextColor(hdc, COLOR_TEXT_LIGHT);
        SetBkColor(hdc, COLOR_BG);
        return (INT_PTR)g_hBrushBg;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == IDC_BTN_GITHUB) DrawModernButton(pDIS, RGB(240, 240, 240), COLOR_TEXT);
        else if (pDIS->CtlID == IDC_BTN_DONATE) DrawModernButton(pDIS, COLOR_DONATE, RGB(255, 255, 255));
        else if (pDIS->CtlID == IDOK) DrawModernButton(pDIS, RGB(240, 240, 240), COLOR_TEXT);
        return TRUE;
    }
    case WM_CLOSE: EndDialog(hwnd, IDOK); return TRUE;
    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case IDC_BTN_GITHUB: ShellExecute(NULL, L"open", L"https://github.com/h-jangra/nppvim", NULL, NULL, SW_SHOWNORMAL); return TRUE;
        case IDC_BTN_DONATE: ShellExecute(NULL, L"open", L"https://paypal.me/h8imansh8u", NULL, NULL, SW_SHOWNORMAL); return TRUE;
        case IDOK: EndDialog(hwnd, IDOK); return TRUE;
        }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK ConfigDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        InitDialogResources();
        SendDlgItemMessage(hwnd, IDC_ESCAPE_KEY, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        SendDlgItemMessage(hwnd, IDC_TIMEOUT, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        HWND hCombo = GetDlgItem(hwnd, IDC_ESCAPE_KEY);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("Escape key only"));
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("jj (double tap j)"));
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("jk"));
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("kj"));
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("Custom"));
        int selIndex = 0;
        if (g_config.escapeKey == "jj") selIndex = 1;
        else if (g_config.escapeKey == "jk") selIndex = 2;
        else if (g_config.escapeKey == "kj") selIndex = 3;
        else if (g_config.escapeKey == "custom") selIndex = 4;
        SendMessage(hCombo, CB_SETCURSEL, selIndex, 0);
        SetDlgItemInt(hwnd, IDC_TIMEOUT, g_config.escapeTimeout, FALSE);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_D, g_config.overrideCtrlD ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_U, g_config.overrideCtrlU ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_R, g_config.overrideCtrlR ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_F, g_config.overrideCtrlF ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_B, g_config.overrideCtrlB ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_O, g_config.overrideCtrlO ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_I, g_config.overrideCtrlI ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_V, g_config.overrideCtrlV ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_A, g_config.overrideCtrlA ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_X, g_config.overrideCtrlX ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_D_CLIPBOARD, g_config.dStoreClipboard ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_C_CLIPBOARD, g_config.cStoreClipboard ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_X_CLIPBOARD, g_config.xStoreClipboard ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_KB_LAYOUT, g_config.enableKeyboardLayoutSwitching ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextA(hwnd, IDC_NORMAL_LAYOUT, g_config.normallayout.c_str());
        SetDlgItemTextA(hwnd, IDC_INSERT_LAYOUT, g_config.insertlayout.c_str());
        SetDlgItemTextA(hwnd, IDC_CUSTOM_ESCAPE, g_config.customEscape.c_str());
        EnableWindow(GetDlgItem(hwnd, IDC_CUSTOM_ESCAPE), selIndex == 4);
        return TRUE;
    }
    case WM_CTLCOLORDLG: return (INT_PTR)g_hBrushBg;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_BG);
        return (INT_PTR)g_hBrushBg;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == IDOK) DrawModernButton(pDIS, COLOR_ACCENT, RGB(255, 255, 255));
        else if (pDIS->CtlID == IDCANCEL || pDIS->CtlID == IDC_RESET_BUTTON) DrawModernButton(pDIS, RGB(240, 240, 240), COLOR_TEXT);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_RESET_BUTTON:
            SendMessage(GetDlgItem(hwnd, IDC_ESCAPE_KEY), CB_SETCURSEL, 0, 0);
            SetDlgItemInt(hwnd, IDC_TIMEOUT, 300, FALSE);
            SetDlgItemTextA(hwnd, IDC_CUSTOM_ESCAPE, "");
            EnableWindow(GetDlgItem(hwnd, IDC_CUSTOM_ESCAPE), FALSE);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_D, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_U, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_R, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_F, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_B, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_O, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_I, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_V, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_A, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_CTRL_X, BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_D_CLIPBOARD, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_C_CLIPBOARD, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_X_CLIPBOARD, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_KB_LAYOUT, BST_UNCHECKED);
            SetDlgItemTextA(hwnd, IDC_NORMAL_LAYOUT, "en-US");
            SetDlgItemTextA(hwnd, IDC_INSERT_LAYOUT, "system");
            break;
        case IDC_ESCAPE_KEY: if (HIWORD(wParam) == CBN_SELCHANGE) EnableWindow(GetDlgItem(hwnd, IDC_CUSTOM_ESCAPE), SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0) == 4); break;
        case IDOK: {
            int sel = SendMessage(GetDlgItem(hwnd, IDC_ESCAPE_KEY), CB_GETCURSEL, 0, 0);
            switch (sel) {
                case 1: g_config.escapeKey = "jj"; break;
                case 2: g_config.escapeKey = "jk"; break;
                case 3: g_config.escapeKey = "kj"; break;
                case 4: {
                    char buf[128]; GetDlgItemTextA(hwnd, IDC_CUSTOM_ESCAPE, buf, 128);
                    g_config.customEscape = buf;
                    if (g_config.customEscape.empty()) return TRUE;
                    g_config.escapeKey = "custom";
                    break;
                }
                default: g_config.escapeKey = "esc"; break;
            }
            g_config.escapeTimeout = GetDlgItemInt(hwnd, IDC_TIMEOUT, NULL, FALSE);
            g_config.overrideCtrlD = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_D) == BST_CHECKED);
            g_config.overrideCtrlU = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_U) == BST_CHECKED);
            g_config.overrideCtrlR = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_R) == BST_CHECKED);
            g_config.overrideCtrlF = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_F) == BST_CHECKED);
            g_config.overrideCtrlB = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_B) == BST_CHECKED);
            g_config.overrideCtrlO = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_O) == BST_CHECKED);
            g_config.overrideCtrlI = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_I) == BST_CHECKED);
            g_config.overrideCtrlV = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_V) == BST_CHECKED);
            g_config.overrideCtrlA = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_A) == BST_CHECKED);
            g_config.overrideCtrlX = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_X) == BST_CHECKED);
            g_config.dStoreClipboard = (IsDlgButtonChecked(hwnd, IDC_CHECK_D_CLIPBOARD) == BST_CHECKED);
            g_config.cStoreClipboard = (IsDlgButtonChecked(hwnd, IDC_CHECK_C_CLIPBOARD) == BST_CHECKED);
            g_config.xStoreClipboard = (IsDlgButtonChecked(hwnd, IDC_CHECK_X_CLIPBOARD) == BST_CHECKED);
            g_config.enableKeyboardLayoutSwitching = (IsDlgButtonChecked(hwnd, IDC_CHECK_KB_LAYOUT) == BST_CHECKED);
            
            char normBuf[128], insBuf[128];
            GetDlgItemTextA(hwnd, IDC_NORMAL_LAYOUT, normBuf, 128);
            GetDlgItemTextA(hwnd, IDC_INSERT_LAYOUT, insBuf, 128);
            g_config.normallayout = normBuf;
            g_config.insertlayout = insBuf;

            saveConfig();
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL: EndDialog(hwnd, IDCANCEL); return TRUE;
        }
        break;
    }
    return FALSE;
}

void showConfigDialog() { DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_CONFIG), nppData._nppHandle, ConfigDialogProc); }

bool checkEscapeSequence(char c) {
    DWORD currentTime = GetTickCount64();
    if (g_firstKey != 0 && currentTime - g_firstKeyTime > g_config.escapeTimeout) g_firstKey = 0;
    if (g_config.escapeKey == "custom" && !g_config.customEscape.empty()) {
        if (g_config.customEscape.find("ctrl+") == 0) return false;
        if (g_config.customEscape.length() == 2) {
            if (g_firstKey == 0) { if (c == g_config.customEscape[0]) { g_firstKey = c; g_firstKeyTime = currentTime; return false; } }
            else if (g_firstKey == g_config.customEscape[0] && c == g_config.customEscape[1]) { g_firstKey = 0; return true; }
        }
    }
    if (g_firstKey == 0) {
        if ((g_config.escapeKey == "jj" && c == 'j') || (g_config.escapeKey == "jk" && c == 'j') || (g_config.escapeKey == "kj" && c == 'k')) {
            g_firstKey = c; g_firstKeyTime = currentTime; return false;
        }
        return false;
    }
    if ((g_config.escapeKey == "jj" && g_firstKey == 'j' && c == 'j') || (g_config.escapeKey == "jk" && g_firstKey == 'j' && c == 'k') || (g_config.escapeKey == "kj" && g_firstKey == 'k' && c == 'j')) {
        g_firstKey = 0; return true;
    }
    g_firstKey = 0; return false;
}

LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;
    if (!orig) return DefWindowProc(hwnd, msg, wParam, lParam);
    if (msg == WM_CLOSE || msg == WM_DESTROY || msg == WM_NCDESTROY) return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    if (!state.vimEnabled) return CallWindowProc(orig, hwnd, msg, wParam, lParam);

    if (msg == WM_INPUTLANGCHANGE && g_config.enableKeyboardLayoutSwitching) {
        g_userLayout = (HKL)lParam;
        if (state.mode == INSERT) state.savedInsertLayout = g_userLayout;
    }

    if ((state.mode == NORMAL || state.mode == VISUAL) && msg == WM_KEYDOWN) {
        bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrlPressed) {
            if (wParam == 'Q') { if (state.mode == VISUAL && state.isBlockVisual) g_normalMode->enter(); else g_visualMode->enterBlock(hwnd); return 0; }
            if (wParam == 'D' && g_config.overrideCtrlD) { Motion::pageDown(hwnd); state.repeatCount = 0; return 0; }
            if (wParam == 'U' && g_config.overrideCtrlU) { Motion::pageUp(hwnd); state.repeatCount = 0; return 0; }
            if (wParam == 'R' && g_config.overrideCtrlR && state.mode == NORMAL) { ::SendMessage(hwnd, SCI_REDO, 0, 0); return 0; }
            if (wParam == 'F' && g_config.overrideCtrlF) { Motion::pageDown(hwnd); state.repeatCount = 0; return 0; }
            if (wParam == 'B' && g_config.overrideCtrlB) { Motion::pageUp(hwnd); state.repeatCount = 0; return 0; }
            if (wParam == 'O' && g_config.overrideCtrlO) { if (g_normalMode) g_normalMode->jumpBackward(hwnd); return 0; }
            if (wParam == 'I' && g_config.overrideCtrlI) { if (g_normalMode) g_normalMode->jumpForward(hwnd); return 0; }
            if (wParam == 'V' && g_config.overrideCtrlV) { if (state.mode == VISUAL && state.isBlockVisual) g_normalMode->enter(); else g_visualMode->enterBlock(hwnd); return 0; }
            if (wParam == 'A' && g_config.overrideCtrlA) { if (g_normalMode) g_normalMode->incrementNumber(hwnd, 1); return 0; }
            if (wParam == 'X' && g_config.overrideCtrlX) { if (g_normalMode) g_normalMode->decrementNumber(hwnd, 1); return 0; }
        }
    }

    if (state.commandMode) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) { g_commandMode->handleEnter(hwnd); return 0; }
            if (wParam == VK_ESCAPE) { Utils::clearSearchHighlights(hwnd); state.lastSearchMatchCount = -1; g_commandMode->exit(); return 0; }
            if (wParam == VK_BACK) { g_commandMode->handleBackspace(hwnd); return 0; }
        }
        if (msg == WM_CHAR) { 
            wchar_t wChar = (wchar_t)wParam;
            g_commandMode->handleKey(hwnd, wChar); 
            return 0; 
        }
        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }

    if (state.mode == INSERT) {
        if (msg == WM_CHAR) {
            wchar_t wChar = (wchar_t)wParam;
            if (state.recordingInsertMacro && wChar != VK_ESCAPE) {
                std::string utf8 = Utils::toUtf8(wChar);
                for (char ch : utf8) state.insertMacroBuffers.back().push_back(ch);
            }
            if ((int)wParam == VK_ESCAPE) { ::SendMessage(hwnd, SCI_SETOVERTYPE, false, 0); g_firstKey = 0; if (state.recordingInsertMacro) { state.insertMacroBuffers.back().push_back('\x1B'); state.recordingInsertMacro = false; } g_normalMode->enter(); return 0; }
            if (g_config.escapeKey != "esc" && checkEscapeSequence((char)wParam)) {
                if (state.recordingInsertMacro && !state.insertMacroBuffers.empty()) { state.insertMacroBuffers.back().push_back('\x1B'); state.recordingInsertMacro = false; }
                int pos = (int)::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
                if (pos >= 1) { ::SendMessage(hwnd, SCI_SETSEL, pos - 1, pos); ::SendMessage(hwnd, SCI_REPLACESEL, 0, (LPARAM)""); }
                ::SendMessage(hwnd, SCI_SETOVERTYPE, false, 0); g_normalMode->enter(); return 0;
            }
        }
        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }

    if (msg == WM_CHAR) {
        wchar_t wChar = (wchar_t)wParam;
        char c = Utils::applyLangmap(wChar);
        if (c == 0) return 0; // Consume unmapped non-ASCII in Normal/Visual mode to prevent text insertion

        if (c == 27) { g_firstKey = 0; g_normalMode->enter(); return 0; }
        if (state.mode == NORMAL) { g_normalMode->handleKey(hwnd, c); return 0; }
        else if (state.mode == VISUAL) { g_visualMode->handleKey(hwnd, c); return 0; }
    }
    return CallWindowProc(orig, hwnd, msg, wParam, lParam);
}

void installScintillaHookFor(HWND hwnd) {
    if (!hwnd || origProcMap.find(hwnd) != origProcMap.end()) return;
    WNDPROC prev = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(ScintillaHookProc));
    if (prev) origProcMap[hwnd] = prev;
}

void removeAllScintillaHooks() {
    for (auto& p : origProcMap) if (IsWindow(p.first)) SetWindowLongPtr(p.first, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(p.second));
    origProcMap.clear();
}

void ensureScintillaHooks() { installScintillaHookFor(nppData._scintillaMainHandle); installScintillaHookFor(nppData._scintillaSecondHandle); }

void updateCursorForCurrentMode() {
    HWND hwndEdit = Utils::getCurrentScintillaHandle();
    if (!hwndEdit || !IsWindow(hwndEdit)) return;
    if (state.vimEnabled) {
        if (state.mode == NORMAL || state.mode == VISUAL) ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
        else if (state.mode == INSERT) ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
    } else ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}

void toggleVimMode() {
    state.vimEnabled = !state.vimEnabled; g_config.vimEnabled = state.vimEnabled; saveConfig();
    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0);
    if (hMenu) ::CheckMenuItem(hMenu, funcItem[0]._cmdID, MF_BYCOMMAND | (state.vimEnabled ? MF_CHECKED : MF_UNCHECKED));
    if (state.vimEnabled) { ensureScintillaHooks(); g_normalMode->enter(); updateCursorForCurrentMode(); Utils::setStatus(TEXT("-- NORMAL --")); }
    else { Utils::setStatus(TEXT(" ")); removeAllScintillaHooks(); updateCursorForCurrentMode(); }
}

void about() { DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUT), nppData._nppHandle, AboutDlgProc); }

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID) {
    if (reasonForCall == DLL_PROCESS_ATTACH) g_hInstance = (HINSTANCE)hModule;
    else if (reasonForCall == DLL_PROCESS_DETACH) {
        removeNppHook();
        removeAllScintillaHooks(); CleanupDialogResources();
        if (g_normalMode) delete g_normalMode; if (g_visualMode) delete g_visualMode; if (g_commandMode) delete g_commandMode;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData; setNppData(notpadPlusData);
    installNppHook();
    initializeOptions();
    g_normalMode = new NormalMode(state); g_visualMode = new VisualMode(state); g_commandMode = new CommandMode(state);
    loadConfig();
    g_englishLayout = LoadKeyboardLayout(TEXT("00000409"), KLF_ACTIVATE); g_userLayout = GetKeyboardLayout(0);
    state.vimEnabled = g_config.vimEnabled; if (state.vimEnabled) { ensureScintillaHooks(); g_normalMode->enter(); updateCursorForCurrentMode(); }
}

extern "C" __declspec(dllexport) const TCHAR* getName() { return PLUGIN_NAME; }

void reloadConfiguration() { 
    if (g_normalMode) delete g_normalMode; 
    if (g_visualMode) delete g_visualMode; 
    if (g_commandMode) delete g_commandMode;
    
    MappingManager::getInstance().clearMappings();
    CommandMode::clearUserCommands();
    OptionRegistry::getInstance().resetToDefaults();

    g_normalMode = new NormalMode(state); 
    g_visualMode = new VisualMode(state); 
    g_commandMode = new CommandMode(state);

    loadConfig(); 
    Utils::setStatus(TEXT("Configuration reloaded")); 
}
void editRcFile() { ConfigManager::getInstance().editRc(); }
void editIniFile() { ConfigManager::getInstance().editIni(); }

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
    *nbF = nbFunc;
    lstrcpy(funcItem[0]._itemName, TEXT("Toggle Vim Mode")); funcItem[0]._pFunc = toggleVimMode; funcItem[0]._init2Check = g_config.vimEnabled;
    lstrcpy(funcItem[1]._itemName, TEXT("Configuration Dialog")); funcItem[1]._pFunc = showConfigDialog;
    lstrcpy(funcItem[2]._itemName, TEXT("Reload nppvim.rc")); funcItem[2]._pFunc = reloadConfiguration;
    lstrcpy(funcItem[3]._itemName, TEXT("--SEPARATOR--"));
    lstrcpy(funcItem[4]._itemName, TEXT("Edit nppvim.rc")); funcItem[4]._pFunc = editRcFile;
    lstrcpy(funcItem[5]._itemName, TEXT("Edit config.ini")); funcItem[5]._pFunc = editIniFile;
    lstrcpy(funcItem[6]._itemName, TEXT("--SEPARATOR--"));
    lstrcpy(funcItem[7]._itemName, TEXT("About")); funcItem[7]._pFunc = about;
    return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode) {
    if (!notifyCode) return;

    if ((notifyCode->nmhdr.code == NPPN_BUFFERACTIVATED || notifyCode->nmhdr.code == NPPN_READY) && state.vimEnabled) {
        ensureScintillaHooks(); 
        updateCursorForCurrentMode();
        // Force update immediately
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) updateRelativeLineNumbers(hwnd, true);
    }

    if (notifyCode->nmhdr.code == NPPN_WORDSTYLESUPDATED || notifyCode->nmhdr.code == NPPN_DARKMODECHANGED) {
        HWND hwnd = Utils::getCurrentScintillaHandle();
        if (hwnd) {
            ::SendMessage(hwnd, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
            updateRelativeLineNumbers(hwnd, true);
        }
    }

    if (notifyCode->nmhdr.code == NPPN_FILESAVED) {
        TCHAR savedPath[MAX_PATH];
        if (::SendMessage(nppData._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, notifyCode->nmhdr.idFrom, (LPARAM)savedPath) != 0) {
            std::string rcPath = ConfigManager::getInstance().getRcPath();
            std::string iniPath = ConfigManager::getInstance().getConfigPath();
#ifdef UNICODE
            std::string sSavedPath = Utils::toUtf8(savedPath);
#else
            std::string sSavedPath(savedPath);
#endif
            auto normalize = [](std::string p) {
                std::transform(p.begin(), p.end(), p.begin(), ::tolower);
                std::replace(p.begin(), p.end(), '/', '\\');
                return p;
            };
            std::string normSaved = normalize(sSavedPath);
            std::string normRc = normalize(rcPath);
            std::string normIni = normalize(iniPath);
            if (normSaved == normRc || normSaved == normIni) {
                reloadConfiguration();
            }
        }
    }

    if (notifyCode->nmhdr.code == SCN_UPDATEUI) {
        // Always update on selection/scroll/content change
        if (notifyCode->updated & (SC_UPDATE_SELECTION | SC_UPDATE_V_SCROLL | SC_UPDATE_CONTENT | SC_UPDATE_H_SCROLL)) {
            updateRelativeLineNumbers((HWND)notifyCode->nmhdr.hwndFrom);
        }
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return 0; }

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
#endif
