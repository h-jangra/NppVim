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

void enterVisualCharMode(HWND hwndEdit) {
    currentMode = VISUAL;
    isLineVisual = false;
    visualAnchor = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    setStatus(TEXT("-- VISUAL --"));
}

void enterVisualLineMode(HWND hwndEdit) {
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
    if (commandBuffer.empty()) {
        setStatus(TEXT(":"));
        return;
    }
    std::string display = commandBuffer;
    std::wstring wdisplay(display.begin(), display.end());
    setStatus(wdisplay.c_str());
}

void setVisualSelection(HWND hwndEdit) {
    if (visualAnchor < 0) return;
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int a, b;
    if (isLineVisual) {
        int line1 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, visualAnchor, 0);
        int line2 = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
        int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, (std::min)(line1, line2), 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, (std::max)(line1, line2), 0);
        a = start;
        b = end;
    }
    else {
        a = (std::min)(visualAnchor, caret);
        b = (std::max)(visualAnchor, caret);
        if (b > a) b++;
    }
    ::SendMessage(hwndEdit, SCI_SETSEL, a, b);
}

std::pair<int, int> findWordBounds(HWND hwndEdit, int pos) {
    int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
    int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
    return { start, end };
}

std::pair<int, int> findMatchingBrackets(HWND hwndEdit, int pos, bool includeOuter) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    std::vector<char> text(docLen + 1);
    ::SendMessage(hwndEdit, SCI_GETTEXT, docLen + 1, (LPARAM)text.data());
    const char* brackets = "(){}[]";
    int bracketPos = -1;
    char bracketChar = 0;
    for (int i = pos; i >= 0; i--) {
        char c = text[i];
        if (strchr(brackets, c)) {
            bracketPos = i;
            bracketChar = c;
            break;
        }
    }
    if (bracketPos == -1) {
        for (int i = pos; i < docLen; i++) {
            char c = text[i];
            if (strchr(brackets, c)) {
                bracketPos = i;
                bracketChar = c;
                break;
            }
        }
    }
    if (bracketPos == -1) return { pos, pos };
    char openBracket, closeBracket;
    int direction;
    int start;
    if (bracketChar == '(' || bracketChar == ')') {
        openBracket = '('; closeBracket = ')';
        direction = (bracketChar == '(') ? 1 : -1;
        start = bracketPos;
    }
    else if (bracketChar == '{' || bracketChar == '}') {
        openBracket = '{'; closeBracket = '}';
        direction = (bracketChar == '{') ? 1 : -1;
        start = bracketPos;
    }
    else if (bracketChar == '[' || bracketChar == ']') {
        openBracket = '['; closeBracket = ']';
        direction = (bracketChar == '[') ? 1 : -1;
        start = bracketPos;
    }
    else {
        return { pos, pos };
    }
    int count = 1;
    int i = start + direction;
    while (i >= 0 && i < docLen && count > 0) {
        if (text[i] == openBracket) {
            count += direction;
        }
        else if (text[i] == closeBracket) {
            count -= direction;
        }
        if (count > 0) i += direction;
    }
    if (count == 0) {
        int left = (std::min)(start, i);
        int right = (std::max)(start, i);
        if (includeOuter) {
            return { left, right + 1 };
        }
        else {
            return { left + 1, right };
        }
    }
    return { pos, pos };
}

std::pair<int, int> findParagraphBounds(HWND hwndEdit, int pos, bool includeOuter) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    std::vector<char> text(docLen + 1);
    ::SendMessage(hwndEdit, SCI_GETTEXT, docLen + 1, (LPARAM)text.data());
    int start = pos;
    int end = pos;
    while (start > 0) {
        int lineStart = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, start, 0);
        int linePos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lineStart, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, lineStart, 0);
        bool isEmpty = true;
        for (int i = linePos; i < lineEnd; i++) {
            if (!isspace(text[i])) {
                isEmpty = false;
                break;
            }
        }
        if (isEmpty) {
            start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lineStart + 1, 0);
            break;
        }
        if (lineStart == 0) {
            start = 0;
            break;
        }
        start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lineStart - 1, 0);
    }
    while (end < docLen) {
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, end, 0);
        int linePos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lineEnd, 0);
        int lineEndPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, lineEnd, 0);
        bool isEmpty = true;
        for (int i = linePos; i < lineEndPos; i++) {
            if (!isspace(text[i])) {
                isEmpty = false;
                break;
            }
        }
        if (isEmpty) {
            end = linePos;
            break;
        }
        int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        if (lineEnd >= totalLines - 1) {
            end = docLen;
            break;
        }
        end = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lineEnd + 1, 0);
    }
    if (includeOuter) {
        while (start > 0 && isspace(text[start - 1])) start--;
        while (end < docLen && isspace(text[end])) end++;
    }
    return { start, end };
}

