#include "plugin/PluginInterface.h"
#include "plugin/Scintilla.h"
#include "plugin/Notepad_plus_msgs.h"
#include "plugin/menuCmdID.h"
#include <algorithm>
#include <map>
#include <string>
#include <windows.h>
#include <cctype>

HINSTANCE g_hInstance = nullptr;
NppData nppData;
const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 3;
FuncItem funcItem[nbFunc];

enum VimMode { NORMAL, INSERT, VISUAL };
enum LastOpType { OP_NONE, OP_DELETE_LINE, OP_YANK_LINE, OP_PASTE_LINE, OP_MOTION, OP_REPLACE };

enum TextObjectType {
    TEXT_OBJECT_WORD,
    TEXT_OBJECT_BIG_WORD,
    TEXT_OBJECT_QUOTE,
    TEXT_OBJECT_BRACKET,
    TEXT_OBJECT_PAREN,
    TEXT_OBJECT_ANGLE,
    TEXT_OBJECT_BRACE,
    TEXT_OBJECT_SENTENCE,
    TEXT_OBJECT_PARAGRAPH
};

struct State {
    VimMode mode = NORMAL;
    bool isLineVisual = false;
    int visualAnchor = -1;
    int visualAnchorLine = -1;
    int repeatCount = 0;
    char opPending = 0;
    char textObjectPending = 0;
    bool replacePending = false;
    bool vimEnabled = false;
    bool commandMode = false;
    bool useRegex = false;
    std::string commandBuffer;
    std::string lastSearchTerm;
    int lastSearchMatchCount = -1;

    char lastSearchChar = 0;
    bool lastSearchForward = true;

    struct {
        LastOpType type = OP_NONE;
        int count = 1;
        char motion = 0;
        char searchChar = 0;
    } lastOp;
} state;

static std::map<HWND, WNDPROC> origProcMap;

// Declarations
LRESULT CALLBACK ScintillaHookProc(HWND, UINT, WPARAM, LPARAM);
void enterNormalMode();
void enterInsertMode();
HWND getCurrentScintillaHandle();
void setStatus(const TCHAR* msg);
void clearSearchHighlights(HWND hwndEdit);
void setVisualSelection(HWND hwndEdit);
void enterVisualCharMode(HWND hwndEdit);
void enterVisualLineMode(HWND hwndEdit);
void enterCommandMode(char prompt);
void exitCommandMode();
void updateCommandStatus();
void DoMotion_char_left(HWND hwndEdit, int count);
void DoMotion_char_right(HWND hwndEdit, int count);
void DoMotion_line_up(HWND hwndEdit, int count);
void DoMotion_end_word_prev(HWND hwndEdit, int count);
void DoMotion_next_char(HWND hwndEdit, int count, char searchChar);
void DoMotion_prev_char(HWND hwndEdit, int count, char searchChar);
void DoMotion_line_down(HWND hwndEdit, int count);
void DoMotion_word_right(HWND hwndEdit, int count);
void DoMotion_word_left(HWND hwndEdit, int count);
void DoMotion_end_word(HWND hwndEdit, int count);
void DoMotion_end_line(HWND hwndEdit, int count);
void DoMotion_line_start(HWND hwndEdit, int count);
void deleteToEndOfLine(HWND hwndEdit, int count);
void joinLines(HWND hwndEdit, int count);
void changeToEndOfLine(HWND hwndEdit, int count);
void applyOperatorToMotion(HWND hwndEdit, char op, char motion, int count);
void doMotion(HWND hwndEdit, char motion, int count);
std::pair<int, int> findWordBounds(HWND hwndEdit, int pos);
void applyTextObject(HWND hwndEdit, char op, char modifier, char object);
void deleteLineOnce(HWND hwndEdit);
void yankLineOnce(HWND hwndEdit);
void repeatLastOp(HWND hwndEdit);
void performSearch(HWND hwndEdit, const std::string& searchTerm, bool useRegex = false);
void searchNext(HWND hwndEdit);
void searchPrevious(HWND hwndEdit);
void showCurrentMatchPosition(HWND hwndEdit);
void updateSearchHighlight(HWND hwndEdit, const std::string& searchTerm, bool useRegex);
void openTutor();
void handleCommand(HWND hwndEdit);
void handleNormalKey(HWND hwndEdit, char c);

// Text object functions
int findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar);
std::pair<int, int> findQuoteBounds(HWND hwndEdit, int pos, char quoteChar);
std::pair<int, int> getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner);
void applyTextObjectOperation(HWND hwndEdit, char op, bool inner, char object);
void performTextObjectOperation(HWND hwndEdit, char op, int start, int end);
void handleTextObjectCommand(HWND hwndEdit, char op, char modifier, char object);
void handleWordTextObject(HWND hwndEdit, char op, bool inner);

HWND getCurrentScintillaHandle() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void setStatus(const TCHAR* msg) {
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)msg);
}

void recordLastOp(LastOpType type, int count = 1, char motion = 0, char searchChar = 0) {
    state.lastOp.type = type;
    state.lastOp.count = count;
    state.lastOp.motion = motion;
    state.lastOp.searchChar = searchChar;
}

void clearSearchHighlights(HWND hwndEdit) {
    if (!hwndEdit) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
    ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);
}

void enterNormalMode() {
    HWND hwndEdit = getCurrentScintillaHandle();
    state.mode = NORMAL;
    state.isLineVisual = false;
    state.visualAnchor = -1;
    state.visualAnchorLine = -1;
    state.repeatCount = 0;
    state.opPending = 0;
    state.textObjectPending = 0;
    state.replacePending = false;
    state.lastSearchMatchCount = -1;
    setStatus(TEXT("-- NORMAL --"));
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);

    clearSearchHighlights(hwndEdit);
}

void enterInsertMode() {
    HWND hwndEdit = getCurrentScintillaHandle();
    state.mode = INSERT;
    state.isLineVisual = false;
    state.visualAnchor = -1;
    state.visualAnchorLine = -1;
    state.repeatCount = 0;
    state.opPending = 0;
    state.textObjectPending = 0;
    setStatus(TEXT("-- INSERT --"));
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}

