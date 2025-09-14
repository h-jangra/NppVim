#include "PluginInterface.h"
#include "Scintilla.h"
#include "Notepad_plus_msgs.h"
#include "menuCmdID.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#define NOMINMAX
#define uni8Bit 0

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
static char opPending = 0;
static char textObjectPending = 0;
static std::map<HWND, WNDPROC> origProcMap;
static bool replacePending = false;
static bool vimModeEnabled = false;
static bool commandMode = false;
static std::string commandBuffer;

enum LastOpType { OP_NONE, OP_DELETE_LINE, OP_YANK_LINE, OP_PASTE_LINE, OP_MOTION };
struct LastOperation {
    LastOpType type = OP_NONE;
    int count = 1;
    char motion = 0;
};
static LastOperation lastOp;

void RecordLastOp(LastOpType type, int count = 1, char motion = 0) {
    lastOp.type = type;
    lastOp.count = count;
    lastOp.motion = motion;
}

HWND getCurrentScintillaHandle() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void about() {
    ::MessageBox(nppData._nppHandle, TEXT("A Notepad++ plugin that adds Vim-style editing and key bindings."), TEXT("NppVim Plugin"), MB_OK);
}

void setStatus(const TCHAR* msg) {
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)msg);
}

void enterNormalMode() {
    HWND hwndEdit = getCurrentScintillaHandle();
    currentMode = NORMAL;
    isLineVisual = false;
    visualAnchor = -1;
    repeatCount = 0;
    opPending = 0;
    textObjectPending = 0;
    setStatus(TEXT("-- NORMAL --"));
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
}

void enterInsertMode() {
    HWND hwndEdit = getCurrentScintillaHandle();
    currentMode = INSERT;
    isLineVisual = false;
    visualAnchor = -1;
    repeatCount = 0;
    opPending = 0;
    textObjectPending = 0;
    setStatus(TEXT("-- INSERT --"));
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}

void setVisualSelection(HWND hwndEdit) {
    if (visualAnchor < 0 || currentMode != VISUAL) return;
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), start, end;
    if (isLineVisual) {
        int line1 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, visualAnchor, 0);
        int line2 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
        int startLine = (std::min)(line1, line2), endLine = (std::max)(line1, line2);
        start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);
        int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        if (endLine < totalLines - 1)
            end = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0);
    }
    else {
        start = (std::min)(visualAnchor, caret);
        end = (std::max)(visualAnchor, caret);
        if (end > start) {
            end++;
            int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
            if (end > docLen) end = docLen;
        }
    }
    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
}
void enterVisualCharMode(HWND hwndEdit) {
    currentMode = VISUAL; isLineVisual = false;
    visualAnchor = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    setStatus(TEXT("-- VISUAL --")); setVisualSelection(hwndEdit);
}
void enterVisualLineMode(HWND hwndEdit) {
    currentMode = VISUAL; isLineVisual = true;
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    visualAnchor = start; setStatus(TEXT("-- VISUAL LINE --"));
    setVisualSelection(hwndEdit);
}

void enterCommandMode() {
    commandMode = true;
    commandBuffer.clear();
    setStatus(TEXT(":"));
}

void exitCommandMode() {
    commandMode = false;
    commandBuffer.clear();
    enterNormalMode();
}

void updateCommandStatus() {
    if (commandBuffer.empty()) { setStatus(TEXT(":")); return; }
    std::string display = commandBuffer;
    std::wstring wdisplay(display.begin(), display.end());
    setStatus(wdisplay.c_str());
}

std::pair<int, int> findWordBounds(HWND hwndEdit, int pos) {
    int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
    int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
    return { start, end };
}

