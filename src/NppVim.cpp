//NppVim.cpp
#include <windows.h>
#include <map>
#include <string>
#include <fstream>

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

HINSTANCE g_hInstance = nullptr;
NppData nppData;
VimState state;

NormalMode* g_normalMode = nullptr;
VisualMode* g_visualMode = nullptr;
CommandMode* g_commandMode = nullptr;

const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 4;
FuncItem funcItem[nbFunc];

static std::map<HWND, WNDPROC> origProcMap;

// Configuration
struct VimConfig {
    std::string escapeKey = "esc";
    int escapeTimeout = 300;
    bool overrideCtrlD = false;
    bool overrideCtrlU = false;
    bool overrideCtrlR = false;
};

VimConfig g_config;

// Forward declarations
LRESULT CALLBACK ScintillaHookProc(HWND, UINT, WPARAM, LPARAM);
void installScintillaHookFor(HWND hwnd);
void removeAllScintillaHooks();
void ensureScintillaHooks();
void toggleVimMode();
void showConfigDialog();
void about();
void loadConfig();
void saveConfig();

// State for sequence detection
static char g_firstKey = 0;
static DWORD g_firstKeyTime = 0;

void setNppData(NppData data) {
    Utils::nppData = data;
}

// Get plugin config directory
std::string getConfigPath() {
    TCHAR configDir[MAX_PATH];
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);

    std::string path;
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, configDir, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        path.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, configDir, -1, &path[0], len, NULL, NULL);
        path.pop_back();
    }
#else
    path = configDir;
#endif

    return path + "\\NppVim\\config.ini";
}

void loadConfig() {
    std::string configPath = getConfigPath();
    std::string configDir = configPath.substr(0, configPath.find_last_of('\\'));
    CreateDirectoryA(configDir.c_str(), NULL);

    // Set defaults
    g_config.escapeKey = "esc";
    g_config.escapeTimeout = 300;
    g_config.overrideCtrlD = false;
    g_config.overrideCtrlU = false;
    g_config.overrideCtrlR = false;

    std::ifstream file(configPath);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            start = key.find_first_not_of(" \t");
            end = key.find_last_not_of(" \t");
            if (start != std::string::npos) {
                key = key.substr(start, end - start + 1);
            }

            start = value.find_first_not_of(" \t");
            end = value.find_last_not_of(" \t");
            if (start != std::string::npos) {
                value = value.substr(start, end - start + 1);
            }

            if (key == "escape_key") {
                g_config.escapeKey = value;
            }
            else if (key == "escape_timeout") {
                try {
                    int timeout = std::stoi(value);
                    if (timeout >= 100 && timeout <= 1000) {
                        g_config.escapeTimeout = timeout;
                    }
                }
                catch (...) {}
            }
            else if (key == "override_ctrl_d") {
                g_config.overrideCtrlD = (value == "1" || value == "true");
            }
            else if (key == "override_ctrl_u") {
                g_config.overrideCtrlU = (value == "1" || value == "true");
            }
            else if (key == "override_ctrl_r") {
                g_config.overrideCtrlR = (value == "1" || value == "true");
            }
        }
    }
    file.close();
}

void saveConfig() {
    std::string configPath = getConfigPath();
    std::string configDir = configPath.substr(0, configPath.find_last_of('\\'));
    CreateDirectoryA(configDir.c_str(), NULL);

    std::ofstream file(configPath);
    if (file.is_open()) {
        file << "# NppVim Configuration File\n";
        file << "# Escape key options: esc, jj, jk, kj\n";
        file << "escape_key=" << g_config.escapeKey << "\n";
        file << "\n";
        file << "# Timeout for two-key escape sequences (100-1000ms)\n";
        file << "escape_timeout=" << g_config.escapeTimeout << "\n";
        file << "\n";
        file << "# Ctrl key overrides (0 or 1)\n";
        file << "override_ctrl_d=" << (g_config.overrideCtrlD ? "1" : "0") << "\n";
        file << "override_ctrl_u=" << (g_config.overrideCtrlU ? "1" : "0") << "\n";
        file << "override_ctrl_r=" << (g_config.overrideCtrlR ? "1" : "0") << "\n";
        file.close();
    }
}