void setVisualSelection(HWND hwndEdit) {
    if (state.mode != VISUAL) return;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    if (state.isLineVisual) {
        int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
        int anchorLine = state.visualAnchorLine;

        int startLine = (std::min)(anchorLine, currentLine);
        int endLine = (std::max)(anchorLine, currentLine);

        int startPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        int endPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);

        int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        if (endLine < totalLines - 1) {
            endPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0);
        }
        else {
            endPos = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        }
        ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
    }
    else {
        ::SendMessage(hwndEdit, SCI_SETANCHOR, state.visualAnchor, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);
    }
}

void enterVisualCharMode(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = false;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchor = caret;
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);

    setStatus(TEXT("-- VISUAL --"));
    setVisualSelection(hwndEdit);
}

void enterVisualLineMode(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = true;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    state.visualAnchor = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);

    setStatus(TEXT("-- VISUAL LINE --"));
    setVisualSelection(hwndEdit);
}

void enterCommandMode(char prompt) {
    state.commandMode = true;
    state.commandBuffer.clear();
    state.commandBuffer.push_back(prompt);
    std::wstring display(1, prompt);
    setStatus(display.c_str());
}

void exitCommandMode() {
    state.commandMode = false;
    state.commandBuffer.clear();
    enterNormalMode();
}

void updateCommandStatus() {
    std::wstring display(state.commandBuffer.begin(), state.commandBuffer.end());

    if (state.lastSearchMatchCount >= 0) {
        const int totalWidth = 80; // Approximate status bar width
        int commandLength = display.length();
        int availableSpace = totalWidth - commandLength - 20; // Reserve space for match info

        // Add match info on the right
        std::wstring matchInfo;
        if (state.lastSearchMatchCount > 0) {
            matchInfo = L"Found " + std::to_wstring(state.lastSearchMatchCount) + L" match";
            if (state.lastSearchMatchCount > 1) matchInfo += L"es";
        }
        else {
            matchInfo = L"Pattern not found";
        }

        // Add the match info with some spacing
        int rightPadding = totalWidth - display.length() - matchInfo.length();
        if (rightPadding > 0) {
            display += std::wstring(rightPadding, L' ');
        }
        display += matchInfo;
    }

    setStatus(display.c_str());
}

void Tab_next(HWND hwndEdit, int count) {
    // Get current tab index
    int currentIndex = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDOCINDEX, 0, 0);
    // Get total number of tabs
    int totalTabs = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, 0);

    // Calculate target index
    int targetIndex = (currentIndex + count) % totalTabs;
    if (targetIndex < 0) targetIndex += totalTabs;

    // Switch to target tab
    ::SendMessage(nppData._nppHandle, NPPM_ACTIVATEDOC, 0, targetIndex);
}

void Tab_previous(HWND hwndEdit, int count) {
    int currentIndex = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDOCINDEX, 0, 0);
    int totalTabs = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, 0);

    int targetIndex = (currentIndex - count) % totalTabs;
    if (targetIndex < 0) targetIndex += totalTabs;

    ::SendMessage(nppData._nppHandle, NPPM_ACTIVATEDOC, 0, targetIndex);
}

void DoMotion_char_left(HWND hwndEdit, int count) {
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    for (int i = 0; i < count; i++) {
        if (caret > 0) caret--;
    }
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);

    if (state.mode == NORMAL) {
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_char_right(HWND hwndEdit, int count) {
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    for (int i = 0; i < count; i++) {
        if (caret < docLen) caret++;
    }
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);

    if (state.mode == NORMAL) {
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_line_up(HWND hwndEdit, int count) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0);
    }

    if (state.mode == VISUAL) {
        int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    }
}

void DoMotion_line_down(HWND hwndEdit, int count) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0);
    }

    if (state.mode == VISUAL) {
        int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    }
}