std::pair<int, int> findMatchingBrackets(HWND hwndEdit, int pos, bool includeOuter) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    return { pos, pos + 1 };
}
std::pair<int, int> findParagraphBounds(HWND hwndEdit, int pos, bool includeOuter) {
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
    return { start, end };
}
void applyTextObject(HWND hwndEdit, char op, char modifier, char object) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    std::pair<int, int> bounds = { pos, pos };
    bool includeOuter = (modifier == 'a');
    switch (object) {
    case 'w': bounds = findWordBounds(hwndEdit, pos); break;
    case 'b': bounds = findMatchingBrackets(hwndEdit, pos, includeOuter); break;
    case 'p': bounds = findParagraphBounds(hwndEdit, pos, includeOuter); break;
    default: return;
    }
    if (bounds.first != bounds.second) {
        int start = bounds.first, end = bounds.second;
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        switch (op) {
        case 'd': ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0); break;
        case 'y': ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end); ::SendMessage(hwndEdit, SCI_SETSEL, start, start); break;
        case 'c': ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0); ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0); enterInsertMode(); break;
        }
    }
}
void executeEditCommand(const std::string& tabName) {
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    setStatus((TEXT("New tab: ") + std::wstring(tabName.begin(), tabName.end())).c_str());
}

void DeleteLineOnce(HWND hwndEdit) {
    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
    ::SendMessage(hwndEdit, SCI_LINEDELETE, 0, 0);
}

void YankLineOnce(HWND hwndEdit) {
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
}

void DoMotion_char_left(HWND hwndEdit, int count) {
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    caret = (std::max)(0, caret - count);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);

    if (currentMode == VISUAL) {
        if (isLineVisual) {
            int startLine = (std::min)(
                (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0),
                (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, visualAnchor, 0)
                );
            int endLine = (std::max)(
                (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0),
                (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, visualAnchor, 0)
                );
            ::SendMessage(hwndEdit, SCI_SETSEL,
                (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0),
                (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0)
            );
        }
        else {
            ::SendMessage(hwndEdit, SCI_SETSEL,
                (std::min)(caret, visualAnchor),
                (std::max)(caret, visualAnchor)
            );
        }
    }
}

void DoMotion_char_right(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARRIGHT, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}

void DoMotion_line_down(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}
void DoMotion_line_up(HWND hwndEdit, int count) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0);
    }

    if (currentMode == VISUAL) {
        int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    }
}


void DoMotion_word_right(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}
void DoMotion_word_left(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}
void DoMotion_end_word(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHTEND, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}
void DoMotion_end_WORD(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHTEND, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}
void DoMotion_end_line(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
}

void DoMotion_half_page_down(HWND hwndEdit, int count) {
    int linesVisible = (int)::SendMessage(hwndEdit, SCI_LINESONSCREEN, 0, 0);
    int halfPage = (std::max)(1, linesVisible / 2);

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < halfPage; j++) {
            ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0);
        }
    }
    if (currentMode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_half_page_up(HWND hwndEdit, int count) {
    int visibleLines = (int)::SendMessage(hwndEdit, SCI_LINESONSCREEN, 0, 0);
    int halfPage = visibleLines / 2;

    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINESCROLL, 0, -halfPage);

        int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
        int newLine = (std::max)(0, currentLine - halfPage);
        int newPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, newLine, 0);

        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
        ::SendMessage(hwndEdit, SCI_SETANCHOR, newPos, 0);

        if (currentMode == VISUAL) {
            setVisualSelection(hwndEdit);
        }
    }
}


void ApplyOperatorToMotion(HWND hwndEdit, char op, char motion, int count) {
    int start = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    switch (motion) {
    case 'w': DoMotion_word_right(hwndEdit, count); break;
    case 'b': DoMotion_word_left(hwndEdit, count); break;
    case 'e': DoMotion_end_word(hwndEdit, count); break;
    case 'E': DoMotion_end_WORD(hwndEdit, count); break;
    case '$': DoMotion_end_line(hwndEdit, count); break;
    case 'h': DoMotion_char_left(hwndEdit, count); break;
    case 'l': DoMotion_char_right(hwndEdit, count); break;
    case 'j': DoMotion_line_down(hwndEdit, count); break;
    case 'k': DoMotion_line_up(hwndEdit, count); break;
    default: break;
    }
    int end = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    if (start > end) std::swap(start, end);
    if (start == end) {
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (end < docLen) end++;
    }
    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    if (op == 'd') {
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0); ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0); RecordLastOp(OP_MOTION, count, motion);
    }
    else if (op == 'y') {
        ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end); ::SendMessage(hwndEdit, SCI_SETSEL, start, start); RecordLastOp(OP_MOTION, count, motion);
    }
    else if (op == 'c') {
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0); ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0); enterInsertMode(); RecordLastOp(OP_MOTION, count, motion);
    }
}

