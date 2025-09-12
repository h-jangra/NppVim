// NppVim.cpp
#include "PluginInterface.h"
#include "Scintilla.h"
#include "Notepad_plus_msgs.h"
#include <windows.h>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>

#define NOMINMAX

// ---------------------------
// Globals
// ---------------------------
HINSTANCE g_hInstance = nullptr;
NppData nppData; // define here to avoid linker issues
const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 1;
FuncItem funcItem[nbFunc];

enum VimMode { NORMAL, INSERT, VISUAL };
static VimMode currentMode = NORMAL;
static bool isLineVisual = false;
static int visualAnchor = -1;

static int repeatCount = 0;
static char opPending = 0; // 'd' or 'y' when waiting for second key
static std::map<HWND, WNDPROC> origProcMap;

// ---------------------------
// Helpers
// ---------------------------
HWND getCurrentScintillaHandle()
{
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void about() {
    ::MessageBox(nppData._nppHandle, TEXT("A Notepad++ plugin that adds Vim-style editing and key bindings."), TEXT("Converter Plugin"), MB_OK);
}

void setStatus(const TCHAR* msg)
{
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)msg);
}

void enterNormalMode()
{
    HWND hwndEdit = getCurrentScintillaHandle();

    currentMode = NORMAL;
    isLineVisual = false;
    visualAnchor = -1;
    repeatCount = 0;
    opPending = 0;
    setStatus(TEXT("-- NORMAL --"));

    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
}

void enterInsertMode()
{
    HWND hwndEdit = getCurrentScintillaHandle();

    currentMode = INSERT;
    isLineVisual = false;
    visualAnchor = -1;
    repeatCount = 0;
    opPending = 0;
    setStatus(TEXT("-- INSERT --"));

    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}


void enterVisualCharMode(HWND hwndEdit)
{
    currentMode = VISUAL;
    isLineVisual = false;
    visualAnchor = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    // selection initially empty; anchor is stored
    setStatus(TEXT("-- VISUAL --"));
}

void enterVisualLineMode(HWND hwndEdit)
{
    currentMode = VISUAL;
    isLineVisual = true;
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
    visualAnchor = start;
    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    setStatus(TEXT("-- VISUAL LINE --"));
}

void setVisualSelection(HWND hwndEdit)
{
    if (visualAnchor < 0) return;
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    int a, b;

    if (isLineVisual)
    {
        // Compute line-wise selection
        int line1 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, visualAnchor, 0);
        int line2 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);

        int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, (std::min)(line1, line2), 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, (std::max)(line1, line2), 0);

        a = start;
        b = end;
    }
    else
    {
        // Char-wise selection
        a = (std::min)(visualAnchor, caret);
        b = (std::max)(visualAnchor, caret);
    }

    ::SendMessage(hwndEdit, SCI_SETSEL, a, b);
}


// delete current line (caret stays roughly at same line)
void DeleteLineOnce(HWND hwndEdit)
{
    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
    ::SendMessage(hwndEdit, SCI_LINEDELETE, 0, 0);
}

// yank (copy) single line into clipboard
void YankLineOnce(HWND hwndEdit)
{
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
}

// apply a motion 'count' times (helpers)
void DoMotion_char_left(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARLEFT, 0, 0); }
void DoMotion_char_right(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARRIGHT, 0, 0); }
void DoMotion_line_down(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0); }
void DoMotion_line_up(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0); }
void DoMotion_word_right(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0); }
void DoMotion_word_left(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0); }