void DoMotion_end_word_prev(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDLEFTEND, 0, 0);
    }

    if (state.mode == NORMAL) {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_next_char(HWND hwndEdit, int count, char searchChar) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    for (int i = 0; i < count; i++) {
        int newPos = pos + 1;
        while (newPos < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, newPos, 0);
            if (ch == searchChar) {
                pos = newPos;
                break;
            }
            newPos++;
        }
        if (newPos >= docLen) break;
    }

    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);

    if (state.mode == NORMAL) {
        ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_prev_char(HWND hwndEdit, int count, char searchChar) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    for (int i = 0; i < count; i++) {
        int newPos = pos - 1;
        while (newPos >= 0) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, newPos, 0);
            if (ch == searchChar) {
                pos = newPos;
                break;
            }
            newPos--;
        }
        if (newPos < 0) break;
    }

    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);

    if (state.mode == NORMAL) {
        ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_word_right(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0);
    }

    if (state.mode == NORMAL) {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_word_left(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0);
    }

    if (state.mode == NORMAL) {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_end_word(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDRIGHTEND, 0, 0);
    }

    if (state.mode == NORMAL) {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_end_line(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
    }

    if (state.mode == NORMAL) {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void DoMotion_line_start(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
    }

    if (state.mode == NORMAL) {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
    else if (state.mode == VISUAL) {
        setVisualSelection(hwndEdit);
    }
}

void deleteToEndOfLine(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    for (int i = 0; i < count; i++) {
        int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
        int endPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        if (pos < endPos) {
            ::SendMessage(hwndEdit, SCI_SETSEL, pos, endPos);
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        }
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    recordLastOp(OP_MOTION, count, 'D');
}

void joinLines(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    int startLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
        (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
    int endLine = (std::min)(startLine + count - 1,
        (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0) - 1);

    for (int line = startLine; line <= endLine; line++) {
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
        int nextLineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line + 1, 0);

        if (line + 1 < (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0)) {
            ::SendMessage(hwndEdit, SCI_SETSEL, lineEnd, nextLineStart);
            ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)" ");
        }
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
}

void changeToEndOfLine(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    for (int i = 0; i < count; i++) {
        int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
        int endPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        if (pos < endPos) {
            ::SendMessage(hwndEdit, SCI_SETSEL, pos, endPos);
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        }
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    enterInsertMode();
}

void applyOperatorToMotion(HWND hwndEdit, char op, char motion, int count) {
    int start = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    switch (motion) {
    case 'h': DoMotion_char_left(hwndEdit, count); break;
    case 'l': DoMotion_char_right(hwndEdit, count); break;
    case 'j': DoMotion_line_down(hwndEdit, count); break;
    case 'k': DoMotion_line_up(hwndEdit, count); break;
    case 'w': case 'W': DoMotion_word_right(hwndEdit, count); break;
    case 'b': case 'B': DoMotion_word_left(hwndEdit, count); break;
    case 'e': case 'E': DoMotion_end_word(hwndEdit, count); break;
    case 'g': case 'G': DoMotion_end_word_prev(hwndEdit, count); break;
    case '$': DoMotion_end_line(hwndEdit, count); break;
    case '^': DoMotion_line_start(hwndEdit, count); break;
    default: break;
    }

    int end = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    if (start > end) std::swap(start, end);

    if (motion == 'e' || motion == 'E') {
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (end < docLen) end++;
    }

    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    switch (op) {
    case 'd':
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
        recordLastOp(OP_MOTION, count, motion);
        break;
    case 'y':
        ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
        recordLastOp(OP_MOTION, count, motion);
        break;
    case 'c':
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        enterInsertMode();
        recordLastOp(OP_MOTION, count, motion);
        break;
    }
}

void doMotion(HWND hwndEdit, char motion, int count) {
    if (count <= 0) count = 1;

    switch (motion) {
    case 'h': DoMotion_char_left(hwndEdit, count); break;
    case 'l': DoMotion_char_right(hwndEdit, count); break;
    case 'j': DoMotion_line_down(hwndEdit, count); break;
    case 'k': DoMotion_line_up(hwndEdit, count); break;
    case 'w': case 'W': DoMotion_word_right(hwndEdit, count); break;
    case 'b': case 'B': DoMotion_word_left(hwndEdit, count); break;
    case 'e': case 'E': DoMotion_end_word(hwndEdit, count); break;
    case '$': DoMotion_end_line(hwndEdit, count); break;
    case '^': DoMotion_line_start(hwndEdit, count); break;
    case 'H':
        for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINESCROLL, 0, 0);
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case 'L':
        for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_LINESCROLL, 0,
            ::SendMessage(hwndEdit, SCI_LINESONSCREEN, 0, 0) - 1);
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case '{':
        for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_PARAUP, 0, 0);
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case '}':
        for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_PARADOWN, 0, 0);
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case '%':
        ::SendMessage(hwndEdit, SCI_BRACEMATCH, 0, 0);
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case 'G':
        if (count == 1) {
            ::SendMessage(hwndEdit, SCI_DOCUMENTEND, 0, 0);
        }
        else {
            ::SendMessage(hwndEdit, SCI_GOTOLINE, count - 1, 0);
        }
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case 'g':
        if (state.opPending == 'g') {
            if (count == 1) {
                ::SendMessage(hwndEdit, SCI_DOCUMENTSTART, 0, 0);
            }
            else {
                ::SendMessage(hwndEdit, SCI_GOTOLINE, count - 1, 0);
            }
            state.opPending = 0;
            if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        }
        break;
    case 'J':
        ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
        for (int i = 0; i < count; ++i) {
            int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
            int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
            if (line >= totalLines - 1) break;

            int endOfCurrentLine = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
            int startOfNextLine = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line + 1, 0);

            ::SendMessage(hwndEdit, SCI_SETSEL, endOfCurrentLine, startOfNextLine);
            ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)" ");
        }
        ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
        break;
    default:
        break;
    }
}

std::pair<int, int> findWordBounds(HWND hwndEdit, int pos) {
    int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
    int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
    return { start, end };
}
// Text Object Functions implementation
int findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    if (pos >= docLen) return -1;

    char currentChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);

    // If on closing bracket, search backwards
    if (currentChar == closeChar) {
        int depth = 1;
        for (int i = pos - 1; i >= 0; i--) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (ch == closeChar) depth++;
            else if (ch == openChar) {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    // If on opening bracket
    else {
        int depth = 1;
        for (int i = pos + 1; i < docLen; i++) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (ch == openChar) depth++;
            else if (ch == closeChar) {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    return -1;
}

std::pair<int, int> findQuoteBounds(HWND hwndEdit, int pos, char quoteChar) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int start = -1, end = -1;

    // Search backwards for opening quote
    for (int i = pos; i >= 0; i--) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
        if (ch == quoteChar) {
            start = i;
            break;
        }
    }

    // Search forwards for closing quote
    if (start != -1) {
        for (int i = start + 1; i < docLen; i++) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (ch == quoteChar) {
                end = i;
                break;
            }
        }
    }

    return { start, end };
}

void handleWordTextObject(HWND hwndEdit, char op, bool inner) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    auto bounds = findWordBounds(hwndEdit, pos);

    if (bounds.first == bounds.second) {
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (pos < docLen) {
            bounds = findWordBounds(hwndEdit, pos + 1);
        }
    }

    if (bounds.first == bounds.second) return;

    int start = bounds.first;
    int end = bounds.second;
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    if (!inner) {
        while (start > 0) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0);
            if (!std::isspace(static_cast<unsigned char>(ch))) break;
            start--;
        }

        while (end < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0);
            if (!std::isspace(static_cast<unsigned char>(ch))) {
                if (end > bounds.second) {
                    break;
                }
            }
            end++;
        }
    }
    else {
        start = bounds.first;
        end = bounds.second;
    }

    if (start < end) {
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        performTextObjectOperation(hwndEdit, op, start, end);
    }
}
std::pair<int, int> getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int start = pos, end = pos;

    switch (objType) {
    case TEXT_OBJECT_WORD: {
        auto bounds = findWordBounds(hwndEdit, pos);
        start = bounds.first;
        end = bounds.second;
        if (inner && start < end) {
            // For inner word, exclude surrounding whitespace
            int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

            // Trim left whitespace
            while (start < end) {
                char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start, 0);
                if (!std::isspace(static_cast<unsigned char>(ch))) break;
                start++;
            }

            // Trim right whitespace
            while (end > start) {
                char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end - 1, 0);
                if (!std::isspace(static_cast<unsigned char>(ch))) break;
                end--;
            }
        }
        break;
    }

    case TEXT_OBJECT_BIG_WORD: {
        start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
        end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
        break;
    }

    case TEXT_OBJECT_PAREN: {
        int match = findMatchingBracket(hwndEdit, pos, '(', ')');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                // Find the closing paren
                int closeMatch = findMatchingBracket(hwndEdit, start, '(', ')');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                // Find the opening paren
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '(', ')');
                if (openMatch != -1) start = openMatch;
            }

            if (inner && start < end) {
                start++;
                end--;
            }
        }
        break;
    }

    case TEXT_OBJECT_BRACE: {
        int match = findMatchingBracket(hwndEdit, pos, '{', '}');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                int closeMatch = findMatchingBracket(hwndEdit, start, '{', '}');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '{', '}');
                if (openMatch != -1) start = openMatch;
            }

            if (inner && start < end) {
                start++;
                end--;
            }
        }
        break;
    }

    case TEXT_OBJECT_BRACKET: {
        int match = findMatchingBracket(hwndEdit, pos, '[', ']');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                int closeMatch = findMatchingBracket(hwndEdit, start, '[', ']');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '[', ']');
                if (openMatch != -1) start = openMatch;
            }

            if (inner && start < end) {
                start++;
                end--;
            }
        }
        break;
    }

    case TEXT_OBJECT_ANGLE: {
        int match = findMatchingBracket(hwndEdit, pos, '<', '>');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                int closeMatch = findMatchingBracket(hwndEdit, start, '<', '>');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '<', '>');
                if (openMatch != -1) start = openMatch;
            }

            if (inner && start < end) {
                start++;
                end--;
            }
        }
        break;
    }

    default:
        break;
    }

    return { start, end };
}