void applyTextObject(HWND hwndEdit, char op, char modifier, char object) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    std::pair<int, int> bounds = { pos, pos };
    bool includeOuter = (modifier == 'a');
    switch (object) {
    case 'w':
        bounds = findWordBounds(hwndEdit, pos);
        break;
    case 'b':
        bounds = findMatchingBrackets(hwndEdit, pos, includeOuter);
        break;
    case 'p':
        bounds = findParagraphBounds(hwndEdit, pos, includeOuter);
        break;
    }
    if (bounds.first != bounds.second) {
        ::SendMessage(hwndEdit, SCI_SETSEL, bounds.first, bounds.second);
        switch (op) {
        case 'd':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            break;
        case 'y':
            ::SendMessage(hwndEdit, SCI_COPY, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETSEL, bounds.first, bounds.first);
            break;
        case 'c':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            enterInsertMode();
            break;
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

void DoMotion_char_left(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARLEFT, 0, 0); }
void DoMotion_char_right(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_CHARRIGHT, 0, 0); }
void DoMotion_line_down(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0); }
void DoMotion_line_up(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0); }
void DoMotion_word_right(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0); }
void DoMotion_word_left(HWND hwndEdit, int count) { for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0); }

void DoMotion_end_word(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_WORDRIGHTEND, 0, 0);
}

void DoMotion_end_WORD(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        pos = (int)::SendMessage(hwndEdit, SCI_WORDRIGHTEND, pos, 0);
        while (true) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
            if (isspace(c) || c == '\0') break;
            pos = (int)::SendMessage(hwndEdit, SCI_WORDRIGHTEND, pos, 0);
        }
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos - 1, 0);
    }
}

void DoMotion_end_line(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
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
    }
    int end = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    if (start > end) std::swap(start, end);
    if (op != 'd' && op != 'c') end++;
    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    switch (op) {
    case 'd': ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0); break;
    case 'y': ::SendMessage(hwndEdit, SCI_COPY, 0, 0); ::SendMessage(hwndEdit, SCI_SETSEL, start, start); break;
    case 'c': ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0); enterInsertMode(); break;
    }
    RecordLastOp(OP_MOTION, count, motion);
}

