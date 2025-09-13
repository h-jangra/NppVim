#include "PluginInterface.h"
#include "Scintilla.h"
#include "Notepad_plus_msgs.h"
#include <windows.h>
#include <string>
#include <map>
#include <algorithm>
#include <cctype>

#define NOMINMAX

HINSTANCE g_hInstance = nullptr;
NppData nppData;
const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 3;
FuncItem funcItem[nbFunc];

enum VimMode { NORMAL, INSERT, VISUAL };
static VimMode currentMode = NORMAL;
static bool isLineVisual = false;
static int visualAnchor = -1;

static int repeatCount = 0;
static char opPending = 0; // 'd' or 'y' when waiting for second key
static std::map<HWND, WNDPROC> origProcMap;

static bool vimModeEnabled = false;

enum LastOpType { OP_NONE, OP_DELETE_LINE, OP_YANK_LINE, OP_PASTE_LINE, OP_MOTION };

struct LastOperation {
    LastOpType type = OP_NONE;
    int count = 1;
    char motion = 0; // for motions: 'h','j','k','l','w','b'
};
static LastOperation lastOp;
void RecordLastOp(LastOpType type, int count = 1, char motion = 0) {
    lastOp.type = type;
    lastOp.count = count;
    lastOp.motion = motion;
}

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
        int line1 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, visualAnchor, 0);
        int line2 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);

        int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, (std::min)(line1, line2), 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, (std::max)(line1, line2), 0);

        a = start;
        b = end;
    }
    else
    {
        a = (std::min)(visualAnchor, caret);
        b = (std::max)(visualAnchor, caret);
    }

    ::SendMessage(hwndEdit, SCI_SETSEL, a, b);
}


void DeleteLineOnce(HWND hwndEdit)
{
    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
    ::SendMessage(hwndEdit, SCI_LINEDELETE, 0, 0);
}

void YankLineOnce(HWND hwndEdit)
{
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
}
void DoMotion_char_left(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARLEFT, 0, 0); }
void DoMotion_char_right(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARRIGHT, 0, 0); }
void DoMotion_line_down(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0); }
void DoMotion_line_up(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0); }
void DoMotion_word_right(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0); }
void DoMotion_word_left(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0); }

// Helper to repeat last operation
void RepeatLastOp(HWND hwndEdit) {
    switch (lastOp.type) {
    case OP_DELETE_LINE:
        for (int i = 0; i < lastOp.count; ++i) DeleteLineOnce(hwndEdit);
        break;
    case OP_YANK_LINE:
        for (int i = 0; i < lastOp.count; ++i) YankLineOnce(hwndEdit);
        break;
    case OP_PASTE_LINE:
        for (int i = 0; i < lastOp.count; ++i)
            ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
        break;
    case OP_MOTION:
        switch (lastOp.motion) {
        case 'h': DoMotion_char_left(hwndEdit, lastOp.count); break;
        case 'l': DoMotion_char_right(hwndEdit, lastOp.count); break;
        case 'j': DoMotion_line_down(hwndEdit, lastOp.count); break;
        case 'k': DoMotion_line_up(hwndEdit, lastOp.count); break;
        case 'w': DoMotion_word_right(hwndEdit, lastOp.count); break;
        case 'b': DoMotion_word_left(hwndEdit, lastOp.count); break;
        }
        break;
    default: break;
    }
}


LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;

    // If Vim mode is disabled, pass all messages through
    if (!vimModeEnabled) {
        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }

    if (msg == WM_CHAR)
    {
        HWND hwndEdit = hwnd;

        // ESC (27) always to Normal
        if ((int)wParam == VK_ESCAPE)
        {
            enterNormalMode();
            return 0; // block further processing
        }

        if (currentMode == INSERT)
        {
            repeatCount = 0;
            opPending = 0;
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }

        char c = (char)wParam;

        if (c == '.') {
            HWND hwndEdit = hwnd;
            RepeatLastOp(hwndEdit);
            return 0;
        }

        // digits -> repeat count (accumulate)
        if (std::isdigit(static_cast<unsigned char>(c)))
        {
            int digit = c - '0';
            if (c == '0' && repeatCount == 0)
            {
                // '0' command: move to start of line
                ::SendMessage(hwndEdit, SCI_HOME, 0, 0); 
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                ::SendMessage(hwndEdit, SCI_SETANCHOR, pos, 0); 
                if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                if (currentMode == NORMAL) opPending = 0;
                return 0;
            }
            // Otherwise, accumulate repeat count
            repeatCount = repeatCount * 10 + digit;
            return 0;
        }

        // compute effective count
        int count = (repeatCount > 0) ? repeatCount : 1;
        repeatCount = 0; // consume the count

        // If operator pending and the same operator repeated like 'dd' or 'yy', handle; else set opPending
        if (c == 'd')
        {
            if (opPending == 'd')
            {
                // perform dd
                for (int i = 0; i < count; ++i) DeleteLineOnce(hwndEdit);
                RecordLastOp(OP_DELETE_LINE, count);
                opPending = 0;
                return 0;
            }
            else
            {
                opPending = 'd';
                // wait for next char
                return 0;
            }
        }
        else if (c == 'y')
        {
            if (opPending == 'y')
            {
                for (int i = 0; i < count; ++i) YankLineOnce(hwndEdit);
                RecordLastOp(OP_YANK_LINE, count);
                opPending = 0;
                return 0;
            }
            else
            {
                opPending = 'y';
                return 0;
            }
        }
        else if (c == 'g')
        {
            if (opPending == 'g')
            {
                // perform gg: go to first line
                ::SendMessage(hwndEdit, SCI_GOTOPOS, 0, 0);
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                ::SendMessage(hwndEdit, SCI_SETANCHOR, pos, 0);
                if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                opPending = 0;
                return 0;
            }
            else
            {
                opPending = 'g'; // wait for next key
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
            RecordLastOp(OP_MOTION, count, 'h');
            break;
        case 'l': 
            DoMotion_char_right(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            RecordLastOp(OP_MOTION, count, 'l');
            break;
        case 'j':
            DoMotion_line_down(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            RecordLastOp(OP_MOTION, count, 'j');
            break;
        case 'k':
            DoMotion_line_up(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            RecordLastOp(OP_MOTION, count, 'k');
            break;
        case 'w':
            DoMotion_word_right(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            RecordLastOp(OP_MOTION, count, 'w');
            break;
        case 'b':
            DoMotion_word_left(hwndEdit, count);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            RecordLastOp(OP_MOTION, count, 'b');
            break;
        case 'i':
            enterInsertMode();
            break;

        case 'D':
        {
            int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
            int endPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

            if (endPos > caret)
            {
                ::SendMessage(hwndEdit, SCI_SETSEL, caret, endPos);
                ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            }
            break;
        }
        case 'O':
        {
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
                (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);

            int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
            ::SendMessage(hwndEdit, SCI_GOTOPOS, lineStart, 0);

            ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);

            int newLinePos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
            ::SendMessage(hwndEdit, SCI_GOTOPOS, newLinePos, 0);

            enterInsertMode();
            break;
        }

        case 'o':
        {
            int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
            int endOfLinePos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, endOfLinePos, 0);
            ::SendMessage(hwndEdit, SCI_SETANCHOR, endOfLinePos, 0);

            ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);

            int newLinePos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newLinePos, 0);
            ::SendMessage(hwndEdit, SCI_SETANCHOR, newLinePos, 0);

            enterInsertMode();
            break;
        }
        case 'u':
            ::SendMessage(hwndEdit, SCI_UNDO, 0, 0);
            break;

        case 'v':
            if (currentMode == VISUAL && !isLineVisual) {
                enterNormalMode();
            }
            else {
                enterVisualCharMode(hwndEdit);
            }
            break;
        case 'V':
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

        case '$':
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
        case 'x':
        {
            if (currentMode == VISUAL) {
                // delete selection
                ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                enterNormalMode(); // exit visual after delete
            }
            else {
                // delete single char under cursor
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                int nextPos = (int)::SendMessage(hwndEdit, SCI_POSITIONAFTER, pos, 0);
                if (nextPos > pos) {
                    ::SendMessage(hwndEdit, SCI_SETSEL, pos, nextPos);
                    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                }
            }
            break;
        }

        case 'gg':


        case 'G':
        {
            int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
            int lastLineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lineCount - 1, 0);
            ::SendMessage(hwndEdit, SCI_GOTOPOS, lastLineStart, 0);
            if (currentMode == VISUAL) setVisualSelection(hwndEdit);
            break;
		}
        case '0':
            // Handled above as special case
			break;

        default: break;
        }

        if (currentMode == VISUAL && !isLineVisual)
        {
            setVisualSelection(hwndEdit);
        }

        return 0;
    }

    return CallWindowProc(orig, hwnd, msg, wParam, lParam);
}

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

void ToggleVimMode()
{
    vimModeEnabled = !vimModeEnabled;

    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0);
    if (hMenu != NULL) {
        ::CheckMenuItem(hMenu, funcItem[0]._cmdID, MF_BYCOMMAND | (vimModeEnabled ? MF_CHECKED : MF_UNCHECKED));
    }

    if (vimModeEnabled) {
        enterNormalMode(); // enable Vim mode
    }
    else {
        // Disable Vim mode
        setStatus(TEXT(""));
        ::SendMessage(getCurrentScintillaHandle(), SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
    }
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
    vimModeEnabled = true; // Start with Vim mode enabled
    EnsureScintillaHooks();
    enterNormalMode();;
}

extern "C" __declspec(dllexport) const TCHAR* getName()
{
    return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF)
{
    *nbF = nbFunc;

    lstrcpy(funcItem[0]._itemName, TEXT("Toggle Vim Mode"));
    funcItem[0]._pFunc = ToggleVimMode;
    funcItem[0]._pShKey = NULL;
    funcItem[0]._init2Check = false; // Ensure it starts unchecked

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
    return 0;
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}
#endif