void performTextObjectOperation(HWND hwndEdit, char op, int start, int end) {
    switch (op) {
    case 'd':
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
        recordLastOp(OP_MOTION, state.repeatCount, 'd');
        break;

    case 'c':
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        enterInsertMode();
        recordLastOp(OP_MOTION, state.repeatCount, 'c');
        break;

    case 'y':
        ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
        recordLastOp(OP_MOTION, state.repeatCount, 'y');
        break;
    }
}

void applyTextObjectOperation(HWND hwndEdit, char op, bool inner, char object) {
    TextObjectType objType;
    char quoteChar = 0;

    // Map character to text object type
    switch (object) {
    case 'w':
        handleWordTextObject(hwndEdit, op, inner);
        return;
    case 'W': objType = TEXT_OBJECT_BIG_WORD; break;
    case '\'':
        quoteChar = '\'';
        break;
    case '"':
        quoteChar = '"';
        break;
    case '`':
        quoteChar = '`';
        break;
    case '(': case ')': objType = TEXT_OBJECT_PAREN; break;
    case '[': case ']': objType = TEXT_OBJECT_BRACKET; break;
    case '{': case '}': objType = TEXT_OBJECT_BRACE; break;
    case '<': case '>': objType = TEXT_OBJECT_ANGLE; break;
    default: return;
    }

    // Special handling for quotes
    if (quoteChar != 0) {
        auto bounds = findQuoteBounds(hwndEdit,
            (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), quoteChar);
        if (bounds.first == -1 || bounds.second == -1) return;

        int start = bounds.first;
        int end = bounds.second + 1;

        if (inner) {
            start++;
            end--;
        }

        if (start < end) {
            ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
            performTextObjectOperation(hwndEdit, op, start, end);
        }
        return;
    }

    // For other text objects
    auto bounds = getTextObjectBounds(hwndEdit, objType, inner);
    if (bounds.first < bounds.second) {
        ::SendMessage(hwndEdit, SCI_SETSEL, bounds.first, bounds.second);
        performTextObjectOperation(hwndEdit, op, bounds.first, bounds.second);
    }
}

void handleTextObjectCommand(HWND hwndEdit, char op, char modifier, char object) {
    bool inner = (modifier == 'i');
    applyTextObjectOperation(hwndEdit, op, inner, object);
}

void applyTextObject(HWND hwndEdit, char op, char modifier, char object) {
    handleTextObjectCommand(hwndEdit, op, modifier, object);
}

void deleteLineOnce(HWND hwndEdit) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int end = (line < totalLines - 1)
        ? (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line + 1, 0)
        : (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
}

void yankLineOnce(HWND hwndEdit) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int end = (line < totalLines - 1)
        ? (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line + 1, 0)
        : (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
    ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
}

void repeatLastOp(HWND hwndEdit) {
    if (state.lastOp.type == OP_NONE) return;

    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    int count = state.repeatCount > 0 ? state.repeatCount : state.lastOp.count;

    switch (state.lastOp.type) {
    case OP_DELETE_LINE:
        for (int i = 0; i < count; ++i) deleteLineOnce(hwndEdit);
        break;

    case OP_YANK_LINE:
        for (int i = 0; i < count; ++i) yankLineOnce(hwndEdit);
        break;

    case OP_PASTE_LINE:
        for (int i = 0; i < count; ++i) ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
        break;

    case OP_MOTION:
        // Handle character search repetition
        if (state.lastOp.motion == 'f' || state.lastOp.motion == 'F') {
            if (state.lastOp.searchChar != 0) {
                if (state.lastOp.motion == 'f') {
                    DoMotion_next_char(hwndEdit, count, state.lastOp.searchChar);
                }
                else {
                    DoMotion_prev_char(hwndEdit, count, state.lastOp.searchChar);
                }
            }
        }
        // Handle search repetition
        else if (state.lastOp.motion == 'n' || state.lastOp.motion == 'N') {
            if (state.lastOp.motion == 'n') {
                searchNext(hwndEdit);
            }
            else {
                searchPrevious(hwndEdit);
            }
        }
        // Handle word search repetition
        else if (state.lastOp.motion == '*' || state.lastOp.motion == '#') {
            if (!state.lastSearchTerm.empty()) {
                if (state.lastOp.motion == '*') {
                    searchNext(hwndEdit);
                }
                else {
                    searchPrevious(hwndEdit);
                }
            }
        }
        // Handle delete character operations
        else if (state.lastOp.motion == 'x' || state.lastOp.motion == 'X') {
            for (int i = 0; i < count; ++i) {
                if (state.lastOp.motion == 'x') {
                    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                    int nextPos = pos + 1;
                    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
                    if (nextPos <= docLen) {
                        ::SendMessage(hwndEdit, SCI_SETSEL, pos, nextPos);
                        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                    }
                }
                else {
                    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                    if (pos > 0) {
                        ::SendMessage(hwndEdit, SCI_SETSEL, pos - 1, pos);
                        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                    }
                }
            }
        }

        else {
            applyOperatorToMotion(hwndEdit, 'd', state.lastOp.motion, count);
        }
        break;

    case OP_REPLACE:
        state.replacePending = true;
        break;
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    state.repeatCount = 0; // Reset repeat count after repetition
}

void updateSearchHighlight(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (searchTerm.empty()) {
        // Clear highlights if search term is empty
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
        ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);
        return;
    }

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    // Clear all indicators first
    ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
    ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);

    // Setup indicator for highlighting
    ::SendMessage(hwndEdit, SCI_INDICSETSTYLE, 0, INDIC_ROUNDBOX);
    ::SendMessage(hwndEdit, SCI_INDICSETFORE, 0, RGB(255, 255, 0));
    ::SendMessage(hwndEdit, SCI_INDICSETALPHA, 0, 100);
    ::SendMessage(hwndEdit, SCI_INDICSETOUTLINEALPHA, 0, 255);

    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    int pos = 0;
    int matchCount = 0;
    int firstMatch = -1;

    while (pos < docLen) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

        if (found == -1) break;

        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

        ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
        ::SendMessage(hwndEdit, SCI_INDICATORFILLRANGE, start, end - start);

        if (firstMatch == -1) {
            firstMatch = start;
        }

        matchCount++;
        pos = end;

        if (end <= start) {
            pos = start + 1;
        }
    }

    state.lastSearchMatchCount = matchCount;
}