void RepeatLastOp(HWND hwndEdit) {
    switch (lastOp.type) {
    case OP_DELETE_LINE: for (int i = 0; i < lastOp.count; ++i) DeleteLineOnce(hwndEdit); break;
    case OP_YANK_LINE: for (int i = 0; i < lastOp.count; ++i) YankLineOnce(hwndEdit); break;
    case OP_PASTE_LINE: for (int i = 0; i < lastOp.count; ++i) ::SendMessage(hwndEdit, SCI_PASTE, 0, 0); break;
    case OP_MOTION: ApplyOperatorToMotion(hwndEdit, 'd', lastOp.motion, lastOp.count); break;
    default: break;
    }
}

void opentutor() {
    TCHAR nppPath[MAX_PATH];
    ::SendMessage(nppData._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, (LPARAM)nppPath);
    std::wstring tutorPath = std::wstring(nppPath) + L"\\plugins\\NppVim\\tutor.txt";
    ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)tutorPath.c_str());
}

LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;
    if (!vimModeEnabled) {
        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }
    HWND hwndEdit = hwnd;
    static std::string lastSearchTerm;

    auto enterCommandModeWith = [&](char promptChar) {
        commandMode = true;
        commandBuffer.clear();
        commandBuffer.push_back(promptChar);
        updateCommandStatus();
        };
    auto leaveCommandMode = [&]() {
        commandMode = false;
        commandBuffer.clear();
        enterNormalMode();
        };

    if (commandMode) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                if (commandBuffer.size() >= 1) {
                    HWND sc = getCurrentScintillaHandle();
                    if (commandBuffer[0] == '/' && commandBuffer.size() > 1) {
                        std::string searchTerm = commandBuffer.substr(1);
                        lastSearchTerm = searchTerm;
                        HWND sc = getCurrentScintillaHandle();
                        int docLen = (int)::SendMessage(sc, SCI_GETTEXTLENGTH, 0, 0);
                        ::SendMessage(sc, SCI_SETINDICATORCURRENT, 0, 0);
                        ::SendMessage(sc, SCI_INDICATORCLEARRANGE, 0, docLen);

                        int pos = 0;
                        int matchCount = 0;
                        bool firstSelected = false;

                        while (pos < docLen) {
                            ::SendMessage(sc, SCI_SETTARGETSTART, pos, 0);
                            ::SendMessage(sc, SCI_SETTARGETEND, docLen, 0);
                            int found = (int)::SendMessage(sc, SCI_SEARCHINTARGET, (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());
                            if (found == -1) break;

                            int start = (int)::SendMessage(sc, SCI_GETTARGETSTART, 0, 0);
                            int end = (int)::SendMessage(sc, SCI_GETTARGETEND, 0, 0);
                            ::SendMessage(sc, SCI_SETINDICATORCURRENT, 0, 0);
                            ::SendMessage(sc, SCI_INDICATORFILLRANGE, start, end - start);

                            if (!firstSelected) {
                                ::SendMessage(sc, SCI_SETSEL, start, end);
                                firstSelected = true;
                            }

                            pos = end;
                            matchCount++;
                        }

                        if (matchCount > 0) {
                            std::string status = "Found " + std::to_string(matchCount) + " match" + (matchCount > 1 ? "es" : "");
                            std::wstring wstatus(status.begin(), status.end());
                            setStatus(wstatus.c_str());
                        }
                        else {
                            setStatus(TEXT("No match found"));
                        }
                    }
                    else if (commandBuffer.rfind(":e ", 0) == 0) {
                        std::string tabName = commandBuffer.substr(3);
                        if (!tabName.empty()) {
                            executeEditCommand(tabName);
                        }
                    }
                    else if (commandBuffer == ":tutor") {
                        opentutor();
                    }
                    leaveCommandMode();
                    return 0;
                }
            }
            else if (wParam == VK_ESCAPE) {
                leaveCommandMode();
                return 0;
            }
            else if (wParam == VK_BACK) {
                if (commandBuffer.size() > 1) {
                    commandBuffer.pop_back();
                    updateCommandStatus();
                }
                return 0;
            }
        }
        if (msg == WM_CHAR) {
            char c = (char)wParam;
            if (c >= 32) {
                commandBuffer.push_back(c);
                updateCommandStatus();
            }
            return 0;
        }
        return 0;
    }

    //if (msg == WM_KEYDOWN) {
    //    if (!commandMode && vimModeEnabled) {
    //        // Handle Ctrl+D (half page down)
    //        if (wParam == 'D' && (GetKeyState(VK_CONTROL) & 0x8000)) {
    //            int count = (repeatCount > 0) ? repeatCount : 1;
    //            repeatCount = 0;
    //            DoMotion_half_page_down(hwndEdit, count);
    //            RecordLastOp(OP_MOTION, count, 4); // Use 4 as special code for Ctrl-D
    //            return 0;
    //        }
    //        // Handle Ctrl+U (half page up)  
    //        else if (wParam == 'U' && (GetKeyState(VK_CONTROL) & 0x8000)) {
    //            int count = (repeatCount > 0) ? repeatCount : 1;
    //            repeatCount = 0;
    //            
    //            DoMotion_half_page_up(hwndEdit, count);
    //            RecordLastOp(OP_MOTION, count, 21); // Use 21 as special code for Ctrl-U
    //            return 0;
    //        }
    //    }
    //}

    if (msg == WM_CHAR) {
        char c = (char)wParam;
        if ((int)wParam == VK_ESCAPE) {
            enterNormalMode();
            return 0;
        }

        // Handle INSERT mode first
        if (currentMode == INSERT) {
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }

        // Handle NORMAL and VISUAL mode
        if (currentMode == NORMAL || currentMode == VISUAL) {
            if (replacePending) {
                HWND sc = hwndEdit;
                int pos = (int)::SendMessage(sc, SCI_GETCURRENTPOS, 0, 0);
                if (pos < (int)::SendMessage(sc, SCI_GETTEXTLENGTH, 0, 0)) {
                    ::SendMessage(sc, SCI_SETSEL, pos, pos + 1);
                    std::string repl(1, c);
                    ::SendMessage(sc, SCI_REPLACESEL, 0, (LPARAM)repl.c_str());
                    ::SendMessage(sc, SCI_SETCURRENTPOS, pos, 0);
                }
                replacePending = false;
                return 0;
            }
            if (c == 'r') {
                replacePending = true;
                return 0;
            }
            if (c == '.') {
                RepeatLastOp(hwndEdit);
                return 0;
            }
            if (std::isdigit(static_cast<unsigned char>(c))) {
                int digit = c - '0';
                if (c == '0' && repeatCount == 0) {
                    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    return 0;
                }
                repeatCount = repeatCount * 10 + digit;
                return 0;
            }
            int count = (repeatCount > 0) ? repeatCount : 1;
            repeatCount = 0;

            // Handle visual mode operations first
            if (currentMode == VISUAL) {
                switch (c) {
                case 'd':
                case 'x':
                    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                    enterNormalMode();
                    return 0;
                case 'y':
                {
                    int start = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
                    int end = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
                    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
                    ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
                    enterNormalMode();
                    return 0;
                }
                case 'c':
                    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                    enterInsertMode();
                    return 0;
                case 'v':
                    if (!isLineVisual) {
                        enterNormalMode();
                    }
                    else {
                        enterVisualCharMode(hwndEdit);
                    }
                    return 0;
                case 'V':
                    if (isLineVisual) {
                        enterNormalMode();
                    }
                    else {
                        enterVisualLineMode(hwndEdit);
                    }
                    return 0;
                }
                // Fall through to motion handling for visual mode
            }

            // FIRST: handle text-object composition (e.g., 'y' then 'i' then 'w' => yiw)
            if (textObjectPending && (c == 'w' || c == 'b' || c == 'p')) {
                applyTextObject(hwndEdit, opPending, textObjectPending, c);
                opPending = 0;
                textObjectPending = 0;
                return 0;
            }
            // If operator is waiting and user typed 'i' or 'a' -> mark text-object modifier and wait
            if (opPending && (c == 'i' || c == 'a')) {
                textObjectPending = c;
                return 0;
            }

            // Now handle operator-first commands like dd, dw, etc. (NORMAL mode only)
            if (currentMode == NORMAL) {
                if (c == 'd') {
                    if (opPending == 'd') {
                        for (int i = 0; i < count; ++i) DeleteLineOnce(hwndEdit);
                        RecordLastOp(OP_DELETE_LINE, count);
                        opPending = 0;
                        return 0;
                    }
                    else { opPending = 'd'; return 0; }
                }
                else if (c == 'y') {
                    if (opPending == 'y') {
                        for (int i = 0; i < count; ++i) YankLineOnce(hwndEdit);
                        RecordLastOp(OP_YANK_LINE, count);
                        opPending = 0;
                        return 0;
                    }
                    else { opPending = 'y'; return 0; }
                }
                else if (c == 'c') {
                    if (opPending == 'c') {
                        // cc: change whole line
                        ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                        ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
                        ::SendMessage(hwndEdit, SCI_LINEEND, 1, 0);
                        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                        enterInsertMode();
                        opPending = 0;
                        return 0;
                    }
                    else { opPending = 'c'; return 0; }
                }
                else if (c == 'g') {
                    if (opPending == 'g') {
                        ::SendMessage(hwndEdit, SCI_GOTOPOS, 0, 0);
                        opPending = 0;
                        if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                        return 0;
                    }
                    else { opPending = 'g'; return 0; }
                }
            }

            if (currentMode == NORMAL && opPending && (c == 'w' || c == '$' || c == 'e' || c == 'E' || c == 'b' || c == 'h' || c == 'l' || c == 'j' || c == 'k')) {
                ApplyOperatorToMotion(hwndEdit, opPending, c, count);
                opPending = 0;
                return 0;
            }

            if (opPending && c != 'i' && c != 'a') {
                opPending = 0;
                textObjectPending = 0;
            }

            // Default single-key motions and commands
            switch (c) {
            case 'h':
                DoMotion_char_left(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'h');
                break;
            case 'l':
                DoMotion_char_right(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'l');
                break;
            case 'j':
                DoMotion_line_down(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'j');
                break;
            case 'k':
                DoMotion_line_up(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'k');
                break;
            case 'w':
                DoMotion_word_right(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'w');
                break;
            case 'b':
                DoMotion_word_left(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'b');
                break;
            case 'e':
                DoMotion_end_word(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'e');
                break;
            case 'E':
                DoMotion_end_WORD(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, 'E');
                break;
            case '$':
                DoMotion_end_line(hwndEdit, count);
                RecordLastOp(OP_MOTION, count, '$');
                break;
            case 'i':
                if (!opPending && currentMode == NORMAL) enterInsertMode();
                break;
            case 'a':
                if (!opPending && currentMode == NORMAL) {
                    DoMotion_char_right(hwndEdit, 1);
                    enterInsertMode();
                }
                break;
            case 'A':
                if (currentMode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
                    enterInsertMode();
                }
                break;
            case 'I':
                if (currentMode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                    enterInsertMode();
                }
                break;
            case 'o':
                if (currentMode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
                    ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);
                    enterInsertMode();
                }
                break;
            case 'O':
                if (currentMode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                    ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);
                    DoMotion_line_up(hwndEdit, 1);
                    enterInsertMode();
                }
                break;
            case 'u':
                if (currentMode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_UNDO, 0, 0);
                }
                break;
            case 'v':
                if (currentMode == NORMAL) {
                    enterVisualCharMode(hwndEdit);
                }
                else if (currentMode == VISUAL && !isLineVisual) {
                    enterNormalMode();
                }
                else if (currentMode == VISUAL && isLineVisual) {
                    enterVisualCharMode(hwndEdit);
                }
                break;
            case 'V':
                if (currentMode == NORMAL) {
                    enterVisualLineMode(hwndEdit);
                }
                else if (currentMode == VISUAL && isLineVisual) {
                    enterNormalMode();
                }
                else if (currentMode == VISUAL && !isLineVisual) {
                    enterVisualLineMode(hwndEdit);
                }
                break;
            case 'p':
                if (currentMode == NORMAL) {
                    for (int i = 0; i < count; i++) {
                        ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
                    }
                    RecordLastOp(OP_PASTE_LINE, count);
                }
                break;
            case 'x':
                if (currentMode == NORMAL) {
                    for (int i = 0; i < count; i++) {
                        int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                        int nextPos = pos + 1;
                        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
                        if (nextPos > docLen) nextPos = docLen;
                        if (nextPos > pos) {
                            ::SendMessage(hwndEdit, SCI_SETSEL, pos, nextPos);
                            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                        }
                    }
                }
                break;
            case 'G':
                ::SendMessage(hwndEdit, SCI_DOCUMENTEND, 0, 0);
                if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                break;
            case ':':
                if (currentMode == NORMAL) enterCommandModeWith(':');
                break;
            case '/':
                if (currentMode == NORMAL) enterCommandModeWith('/');
                break;
            case 'n':
                if (currentMode == NORMAL && !lastSearchTerm.empty()) {
                    HWND sc = getCurrentScintillaHandle();
                    int docLen = (int)::SendMessage(sc, SCI_GETTEXTLENGTH, 0, 0);
                    int caret = (int)::SendMessage(sc, SCI_GETCURRENTPOS, 0, 0);
                    ::SendMessage(sc, SCI_SETTARGETSTART, caret, 0);
                    ::SendMessage(sc, SCI_SETTARGETEND, docLen, 0);
                    int found = (int)::SendMessage(sc, SCI_SEARCHINTARGET, (WPARAM)lastSearchTerm.length(), (LPARAM)lastSearchTerm.c_str());
                    if (found != -1) {
                        int s = (int)::SendMessage(sc, SCI_GETTARGETSTART, 0, 0);
                        int e = (int)::SendMessage(sc, SCI_GETTARGETEND, 0, 0);
                        ::SendMessage(sc, SCI_SETSEL, s, e);
                    }
                    else {
                        ::SendMessage(sc, SCI_SETTARGETSTART, 0, 0);
                        ::SendMessage(sc, SCI_SETTARGETEND, docLen, 0);
                        found = (int)::SendMessage(sc, SCI_SEARCHINTARGET, (WPARAM)lastSearchTerm.length(), (LPARAM)lastSearchTerm.c_str());
                        if (found != -1) {
                            int s = (int)::SendMessage(sc, SCI_GETTARGETSTART, 0, 0);
                            int e = (int)::SendMessage(sc, SCI_GETTARGETEND, 0, 0);
                            ::SendMessage(sc, SCI_SETSEL, s, e);
                        }
                    }
                }
                break;
            case 'N':
                if (currentMode == NORMAL && !lastSearchTerm.empty()) {
                    HWND sc = getCurrentScintillaHandle();
                    int docLen = (int)::SendMessage(sc, SCI_GETTEXTLENGTH, 0, 0);
                    int caret = (int)::SendMessage(sc, SCI_GETCURRENTPOS, 0, 0);

                    int pos = 0;
                    int lastStart = -1, lastEnd = -1;
                    while (pos < docLen) {
                        ::SendMessage(sc, SCI_SETTARGETSTART, pos, 0);
                        ::SendMessage(sc, SCI_SETTARGETEND, docLen, 0);
                        int found = (int)::SendMessage(sc, SCI_SEARCHINTARGET, (WPARAM)lastSearchTerm.length(), (LPARAM)lastSearchTerm.c_str());
                        if (found == -1) break;

                        int s = (int)::SendMessage(sc, SCI_GETTARGETSTART, 0, 0);
                        int e = (int)::SendMessage(sc, SCI_GETTARGETEND, 0, 0);
                        if (e >= caret) break;
                        lastStart = s; lastEnd = e;
                        pos = e;
                    }

                    if (lastStart != -1) {
                        ::SendMessage(sc, SCI_SETSEL, lastStart, lastEnd);
                    }
                    else {
                        pos = 0; lastStart = -1; lastEnd = -1;
                        while (pos < docLen) {
                            ::SendMessage(sc, SCI_SETTARGETSTART, pos, 0);
                            ::SendMessage(sc, SCI_SETTARGETEND, docLen, 0);
                            int found = (int)::SendMessage(sc, SCI_SEARCHINTARGET, (WPARAM)lastSearchTerm.length(), (LPARAM)lastSearchTerm.c_str());
                            if (found == -1) break;
                            int s = (int)::SendMessage(sc, SCI_GETTARGETSTART, 0, 0);
                            int e = (int)::SendMessage(sc, SCI_GETTARGETEND, 0, 0);
                            lastStart = s; lastEnd = e;
                            pos = e;
                        }
                        if (lastStart != -1) ::SendMessage(sc, SCI_SETSEL, lastStart, lastEnd);
                    }
                }
                break;
            }
            return 0;
        }
    }
    return CallWindowProc(orig, hwnd, msg, wParam, lParam);
}