void RepeatLastOp(HWND hwndEdit) {
    switch (lastOp.type) {
    case OP_DELETE_LINE:
        for (int i = 0; i < lastOp.count; ++i) DeleteLineOnce(hwndEdit);
        break;
    case OP_YANK_LINE:
        for (int i = 0; i < lastOp.count; ++i) YankLineOnce(hwndEdit);
        break;
    case OP_PASTE_LINE:
        for (int i = 0; i < lastOp.count; ++i) ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
        break;
    case OP_MOTION:
        switch (lastOp.motion) {
        case 'h': DoMotion_char_left(hwndEdit, lastOp.count); break;
        case 'l': DoMotion_char_right(hwndEdit, lastOp.count); break;
        case 'j': DoMotion_line_down(hwndEdit, lastOp.count); break;
        case 'k': DoMotion_line_up(hwndEdit, lastOp.count); break;
        case 'w': DoMotion_word_right(hwndEdit, lastOp.count); break;
        case 'b': DoMotion_word_left(hwndEdit, lastOp.count); break;
        case 'e': DoMotion_end_word(hwndEdit, lastOp.count); break;
        case 'E': DoMotion_end_WORD(hwndEdit, lastOp.count); break;
        case '$': DoMotion_end_line(hwndEdit, lastOp.count); break;
        }
        break;
    default: break;
    }
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

                        // Update status bar
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

    if (msg == WM_CHAR) {
        char c = (char)wParam;
        if ((int)wParam == VK_ESCAPE) {
            enterNormalMode();
            return 0;
        }
        if (currentMode == NORMAL) {
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
            if (currentMode == INSERT) {
                repeatCount = 0;
                opPending = 0;
                textObjectPending = 0;
                return CallWindowProc(orig, hwnd, msg, wParam, lParam);
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
            if (textObjectPending && (c == 'w' || c == 'b' || c == 'p')) {
                applyTextObject(hwndEdit, opPending, textObjectPending, c);
                opPending = 0;
                textObjectPending = 0;
                return 0;
            }
            if (opPending && (c == 'i' || c == 'a')) {
                textObjectPending = c;
                return 0;
            }
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
            else {
                if (opPending && c != 'i' && c != 'a') {
                    opPending = 0;
                    textObjectPending = 0;
                }
                if (opPending && (c == 'w' || c == 'e' || c == 'E' || c == '$' || c == 'b')) {
                    ApplyOperatorToMotion(hwndEdit, opPending, c, count);
                    opPending = 0;
                    return 0;
                }
                switch (c) {
                case 'h':
                    DoMotion_char_left(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'h');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'l':
                    DoMotion_char_right(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'l');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'j':
                    DoMotion_line_down(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'j');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'k':
                    DoMotion_line_up(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'k');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'w':
                    DoMotion_word_right(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'w');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'b':
                    DoMotion_word_left(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'b');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'e':
                    DoMotion_end_word(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'e');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'E':
                    DoMotion_end_WORD(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, 'E');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'i':
                    if (!opPending) enterInsertMode();
                    break;
                case 'a':
                    if (!opPending) {
                        DoMotion_char_right(hwndEdit, 1);
                        enterInsertMode();
                    }
                    break;
                case 'A':
                    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
                    enterInsertMode();
                    break;
                case 'I':
                    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                    enterInsertMode();
                    break;
                case 'o':
                    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
                    ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);
                    enterInsertMode();
                    break;
                case 'O':
                    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                    ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);
                    DoMotion_line_up(hwndEdit, 1);
                    enterInsertMode();
                    break;
                case 'u': ::SendMessage(hwndEdit, SCI_UNDO, 0, 0); break;
                case 'v':
                    if (currentMode == VISUAL && !isLineVisual)
                        enterNormalMode();
                    else
                        enterVisualCharMode(hwndEdit);
                    break;
                case 'V':
                    if (currentMode == VISUAL && isLineVisual)
                        enterNormalMode();
                    else
                        enterVisualLineMode(hwndEdit);
                    break;
                case 'p':
                    for (int i = 0; i < count; i++) {
                        ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
                    }
                    RecordLastOp(OP_PASTE_LINE, count);
                    break;
                case 'x':
                    if (currentMode == VISUAL) {
                        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                        enterNormalMode();
                    }
                    else {
                        for (int i = 0; i < count; i++) {
                            int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                            int nextPos = (int)::SendMessage(hwndEdit, SCI_POSITIONAFTER, pos, 0);
                            if (nextPos > pos) {
                                ::SendMessage(hwndEdit, SCI_SETSEL, pos, nextPos);
                                ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                            }
                        }
                    }
                    break;
                case '0':
                    if (repeatCount == 0) {
                        ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
                        if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    }
                    break;
                case '$':
                    DoMotion_end_line(hwndEdit, count);
                    RecordLastOp(OP_MOTION, count, '$');
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case 'G':
                    ::SendMessage(hwndEdit, SCI_DOCUMENTEND, 0, 0);
                    if (currentMode == VISUAL) setVisualSelection(hwndEdit);
                    break;
                case ':': enterCommandModeWith(':'); break;
                case 'n':
                {
                    if (!lastSearchTerm.empty())
                    {
                        HWND sc = getCurrentScintillaHandle();
                        int docLen = (int)::SendMessage(sc, SCI_GETTEXTLENGTH, 0, 0);
                        int caret = (int)::SendMessage(sc, SCI_GETCURRENTPOS, 0, 0);
                        ::SendMessage(sc, SCI_SETTARGETSTART, caret, 0);
                        ::SendMessage(sc, SCI_SETTARGETEND, docLen, 0);
                        int found = (int)::SendMessage(sc, SCI_SEARCHINTARGET, (WPARAM)lastSearchTerm.length(), (LPARAM)lastSearchTerm.c_str());
                        if (found != -1)
                        {
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
                }

                case 'N':
                {
                    if (!lastSearchTerm.empty())
                    {
                        HWND sc = getCurrentScintillaHandle();
                        int docLen = (int)::SendMessage(sc, SCI_GETTEXTLENGTH, 0, 0);
                        int caret = (int)::SendMessage(sc, SCI_GETCURRENTPOS, 0, 0);

                        int pos = 0;
                        int lastStart = -1, lastEnd = -1;
                        while (pos < docLen)
                        {
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

                        if (lastStart != -1)
                        {
                            ::SendMessage(sc, SCI_SETSEL, lastStart, lastEnd);
                        }
                        else {
                            pos = 0; lastStart = -1; lastEnd = -1;
                            while (pos < docLen)
                            {
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
                case '/': enterCommandModeWith('/'); break;
            }
                return 0;
            }
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