void performSearch(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (searchTerm.empty()) return;

    state.lastSearchTerm = searchTerm;
    state.useRegex = useRegex;

    updateSearchHighlight(hwndEdit, searchTerm, useRegex);

    // Move to first match if found
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found != -1) {
        int s = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, s, e);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
    }
}

void searchNext(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1) {
        // Wrap around to beginning
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1) {
            setStatus(L"Search hit BOTTOM, continuing at TOP");
        }
    }

    if (found != -1) {
        int s = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, s, e);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

        // Show current match position and total count
        showCurrentMatchPosition(hwndEdit);
    }
    else {
        setStatus(L"Pattern not found");
    }
}

void searchPrevious(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    if (startPos > 0) startPos--;

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    // Search backwards from current position to beginning
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, 0, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1) {
        // Wrap around to end
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, docLen, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1) {
            setStatus(L"Search hit TOP, continuing at BOTTOM");
        }
    }

    if (found != -1) {
        int s = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, s, e);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

        // Show current match position and total count
        showCurrentMatchPosition(hwndEdit);
    }
    else {
        setStatus(L"Pattern not found");
    }
}

void showCurrentMatchPosition(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    // Count total matches
    int totalMatches = 0;
    int pos = 0;
    while (pos < docLen) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found == -1) break;

        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

        totalMatches++;
        pos = end;

        if (end <= start) {
            pos = start + 1;
        }
    }

    // Find current match index
    int currentMatchIndex = 0;
    pos = 0;
    int matchCount = 0;
    while (pos < docLen) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found == -1) break;

        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

        matchCount++;

        // Check if current position is within this match
        if (currentPos >= start && currentPos <= end) {
            currentMatchIndex = matchCount;
            break;
        }

        pos = end;
        if (end <= start) {
            pos = start + 1;
        }
    }

    if (totalMatches > 0 && currentMatchIndex > 0) {
        std::string status = "Match " + std::to_string(currentMatchIndex) + " of " + std::to_string(totalMatches);
        std::wstring wstatus(status.begin(), status.end());
        setStatus(wstatus.c_str());
    }
}

void openTutor() {
    TCHAR nppPath[MAX_PATH] = { 0 };
    ::SendMessage(nppData._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, (LPARAM)nppPath);
    std::wstring tutorPath = std::wstring(nppPath) + L"\\plugins\\NppVim\\tutor.txt";
    ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)tutorPath.c_str());
}

void handleCommand(HWND hwndEdit) {
    if (state.commandBuffer.empty()) return;

    const std::string& buf = state.commandBuffer;

    if (buf[0] == '/' && buf.size() > 1) {
        std::string searchTerm = buf.substr(1);
        performSearch(hwndEdit, searchTerm, false);
    }
    else if (buf == ":tutor") {
        openTutor();
    }
    else if (buf[0] == ':' && buf.size() > 1) {
        std::string cmd = buf.substr(1);

        bool isNumber = true;
        for (char ch : cmd) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                isNumber = false;
                break;
            }
        }

        if (isNumber && !cmd.empty()) {
            int lineNum = std::stoi(cmd);
            if (lineNum > 0) {
                ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
                std::wstring msg = L"Line " + std::to_wstring(lineNum);
                setStatus(msg.c_str());
            }
        }
        else if (cmd.size() > 2 && cmd[0] == 's' && cmd[1] == ' ') {
            std::string pattern = cmd.substr(2);
            performSearch(hwndEdit, pattern, true);
        }
        else if (cmd == "w") {
            ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
            setStatus(L"File saved");
        }
        else if (cmd == "q") {
            ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
        }
        else if (cmd == "wq") {
            ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
            ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
        }
        else {
            std::wstring wcmd(cmd.begin(), cmd.end());
            std::wstring msg = L"Not an editor command: " + wcmd;
            setStatus(msg.c_str());
        }
    }

    state.commandMode = false;
    state.commandBuffer.clear();
}

