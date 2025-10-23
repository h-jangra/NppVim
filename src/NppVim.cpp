#include <windows.h>
#include <map>

#include "../plugin/PluginInterface.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/menuCmdID.h"

#include "../include/NppVim.h"
#include "../include/NormalMode.h"
#include "../include/VisualMode.h"
#include "../include/CommandMode.h"
#include "../include/Utils.h"

HINSTANCE g_hInstance = nullptr;
NppData nppData;
VimState g_state;

NormalMode* g_normalMode = nullptr;
VisualMode* g_visualMode = nullptr;
CommandMode* g_commandMode = nullptr;

const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 3;
FuncItem funcItem[nbFunc];

static std::map<HWND, WNDPROC> origProcMap;

// Forward declarations
LRESULT CALLBACK ScintillaHookProc(HWND, UINT, WPARAM, LPARAM);
void installScintillaHookFor(HWND hwnd);
void removeAllScintillaHooks();
void ensureScintillaHooks();
void toggleVimMode();
void about();

// Helper to set NppData in Utils
void setNppData(NppData data) {
    Utils::nppData = data;
}

LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;

    if (!g_state.vimEnabled || !orig) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    HWND hwndEdit = hwnd;

    // Handle command mode
    if (g_state.commandMode) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                g_commandMode->handleEnter(hwndEdit);
                return 0;
            } else if (wParam == VK_ESCAPE) {
                Utils::clearSearchHighlights(hwndEdit);
                g_state.lastSearchMatchCount = -1;
                g_commandMode->exit();
                return 0;
            } else if (wParam == VK_BACK) {
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

    // Handle regular key presses
    if (msg == WM_CHAR) {
        char c = (char)wParam;

        // ESC key
        if ((int)wParam == VK_ESCAPE) {
            if (g_state.mode == INSERT) {
                ::SendMessage(hwndEdit, SCI_SETOVERTYPE, false, 0);
            }
            g_normalMode->enter();
            return 0;
        }

        // In insert mode, let keys through
        if (g_state.mode == INSERT) {
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }

        // Handle replace pending
        if (g_state.replacePending && g_state.mode == NORMAL) {
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
            g_state.replacePending = false;
            g_state.repeatCount = 0;
            return 0;
        }

        // Route to appropriate mode handler
        if (g_state.mode == NORMAL) {
            g_normalMode->handleKey(hwndEdit, c);
            return 0;
        } else if (g_state.mode == VISUAL) {
            g_visualMode->handleKey(hwndEdit, c);
            return 0;
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

void toggleVimMode() {
    g_state.vimEnabled = !g_state.vimEnabled;
    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0);
    if (hMenu) {
        ::CheckMenuItem(hMenu, funcItem[0]._cmdID,
            MF_BYCOMMAND | (g_state.vimEnabled ? MF_CHECKED : MF_UNCHECKED));
    }

    if (g_state.vimEnabled) {
        ensureScintillaHooks();
        g_normalMode->enter();
    } else {
        Utils::setStatus(TEXT(""));
        ::SendMessage(Utils::getCurrentScintillaHandle(), SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
        removeAllScintillaHooks();
    }
}

void about() {
    ::MessageBox(nppData._nppHandle,
        TEXT("A Notepad++ plugin that adds Vim-style editing and key bindings."),
        TEXT("NppVim Plugin"), MB_OK);
}

// DLL Entry Point
BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/) {
    if (reasonForCall == DLL_PROCESS_ATTACH) {
        g_hInstance = (HINSTANCE)hModule;
    } else if (reasonForCall == DLL_PROCESS_DETACH) {
        removeAllScintillaHooks();

        // Cleanup mode handlers
        if (g_normalMode) delete g_normalMode;
        if (g_visualMode) delete g_visualMode;
        if (g_commandMode) delete g_commandMode;
    }
    return TRUE;
}

// Plugin Interface
extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
    setNppData(notpadPlusData);

    // Initialize mode handlers
    g_normalMode = new NormalMode(g_state);
    g_visualMode = new VisualMode(g_state);
    g_commandMode = new CommandMode(g_state);

    g_state.vimEnabled = true;
    ensureScintillaHooks();
    g_normalMode->enter();
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

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode) {
    if (!notifyCode) return;

    switch (notifyCode->nmhdr.code) {
        case NPPN_BUFFERACTIVATED:
        case NPPN_READY:
            if (g_state.vimEnabled) ensureScintillaHooks();
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