void InstallScintillaHookFor(HWND hwnd) {
    if (!hwnd) return;
    if (origProcMap.find(hwnd) != origProcMap.end()) return;
    WNDPROC prev = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)ScintillaHookProc);
    if (prev) {
        origProcMap[hwnd] = prev;
    }
}

void RemoveAllScintillaHooks() {
    for (auto& p : origProcMap) {
        if (IsWindow(p.first)) {
            SetWindowLongPtr(p.first, GWLP_WNDPROC, (LONG_PTR)p.second);
        }
    }
    origProcMap.clear();
}

void EnsureScintillaHooks() {
    HWND main = nppData._scintillaMainHandle;
    HWND second = nppData._scintillaSecondHandle;
    InstallScintillaHookFor(main);
    InstallScintillaHookFor(second);
}

void ToggleVimMode() {
    vimModeEnabled = !vimModeEnabled;
    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0);
    if (hMenu != NULL) {
        ::CheckMenuItem(hMenu, funcItem[0]._cmdID, MF_BYCOMMAND | (vimModeEnabled ? MF_CHECKED : MF_UNCHECKED));
    }
    if (vimModeEnabled) {
        enterNormalMode();
    }
    else {
        setStatus(TEXT(""));
        ::SendMessage(getCurrentScintillaHandle(), SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
    }
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/) {
    if (reasonForCall == DLL_PROCESS_ATTACH) {
        g_hInstance = (HINSTANCE)hModule;
    }
    else if (reasonForCall == DLL_PROCESS_DETACH) {
        RemoveAllScintillaHooks();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
    vimModeEnabled = true;
    EnsureScintillaHooks();
    enterNormalMode();
}

extern "C" __declspec(dllexport) const TCHAR* getName() {
    return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
    *nbF = nbFunc;
    lstrcpy(funcItem[0]._itemName, TEXT("Toggle Vim Mode"));
    funcItem[0]._pFunc = ToggleVimMode;
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
        EnsureScintillaHooks();
        break;
    default:
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