std::string getClipboardText(HWND hwnd) {
    std::string text;
    if (!OpenClipboard(hwnd)) return text;

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char* pszText = static_cast<char*>(GlobalLock(hData));
        if (pszText) {
            text = pszText;
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return text;
}

void handleNormalKey(HWND hwndEdit, char c) {
    // Handle digits for repeat counts
    if (std::isdigit(static_cast<unsigned char>(c))) {
        int digit = c - '0';
        if (c == '0' && state.repeatCount == 0) {
            ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
            if (state.mode == VISUAL) setVisualSelection(hwndEdit);
            return;
        }
        state.repeatCount = state.repeatCount * 10 + digit;
        return;
    }

    int count = (state.repeatCount > 0) ? state.repeatCount : 1;

    // Handle replace character
    if (state.replacePending) {
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

        // Record for repetition
        state.lastOp.type = OP_REPLACE;
        state.lastOp.count = 1;
        state.lastOp.motion = 'r';
        return;
    }

    // Handle character search after 'f' or 'F' - THIS IS THE KEY FIX
    if (state.textObjectPending == 'f' && (state.opPending == 'f' || state.opPending == 'F')) {
        char searchChar = c;
        int originalPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        bool found = false;

        if (state.opPending == 'f') {
            int newPos = originalPos;
            for (int i = 0; i < count; i++) {
                int tempPos = newPos + 1;
                int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
                while (tempPos < docLen) {
                    char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, tempPos, 0);
                    if (ch == searchChar) {
                        newPos = tempPos;
                        found = true;
                        break;
                    }
                    tempPos++;
                }
                if (!found) break;
            }
            if (found) {
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
                if (state.mode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_SETSEL, newPos, newPos);
                }
                else if (state.mode == VISUAL) {
                    setVisualSelection(hwndEdit);
                }
            }
        }
        else if (state.opPending == 'F') {
            int newPos = originalPos;
            for (int i = 0; i < count; i++) {
                int tempPos = newPos - 1;
                while (tempPos >= 0) {
                    char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, tempPos, 0);
                    if (ch == searchChar) {
                        newPos = tempPos;
                        found = true;
                        break;
                    }
                    tempPos--;
                }
                if (!found) break;
            }
            if (found) {
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
                if (state.mode == NORMAL) {
                    ::SendMessage(hwndEdit, SCI_SETSEL, newPos, newPos);
                }
                else if (state.mode == VISUAL) {
                    setVisualSelection(hwndEdit);
                }
            }
        }

        if (found) {
            state.lastSearchChar = searchChar;
            state.lastSearchForward = (state.opPending == 'f');
            recordLastOp(OP_MOTION, count, state.opPending, searchChar);
        }
        else {
            setStatus(TEXT("Character not found"));
        }

        // ALWAYS reset these states regardless of whether character was found
        state.opPending = 0;
        state.textObjectPending = 0;
        state.repeatCount = 0;
        return;
    }

    // Handle text object completion
    if (state.textObjectPending && state.opPending &&
        (c == 'w' || c == 'W' || c == '"' || c == '\'' || c == '`' ||
            c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '<' || c == '>' ||
            c == 's' || c == 'p')) {

        handleTextObjectCommand(hwndEdit, state.opPending, state.textObjectPending, c);

        state.opPending = 0;
        state.textObjectPending = 0;
        state.repeatCount = 0;
        return;
    }

    // Handle operator + text object modifier (i/a)
    if (state.opPending && (c == 'i' || c == 'a')) {
        state.textObjectPending = c;
        state.repeatCount = 0;
        return;
    }

    // Handle operator + motion combinations
    if (state.mode == NORMAL) {
        // Handle double operator commands (dd, yy, cc)
        if ((c == 'd' || c == 'y' || c == 'c') && state.opPending == c) {
            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            for (int i = 0; i < count; ++i) {
                if (c == 'd') {
                    deleteLineOnce(hwndEdit);
                }
                else if (c == 'y') yankLineOnce(hwndEdit);
                else if (c == 'c') {
                    deleteLineOnce(hwndEdit);
                    enterInsertMode();
                }
            }
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

            if (c == 'd') recordLastOp(OP_DELETE_LINE, count);
            else if (c == 'y') recordLastOp(OP_YANK_LINE, count);
            else if (c == 'c') recordLastOp(OP_MOTION, count, 'c');

            state.opPending = 0;
            state.repeatCount = 0;
            return;
        }
        else if (c == 'd' || c == 'y' || c == 'c') {
            state.opPending = c;
            state.repeatCount = 0;
            return;
        }

        // Handle f/F character search motions
        if (c == 'f' || c == 'F') {
            state.opPending = c;
            state.textObjectPending = 'f';
            state.repeatCount = 0;
            return;
        }

        // Handle g motions
        if (c == 'g') {
            if (state.opPending != 'g') {
                state.opPending = 'g';
                state.repeatCount = 0;
            }
            else {
                doMotion(hwndEdit, 'g', count);
                state.opPending = 0;
                state.repeatCount = 0;
            }
            return;
        }

        // Handle gt and gT tab navigation
        if (state.opPending == 'g') {
            if (c == 't') {
                Tab_next(hwndEdit, count);
                state.opPending = 0;
                state.repeatCount = 0;
                return;
            }
            else if (c == 'T') {
                Tab_previous(hwndEdit, count);
                state.opPending = 0;
                state.repeatCount = 0;
                return;
            }
        }

        // Handle motion with pending operator (EXCLUDE f/F since they're handled separately)
        if (state.opPending && state.opPending != 'f' && state.opPending != 'F' &&
            (c == 'w' || c == 'W' || c == '$' || c == 'e' || c == 'E' || c == 'b' || c == 'B' ||
                c == 'h' || c == 'l' || c == 'j' || c == 'k' || c == '^' || c == 'H' || c == 'L' ||
                c == '{' || c == '}' || c == 'G' || c == '%' || c == 'g' || c == '0'))
        {
            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            applyOperatorToMotion(hwndEdit, state.opPending, c, count);
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
            state.opPending = 0;
            state.repeatCount = 0;
            return;
        }
    }

    // Handle visual mode operations
    if (state.mode == VISUAL) {
        switch (c) {
        case 'd':
        case 'x':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            recordLastOp(OP_MOTION, count, 'd'); // Record for repetition
            enterNormalMode();
            state.repeatCount = 0;
            return;
        case 'y': {
            int start = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
            int end = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
            ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
            recordLastOp(OP_MOTION, count, 'y');
            enterNormalMode();
            state.repeatCount = 0;
            return;
        }
        case 'c':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            recordLastOp(OP_MOTION, count, 'c');
            enterInsertMode();
            state.repeatCount = 0;
            return;
        case 'v':
            if (!state.isLineVisual) {
                enterNormalMode();
            }
            else {
                enterVisualCharMode(hwndEdit);
            }
            state.repeatCount = 0;
            return;
        case 'V':
            if (state.isLineVisual) {
                enterNormalMode();
            }
            else {
                enterVisualLineMode(hwndEdit);
            }
            state.repeatCount = 0;
            return;
        case 'o':
        {
            int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);

            // Swap current position and anchor
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, anchor, 0);
            ::SendMessage(hwndEdit, SCI_SETANCHOR, currentPos, 0);

            // Update visual anchor state
            if (state.isLineVisual) {
                state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
                state.visualAnchor = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
            }
            else {
                state.visualAnchor = anchor;
                state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
            }
        }
        state.repeatCount = 0;
        return;
        }

        if (c == 'g') {
            if (state.opPending != 'g') {
                state.opPending = 'g';
                state.repeatCount = 0;
            }
            else {
                doMotion(hwndEdit, 'g', count);
                state.opPending = 0;
                state.repeatCount = 0;
            }
            return;
        }
    }

    // Reset state for new command ONLY if no pending operations
    if (!state.opPending && !state.textObjectPending) {
        state.repeatCount = 0;
    }

    // Normal single-key commands
    if (!state.opPending && !state.textObjectPending) {
        switch (c) {
        case 'h': case 'l': case 'j': case 'k':
        case 'w': case 'W': case 'b': case 'B':
        case 'e': case 'E': case '$': case '^':
        case 'H': case 'L': case '{': case '}':
        case '%': case 'g': case 'G':
        case '0': case '-': case '+':
            doMotion(hwndEdit, c, count);
            recordLastOp(OP_MOTION, count, c);
            break;

        case 'i':
            enterInsertMode();
            break;

        case 'a':
            doMotion(hwndEdit, 'l', 1);
            enterInsertMode();
            break;

        case 'A':
            ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
            enterInsertMode();
            break;

        case 'I':
            ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
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
            doMotion(hwndEdit, 'k', 1);
            enterInsertMode();
            break;

        case 'u':
            ::SendMessage(hwndEdit, SCI_UNDO, 0, 0);
            recordLastOp(OP_MOTION, 1, 'u');
            break;

        case 'r':
            state.replacePending = true;
            recordLastOp(OP_REPLACE, count, 'r');
            break;

        case 'R':
            state.mode = INSERT;
            state.isLineVisual = false;
            state.visualAnchor = -1;
            state.visualAnchorLine = -1;
            state.repeatCount = 0;
            state.opPending = 0;
            state.textObjectPending = 0;
            setStatus(TEXT("-- REPLACE --"));
            ::SendMessage(hwndEdit, SCI_SETOVERTYPE, true, 0);
            ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
            recordLastOp(OP_MOTION, count, 'R'); // Record replace mode
            break;

        case '.':
            repeatLastOp(hwndEdit);
            break;

        case 'v':
            if (state.mode == NORMAL) {
                enterVisualCharMode(hwndEdit);
            }
            else if (state.mode == VISUAL) {
                if (state.isLineVisual) {
                    enterVisualCharMode(hwndEdit);
                }
                else {
                    enterNormalMode();
                }
            }
            break;

        case 'V':
            if (state.mode == NORMAL) {
                enterVisualLineMode(hwndEdit);
            }
            else if (state.mode == VISUAL) {
                if (state.isLineVisual) {
                    enterNormalMode();
                }
                else {
                    enterVisualLineMode(hwndEdit);
                }
            }
            break;

        case 'p': { // Paste after cursor
            std::string clipText = getClipboardText(hwndEdit);
            if (clipText.empty()) break;

            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            for (int i = 0; i < count; ++i) {
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                int insertPos = (int)::SendMessage(hwndEdit, SCI_POSITIONAFTER, pos, 0);
                ::SendMessage(hwndEdit, SCI_INSERTTEXT, insertPos, (LPARAM)clipText.c_str());
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, insertPos + (int)clipText.size(), 0);
            }
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
            recordLastOp(OP_PASTE_LINE, count);
            break;
        }

        case 'P': { // Paste before cursor
            std::string clipText = getClipboardText(hwndEdit);
            if (clipText.empty()) break;

            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            for (int i = 0; i < count; ++i) {
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                ::SendMessage(hwndEdit, SCI_INSERTTEXT, pos, (LPARAM)clipText.c_str());
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos + (int)clipText.size(), 0);
            }
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
            recordLastOp(OP_PASTE_LINE, count);
            break;
        }

        case 'x':
            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            for (int i = 0; i < count; ++i) {
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                int nextPos = pos + 1;
                int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
                if (nextPos <= docLen) {
                    ::SendMessage(hwndEdit, SCI_SETSEL, pos, nextPos);
                    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                }
            }
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
            recordLastOp(OP_MOTION, count, 'x'); // Record for repetition
            break;

        case 'X':
            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            for (int i = 0; i < count; ++i) {
                int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
                if (pos > 0) {
                    ::SendMessage(hwndEdit, SCI_SETSEL, pos - 1, pos);
                    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
                }
            }
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
            recordLastOp(OP_MOTION, count, 'X'); // Record for repetition
            break;

        case 'D':
            deleteToEndOfLine(hwndEdit, count);
            recordLastOp(OP_MOTION, count, 'D');
            break;

        case 'J':
            joinLines(hwndEdit, count);
            recordLastOp(OP_MOTION, count, 'J');
            break;

        case 'C':
            changeToEndOfLine(hwndEdit, count);
            recordLastOp(OP_MOTION, count, 'C');
            break;

        case ';':
            if (state.lastSearchChar != 0) {
                if (state.lastSearchForward) {
                    DoMotion_next_char(hwndEdit, count, state.lastSearchChar);
                }
                else {
                    DoMotion_prev_char(hwndEdit, count, state.lastSearchChar);
                }
                recordLastOp(OP_MOTION, count, state.lastSearchForward ? 'f' : 'F', state.lastSearchChar);
            }
            break;

        case ',':
            if (state.lastSearchChar != 0) {
                if (state.lastSearchForward) {
                    DoMotion_prev_char(hwndEdit, count, state.lastSearchChar);
                }
                else {
                    DoMotion_next_char(hwndEdit, count, state.lastSearchChar);
                }
                recordLastOp(OP_MOTION, count, state.lastSearchForward ? 'F' : 'f', state.lastSearchChar);
            }
            break;

        case ':':
            enterCommandMode(':');
            break;

        case '/':
            enterCommandMode('/');
            break;

        case 'n':
            searchNext(hwndEdit);
            recordLastOp(OP_MOTION, count, 'n'); // Record search for repetition
            break;

        case 'N':
            searchPrevious(hwndEdit);
            recordLastOp(OP_MOTION, count, 'N'); // Record search for repetition
            break;

        case '*':
        {
            int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            auto bounds = findWordBounds(hwndEdit, pos);
            if (bounds.first != bounds.second) {
                int len = bounds.second - bounds.first;
                std::vector<char> word(len + 1);
                Sci_TextRangeFull textRange;
                textRange.chrg.cpMin = bounds.first;
                textRange.chrg.cpMax = bounds.second;
                textRange.lpstrText = word.data();
                ::SendMessage(hwndEdit, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&textRange);

                performSearch(hwndEdit, word.data(), false);
                searchNext(hwndEdit);
                recordLastOp(OP_MOTION, count, '*'); // Record word search for repetition
            }
        }
        break;

        case '#':
        {
            int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            auto bounds = findWordBounds(hwndEdit, pos);
            if (bounds.first != bounds.second) {
                int len = bounds.second - bounds.first;
                std::vector<char> word(len + 1);
                Sci_TextRangeFull textRange;
                textRange.chrg.cpMin = bounds.first;
                textRange.chrg.cpMax = bounds.second;
                textRange.lpstrText = word.data();
                ::SendMessage(hwndEdit, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&textRange);

                performSearch(hwndEdit, word.data(), false);
                searchPrevious(hwndEdit);
                recordLastOp(OP_MOTION, count, '#'); // Record word search for repetition
            }
        }
        break;

        default:
            break;
        }
    }
}