// Configuration dialog procedure
INT_PTR CALLBACK ConfigDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        HWND hEscapeCombo = GetDlgItem(hwnd, IDC_ESCAPE_KEY);
        SendMessage(hEscapeCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("Escape key only"));
        SendMessage(hEscapeCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("jj (double tap j)"));
        SendMessage(hEscapeCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("jk"));
        SendMessage(hEscapeCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("kj"));

        int selIndex = 0;
        if (g_config.escapeKey == "jj") selIndex = 1;
        else if (g_config.escapeKey == "jk") selIndex = 2;
        else if (g_config.escapeKey == "kj") selIndex = 3;

        SendMessage(hEscapeCombo, CB_SETCURSEL, selIndex, 0);

        SetDlgItemInt(hwnd, IDC_TIMEOUT, g_config.escapeTimeout, FALSE);

        // Set checkboxes
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_D, g_config.overrideCtrlD ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_U, g_config.overrideCtrlU ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_CHECK_CTRL_R, g_config.overrideCtrlR ? BST_CHECKED : BST_UNCHECKED);

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            HWND hEscapeCombo = GetDlgItem(hwnd, IDC_ESCAPE_KEY);
            int sel = SendMessage(hEscapeCombo, CB_GETCURSEL, 0, 0);

            switch (sel) {
            case 1: g_config.escapeKey = "jj"; break;
            case 2: g_config.escapeKey = "jk"; break;
            case 3: g_config.escapeKey = "kj"; break;
            default: g_config.escapeKey = "esc"; break;
            }

            BOOL success;
            int timeout = GetDlgItemInt(hwnd, IDC_TIMEOUT, &success, FALSE);
            if (success && timeout >= 100 && timeout <= 1000) {
                g_config.escapeTimeout = timeout;
            }
            else {
                MessageBox(hwnd, TEXT("Timeout must be between 100 and 1000 milliseconds."),
                    TEXT("Invalid Input"), MB_OK | MB_ICONWARNING);
                return TRUE;
            }

            // Get checkbox states
            g_config.overrideCtrlD = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_D) == BST_CHECKED);
            g_config.overrideCtrlU = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_U) == BST_CHECKED);
            g_config.overrideCtrlR = (IsDlgButtonChecked(hwnd, IDC_CHECK_CTRL_R) == BST_CHECKED);

            saveConfig();
            EndDialog(hwnd, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void showConfigDialog() {
    DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_CONFIG), nppData._nppHandle, ConfigDialogProc);
}

bool checkEscapeSequence(char c) {
    DWORD currentTime = GetTickCount64();

    if (g_firstKey != 0 && currentTime - g_firstKeyTime > g_config.escapeTimeout) {
        g_firstKey = 0;
    }

    if (g_firstKey == 0) {
        if ((g_config.escapeKey == "jj" && c == 'j') ||
            (g_config.escapeKey == "jk" && c == 'j') ||
            (g_config.escapeKey == "kj" && c == 'k')) {
            g_firstKey = c;
            g_firstKeyTime = currentTime;
            return false;
        }
        return false;
    }

    if ((g_config.escapeKey == "jj" && g_firstKey == 'j' && c == 'j') ||
        (g_config.escapeKey == "jk" && g_firstKey == 'j' && c == 'k') ||
        (g_config.escapeKey == "kj" && g_firstKey == 'k' && c == 'j')) {
        g_firstKey = 0;
        return true;
    }

    g_firstKey = 0;
    return false;
}

LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;

    if (!state.vimEnabled) {
        if (orig) {
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }
        else {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }

    if (!orig) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    HWND hwndEdit = hwnd;

    // Handle Ctrl key overrides in NORMAL and VISUAL modes
    if ((state.mode == NORMAL || state.mode == VISUAL) && msg == WM_KEYDOWN) {
        bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (ctrlPressed) {
            if (wParam == 'D' && g_config.overrideCtrlD) {
                // Ctrl+D: Page down
                Motion::pageDown(hwndEdit);
                if (state.mode == NORMAL) {
                    state.recordLastOp(OP_MOTION, state.repeatCount > 0 ? state.repeatCount : 1, 4); // 4 represents Ctrl+D
                }
                state.repeatCount = 0;
                return 0;
            }
            else if (wParam == 'U' && g_config.overrideCtrlU) {
                // Ctrl+U: Page up
                Motion::pageUp(hwndEdit);
                if (state.mode == NORMAL) {
                    state.recordLastOp(OP_MOTION, state.repeatCount > 0 ? state.repeatCount : 1, 21); // 21 represents Ctrl+U
                }
                state.repeatCount = 0;
                return 0;
            }
            else if (wParam == 'R' && g_config.overrideCtrlR && state.mode == NORMAL) {
                // Ctrl+R: Redo in normal mode
                ::SendMessage(hwndEdit, SCI_REDO, 0, 0);
                state.recordLastOp(OP_MOTION, 1, 'R');
                return 0;
            }
        }
    }

    // Handle command mode
    if (state.commandMode) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                g_commandMode->handleEnter(hwndEdit);
                return 0;
            }
            else if (wParam == VK_ESCAPE) {
                Utils::clearSearchHighlights(hwndEdit);
                state.lastSearchMatchCount = -1;
                g_commandMode->exit();
                return 0;
            }
            else if (wParam == VK_BACK) {
                g_commandMode->handleBackspace(hwndEdit);
                return 0;
            }
        }

        if (msg == WM_CHAR) {
            char c = (char)wParam;
            if (c >= 32 && c != 10 && c != 13) {
                g_commandMode->handleKey(hwndEdit, c);
            }
            return 0;
        }
        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }

    // Handle INSERT mode
    if (state.mode == INSERT) {
        if (msg == WM_CHAR) {
            char c = (char)wParam;

            if ((int)wParam == VK_ESCAPE) {
                ::SendMessage(hwndEdit, SCI_SETOVERTYPE, false, 0);
                g_firstKey = 0;
                g_normalMode->enter();
                return 0;
            }

            if (g_config.escapeKey != "esc") {
                bool isEscape = checkEscapeSequence(c);
                if (isEscape) {
                    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                    if (pos >= 2) {
                        ::SendMessage(hwndEdit, SCI_SETSEL, pos - 2, pos);
                        ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)"");
                    }
                    else if (pos == 1) {
                        ::SendMessage(hwndEdit, SCI_SETSEL, 0, 1);
                        ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)"");
                    }
                    ::SendMessage(hwndEdit, SCI_SETOVERTYPE, false, 0);
                    g_normalMode->enter();
                    return 0;
                }
            }
        }

        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }

    // Handle NORMAL and VISUAL modes
    if (msg == WM_CHAR) {
        char c = (char)wParam;

        if ((int)wParam == VK_ESCAPE) {
            g_firstKey = 0;
            g_normalMode->enter();
            return 0;
        }

        if (state.replacePending && state.mode == NORMAL) {
            int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
            if (pos < docLen) {
                char currentChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
                if (currentChar != '\r' && currentChar != '\n') {
                    ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos + 1);
                    std::string repl(1, c);
                    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)repl.c_str());
                    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos + 1, 0);
                }
            }
            state.replacePending = false;
            state.repeatCount = 0;
            return 0;
        }

        if (state.mode == NORMAL) {
            g_normalMode->handleKey(hwndEdit, c);
            return 0;
        }
        else if (state.mode == VISUAL) {
            g_visualMode->handleKey(hwndEdit, c);
            return 0;
        }
    }

    if (msg == WM_KEYDOWN) {
        switch (wParam) {
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_INSERT:
        case VK_DELETE:
            g_firstKey = 0;
            break;
        }

        if (wParam == VK_SHIFT || wParam == VK_CONTROL || wParam == VK_MENU) {
            g_firstKey = 0;
        }
    }

    return CallWindowProc(orig, hwnd, msg, wParam, lParam);
}

void installScintillaHookFor(HWND hwnd) {
    if (!hwnd || origProcMap.find(hwnd) != origProcMap.end()) return;

    WNDPROC prev = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)ScintillaHookProc);
    if (prev) {
        origProcMap[hwnd] = prev;
    }
}

void removeAllScintillaHooks() {
    for (auto& p : origProcMap) {
        if (IsWindow(p.first)) {
            SetWindowLongPtr(p.first, GWLP_WNDPROC, (LONG_PTR)p.second);
        }
    }
    origProcMap.clear();
}

void ensureScintillaHooks() {
    installScintillaHookFor(nppData._scintillaMainHandle);
    installScintillaHookFor(nppData._scintillaSecondHandle);
}

void updateCursorForCurrentMode() {
    HWND hwndEdit = Utils::getCurrentScintillaHandle();
    if (!hwndEdit || !IsWindow(hwndEdit)) return;

    if (state.vimEnabled) {
        if (state.mode == NORMAL || state.mode == VISUAL) {
            ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
        }
        else if (state.mode == INSERT) {
            ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
        }
    }
    else {
        ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
    }
}