// Scintilla subclass: intercept keys
LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // find original for this hwnd
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;

    // We handle WM_CHAR (character input) — easier for letters & digits
    if (msg == WM_CHAR)
    {
        HWND hwndEdit = hwnd; // the Scintilla control

        // ESC (27) always to Normal
        if ((int)wParam == VK_ESCAPE)
        {
            enterNormalMode();
            return 0; // block further processing
        }

        // If we are in INSERT mode, allow char to pass through
        if (currentMode == INSERT)
        {
            // reset transient state
            repeatCount = 0;
            opPending = 0;
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }

        // NORMAL or VISUAL mode: interpret keys as commands, block default typing
        char c = (char)wParam;

        // digits -> repeat count (accumulate)
        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            // if an operator is pending, digits probably apply to operator count (we keep it simple)
            int digit = c - '0';
            // treat leading zero properly: if repeatCount was 0 and digit==0, that's '0' command in vim (move to column 0), but for simplicity keep accumulation
            repeatCount = repeatCount * 10 + digit;
            return 0; // consumed
        }

        // compute effective count
        int count = (repeatCount > 0) ? repeatCount : 1;
        repeatCount = 0; // consume the count

        // If operator pending and the same operator repeated like 'dd' or 'yy', handle; else set opPending
        if (c == 'd')
        {
            if (opPending == 'd')
            {
                // perform dd (delete lines)
                for (int i = 0; i < count; ++i) DeleteLineOnce(hwndEdit);
                opPending = 0;
                return 0;
            }
            else
            {
                opPending = 'd';
                // wait for next char (e.g., second 'd')
                return 0;
            }
        }
        else if (c == 'y')
        {
            if (opPending == 'y')
            {
                for (int i = 0; i < count; ++i) YankLineOnce(hwndEdit);
                opPending = 0;
                return 0;
            }
            else
            {
                opPending = 'y';
                return 0;
            }
        }
        else
        {
            // any other key clears operator pending
            opPending = 0;
        }

        // handle motions & commands
        switch (c)
        {
        case 'h':
            DoMotion_char_left(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        case 'l': 
            DoMotion_char_right(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        case 'j':
            DoMotion_line_down(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        case 'k':
            DoMotion_line_up(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        case 'w':
            DoMotion_word_right(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        case 'b':
            DoMotion_word_left(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        case 'i':
            enterInsertMode();
            break;
        case 'v':
            // toggle charwise visual
            if (currentMode == VISUAL && !isLineVisual) {
                // exit visual
                enterNormalMode();
            }
            else {
                enterVisualCharMode(hwndEdit);
            }
            break;
        case 'V':
            // toggle linewise visual
            if (currentMode == VISUAL && isLineVisual) {
                enterNormalMode();
            }
            else {
                enterVisualLineMode(hwndEdit);
            }
            break;
        case 'p':
            ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
            break;

        case '0':
        {
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
            int pos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
            ::SendMessage(hwndEdit, SCI_GOTOPOS, pos, 0);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        }
        case '$': // move to line end
        {
            for (int i = 0; i < count; i++)
            {
                int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
                int pos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
                ::SendMessage(hwndEdit, SCI_GOTOPOS, pos, 0);
            }
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
        }
        default:
            // unknown command — ignore (and block insertion)
            break;
        }

        // After handling, ensure visual selection updated if needed
        if (currentMode == VISUAL && !isLineVisual)
        {
            setVisualSelection(hwndEdit);
        }

        return 0; // we handled/blocked the char
    }

    // For other messages, call original handler
    return CallWindowProc(orig, hwnd, msg, wParam, lParam);
}

// Install / remove hooks
void InstallScintillaHookFor(HWND hwnd)
{
    if (!hwnd) return;
    // if already hooked this exact hwnd, skip
    if (origProcMap.find(hwnd) != origProcMap.end()) return;
    WNDPROC prev = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)ScintillaHookProc);
    if (prev)
    {
        origProcMap[hwnd] = prev;
    }
}

void RemoveAllScintillaHooks()
{
    for (auto& p : origProcMap)
    {
        if (IsWindow(p.first))
        {
            SetWindowLongPtr(p.first, GWLP_WNDPROC, (LONG_PTR)p.second);
        }
    }
    origProcMap.clear();
}

void EnsureScintillaHooks()
{
    HWND main = nppData._scintillaMainHandle;
    HWND second = nppData._scintillaSecondHandle;
    InstallScintillaHookFor(main);
    InstallScintillaHookFor(second);
}

// Plugin callbacks / exports
void ToggleVimMode()
{
    HWND hwndEdit = getCurrentScintillaHandle();
    if (currentMode == NORMAL) enterInsertMode();
    else enterNormalMode();
    // update selection/caret style could be added
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/)
{
    if (reasonForCall == DLL_PROCESS_ATTACH) {
        g_hInstance = (HINSTANCE)hModule;
    }
    else if (reasonForCall == DLL_PROCESS_DETACH) {
        RemoveAllScintillaHooks();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
    nppData = notpadPlusData;
    enterNormalMode();
    EnsureScintillaHooks();
}

extern "C" __declspec(dllexport) const TCHAR* getName()
{
    return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF)
{
    *nbF = nbFunc;

    // Fill one menu item: Toggle Vim Mode
    lstrcpy(funcItem[0]._itemName, TEXT("Toggle Vim Mode"));
    funcItem[0]._pFunc = ToggleVimMode;
    funcItem[0]._pShKey = NULL;
    funcItem[0]._init2Check = false;

    lstrcpy(funcItem[1]._itemName, TEXT("--SEPARATOR--"));
    funcItem[1]._pFunc = NULL;
    funcItem[1]._pShKey = NULL;
    funcItem[1]._init2Check = false;

    lstrcpy(funcItem[2]._itemName, TEXT("About"));
    funcItem[2]._pFunc = about;
    funcItem[2]._pShKey = NULL;
    funcItem[2]._init2Check = false;

    return funcItem;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode)
{
    // Reinstall hooks on buffer activation or NPP ready (Scintilla could be recreated)
    if (!notifyCode) return;

    switch (notifyCode->nmhdr.code)
    {
    case NPPN_BUFFERACTIVATED:
    case NPPN_READY:
        EnsureScintillaHooks();
        break;
    default:
        break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    // Required export for Notepad++ compatibility. We subclass Scintilla windows directly,
    // so this can be a no-op. Return 0 to allow normal processing by NPP.
    return 0;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}
#endif