LRESULT CALLBACK ScintillaHookProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC orig = nullptr;
    auto it = origProcMap.find(hwnd);
    if (it != origProcMap.end()) orig = it->second;

    if (!state.vimEnabled || !orig) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    HWND hwndEdit = hwnd;

    if (state.commandMode) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                handleCommand(hwndEdit);
                enterNormalMode();
                return 0;
            }
            else if (wParam == VK_ESCAPE) {
                clearSearchHighlights(hwndEdit);
                state.lastSearchMatchCount = -1;  // Reset match count
                exitCommandMode();
                return 0;
            }
            else if (wParam == VK_BACK) {
                if (state.commandBuffer.size() > 1) {
                    state.commandBuffer.pop_back();
                    updateCommandStatus();

                    // Update search highlights in real-time for backspace
                    if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
                        // Regular search
                        std::string currentSearch = state.commandBuffer.substr(1);
                        updateSearchHighlight(hwndEdit, currentSearch, false);
                    }
                    else if (state.commandBuffer[0] == ':' && state.commandBuffer.size() > 3 &&
                        state.commandBuffer[1] == 's' && state.commandBuffer[2] == ' ') {
                        // Regex search
                        std::string currentPattern = state.commandBuffer.substr(3);
                        updateSearchHighlight(hwndEdit, currentPattern, true);
                    }
                    else if (state.commandBuffer.size() == 1) {
                        // Clear highlights when back to just prompt character
                        clearSearchHighlights(hwndEdit);
                        state.lastSearchMatchCount = -1;  // Reset match count
                    }
                }
                else {
                    // Clear search highlights when backspacing to just prompt
                    clearSearchHighlights(hwndEdit);
                    state.lastSearchMatchCount = -1;  // Reset match count
                    exitCommandMode();
                }
                return 0;
            }
        }

        if (msg == WM_CHAR) {
            char c = (char)wParam;
            if (c >= 32 && c != 10 && c != 13) {
                state.commandBuffer.push_back(c);
                updateCommandStatus();

                // Update search highlights in real-time
                if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
                    // Regular search
                    std::string currentSearch = state.commandBuffer.substr(1);
                    updateSearchHighlight(hwndEdit, currentSearch, false);
                }
                else if (state.commandBuffer[0] == ':' && state.commandBuffer.size() > 3) {
                    // Check if it's a regex search command
                    if (state.commandBuffer[1] == 's' && state.commandBuffer[2] == ' ') {
                        // Regex search
                        std::string currentPattern = state.commandBuffer.substr(3);
                        updateSearchHighlight(hwndEdit, currentPattern, true);
                    }
                }
            }
            return 0;
        }
        return CallWindowProc(orig, hwnd, msg, wParam, lParam);
    }

    if (msg == WM_CHAR) {
        char c = (char)wParam;

        if ((int)wParam == VK_ESCAPE) {
            if (state.mode == INSERT) {
                ::SendMessage(hwndEdit, SCI_SETOVERTYPE, false, 0);
            }
            enterNormalMode();
            return 0;
        }

        if (state.mode == INSERT) {
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
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

        if (state.mode == NORMAL || state.mode == VISUAL) {
            handleNormalKey(hwndEdit, c);
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
    state.vimEnabled = !state.vimEnabled;
    HMENU hMenu = (HMENU)::SendMessage(nppData._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0);
    if (hMenu) {
        ::CheckMenuItem(hMenu, funcItem[0]._cmdID,
            MF_BYCOMMAND | (state.vimEnabled ? MF_CHECKED : MF_UNCHECKED));
    }

    if (state.vimEnabled) {
        ensureScintillaHooks();
        enterNormalMode();
    }
    else {
        setStatus(TEXT(""));
        ::SendMessage(getCurrentScintillaHandle(), SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
        removeAllScintillaHooks();
    }
}

void about() {
    ::MessageBox(nppData._nppHandle,
        TEXT("A Notepad++ plugin that adds Vim-style editing and key bindings."),
        TEXT("NppVim Plugin"), MB_OK);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/) {
    if (reasonForCall == DLL_PROCESS_ATTACH) {
        g_hInstance = (HINSTANCE)hModule;
    }
    else if (reasonForCall == DLL_PROCESS_DETACH) {
        removeAllScintillaHooks();
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
    state.vimEnabled = true;
    ensureScintillaHooks();
    enterNormalMode();
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
        if (state.vimEnabled) ensureScintillaHooks();
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