void toggleVimMode() {
    state.vimEnabled = !state.vimEnabled;

    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0);
    if (hMenu) {
        ::CheckMenuItem(hMenu, funcItem[0]._cmdID,
            MF_BYCOMMAND | (state.vimEnabled ? MF_CHECKED : MF_UNCHECKED));
    }

    if (state.vimEnabled) {
        ensureScintillaHooks();
        g_normalMode->enter();
        updateCursorForCurrentMode();
        Utils::setStatus(TEXT("-- NORMAL --"));
    }
    else {
        Utils::setStatus(TEXT(" "));

        HWND editors[] = {
            nppData._scintillaMainHandle,
            nppData._scintillaSecondHandle,
            Utils::getCurrentScintillaHandle()
        };

        for (HWND hwnd : editors) {
            if (hwnd && IsWindow(hwnd)) {
                ::SendMessage(hwnd, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
                ::SendMessage(hwnd, SCI_SETOVERTYPE, false, 0);

                int pos = (int)::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
                ::SendMessage(hwnd, SCI_SETSEL, pos, pos);

                ::InvalidateRect(hwnd, NULL, TRUE);
                ::UpdateWindow(hwnd);
            }
        }

        removeAllScintillaHooks();

        SetTimer(nppData._nppHandle, 1, 100, [](HWND, UINT, UINT_PTR id, DWORD) {
            HWND hwndEdit = Utils::getCurrentScintillaHandle();
            if (hwndEdit && IsWindow(hwndEdit)) {
                ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
            }
            KillTimer(nppData._nppHandle, id);
            });
    }
}

void about() {
    ::MessageBox(nppData._nppHandle,
        TEXT("NppVim - A Vim emulation plugin for Notepad++\n\n")
        TEXT("Features:\n")
        TEXT("• Normal, Insert, Visual, and Command modes\n")
        TEXT("• Customizable escape key sequences (Esc, jj, jk, kj)\n")
        TEXT("• Common Vim motions and commands\n")
        TEXT("• Search and navigation\n")
        TEXT("• Ctrl+D, Ctrl+U, Ctrl+R support"),
        TEXT("About NppVim"), MB_OK | MB_ICONINFORMATION);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/) {
    if (reasonForCall == DLL_PROCESS_ATTACH) {
        g_hInstance = (HINSTANCE)hModule;
    }
    else if (reasonForCall == DLL_PROCESS_DETACH) {
        removeAllScintillaHooks();

        if (g_normalMode) delete g_normalMode;
        if (g_visualMode) delete g_visualMode;
        if (g_commandMode) delete g_commandMode;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
    setNppData(notpadPlusData);

    loadConfig();

    g_normalMode = new NormalMode(state);
    g_visualMode = new VisualMode(state);
    g_commandMode = new CommandMode(state);

    state.vimEnabled = true;
    ensureScintillaHooks();
    g_normalMode->enter();
    updateCursorForCurrentMode();
}

extern "C" __declspec(dllexport) const TCHAR* getName() {
    return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
    *nbF = nbFunc;

    lstrcpy(funcItem[0]._itemName, TEXT("Toggle Vim Mode"));
    funcItem[0]._pFunc = toggleVimMode;
    funcItem[0]._pShKey = NULL;
    funcItem[0]._init2Check = true;

    lstrcpy(funcItem[1]._itemName, TEXT("Configuration"));
    funcItem[1]._pFunc = showConfigDialog;
    funcItem[1]._pShKey = NULL;
    funcItem[1]._init2Check = false;

    lstrcpy(funcItem[2]._itemName, TEXT("--SEPARATOR--"));
    funcItem[2]._pFunc = NULL;
    funcItem[2]._pShKey = NULL;
    funcItem[2]._init2Check = false;

    lstrcpy(funcItem[3]._itemName, TEXT("About"));
    funcItem[3]._pFunc = about;
    funcItem[3]._pShKey = NULL;
    funcItem[3]._init2Check = false;

    return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode) {
    if (!notifyCode) return;

    switch (notifyCode->nmhdr.code) {
    case NPPN_BUFFERACTIVATED:
    case NPPN_READY:
        if (state.vimEnabled) {
            ensureScintillaHooks();
            updateCursorForCurrentMode();
        }
        break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam) {
    return 0;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode() {
    return TRUE;
}
#endif