// VisualMode.cpp
#include "../include/CommandMode.h"
#include "../include/Motion.h"
#include "../include/NormalMode.h"
#include "../include/NppVim.h"
#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../include/VisualMode.h"
#include "../plugin/menuCmdID.h"
//#include "../plugin/Notepad_plus_msgs.h"
//#include "../plugin/PluginInterface.h"
#include "../plugin/Scintilla.h"
#include <algorithm>
#include <cctype>

extern NormalMode* g_normalMode;
extern CommandMode* g_commandMode;
extern NppData nppData;

VisualMode::VisualMode(VimState& state) : state(state) {
    setupKeyHandlers();
}

bool VisualMode::iswalnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c));
}

void VisualMode::setupKeyHandlers() {
    keyHandlers['d'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['x'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['y'] = [this](HWND hwnd, int c) { handleYank(hwnd, c); };
    keyHandlers['o'] = [this](HWND hwnd, int c) { handleSwapAnchor(hwnd, c); };

    keyHandlers['v'] = [this](HWND hwnd, int c) { handleVisualChar(hwnd, c); };
    keyHandlers['V'] = [this](HWND hwnd, int c) { handleVisualLine(hwnd, c); };
    keyHandlers[22] = [this](HWND hwnd, int c) { handleVisualBlock(hwnd, c); };

    keyHandlers['H'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'H', c); };
    keyHandlers['L'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'L', c); };

    keyHandlers['I'] = [this](HWND hwnd, int c) { handleBlockInsert(hwnd, c); };
    keyHandlers['A'] = [this](HWND hwnd, int c) { handleBlockAppend(hwnd, c); };
    keyHandlers['i'] = [this](HWND hwnd, int c) {
    if (state.isBlockVisual) {
        return;
      }
      state.textObjectPending = 'i';
      Utils::setStatus(TEXT("-- inner text object --"));
  };

    keyHandlers['a'] = [this](HWND hwnd, int c) {
        if (state.isBlockVisual) {
            return;
        }
        state.textObjectPending = 'a';
        Utils::setStatus(TEXT("-- around text object --"));
    };

    keyHandlers['h'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'h', c); };
    keyHandlers['j'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'j', c); };
    keyHandlers['k'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'k', c); };
    keyHandlers['l'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'l', c); };
    keyHandlers['w'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'w', c); };
    keyHandlers['W'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'W', c); };
    keyHandlers['b'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'b', c); };
    keyHandlers['B'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'B', c); };
    keyHandlers['e'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'e', c); };
    keyHandlers['E'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'E', c); };
    keyHandlers['$'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '$', c); };
    keyHandlers['^'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '^', c); };
    keyHandlers['0'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '^', c); };
    keyHandlers['{'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '{', c); };
    keyHandlers['}'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '}', c); };
    keyHandlers['%'] = [this](HWND hwnd, int c) { motion.matchPair(hwnd); };
    keyHandlers['G'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'G', c); };
    keyHandlers['~'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '~', c); };
    keyHandlers['g'] = [this](HWND hwnd, int c) { handleGCommand(hwnd, c); };
    keyHandlers['c'] = [this](HWND hwnd, int c) {
        if (state.opPending == 'g') {
            handleToggleComment(hwnd, c);
            state.opPending = 0;
        }
        else {
            handleChange(hwnd, c);
        }
        };

    keyHandlers['<'] = [this](HWND hwnd, int c) {Utils::handleIndent(hwnd, c);};
    keyHandlers['>'] = [this](HWND hwnd, int c) {Utils::handleIndent(hwnd, c);};
    keyHandlers['='] = [this](HWND hwnd, int c) {Utils::handleAutoIndent(hwnd, c);};

    // find / till motions
    keyHandlers['f'] = [this](HWND hwnd, int c) { handleFindChar(hwnd, c); };
    keyHandlers['F'] = [this](HWND hwnd, int c) { handleFindCharBack(hwnd, c); };
    keyHandlers['t'] = [this](HWND hwnd, int c) { handleTillChar(hwnd, c); };
    keyHandlers['T'] = [this](HWND hwnd, int c) { handleTillCharBack(hwnd, c); };

    keyHandlers[';'] = [this](HWND hwnd, int c) { handleRepeatFind(hwnd, c); };
    keyHandlers[','] = [this](HWND hwnd, int c) { handleRepeatFindReverse(hwnd, c); };

    keyHandlers['/'] = [this](HWND hwnd, int c) { handleSearchForward(hwnd, c); };
    keyHandlers['n'] = [this](HWND hwnd, int c) { handleSearchNext(hwnd, c); };
    keyHandlers['N'] = [this](HWND hwnd, int c) { handleSearchPrevious(hwnd, c); };

    keyHandlers['i'] = [this](HWND hwnd, int c) {
        if (state.isBlockVisual) {
            return;
        }
        state.textObjectPending = 'i';
        Utils::setStatus(TEXT("-- inner text object --"));
        };

    keyHandlers['a'] = [this](HWND hwnd, int c) {
        if (state.isBlockVisual) {
            return;
        }
        state.textObjectPending = 'a';
        Utils::setStatus(TEXT("-- around text object --"));
        };
}

void VisualMode::enterChar(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = false;
    state.isBlockVisual = false;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETANCHOR, caret, 0);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);

    state.visualAnchor = caret;
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    state.visualSearchAnchor = -1;  // Reset search anchor when entering visual mode

    Utils::setStatus(TEXT("-- VISUAL --"));
}

void VisualMode::enterLine(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = true;
    state.isBlockVisual = false;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    state.visualAnchor = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);

    Utils::setStatus(TEXT("-- VISUAL LINE --"));
    setSelection(hwndEdit);
}

void VisualMode::enterBlock(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = false;
    state.isBlockVisual = true;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONANCHOR, caret, 0);
    ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);

    state.visualAnchor = caret;
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    state.visualSearchAnchor = -1;

    Utils::setStatus(TEXT("-- VISUAL BLOCK --"));
}

void VisualMode::setSelection(HWND hwndEdit) {
    if (state.mode != VISUAL) return;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    if (state.isLineVisual) {
        int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
        int anchorLine = state.visualAnchorLine;

        int startLine = (std::min)(anchorLine, currentLine);
        int endLine = (std::max)(anchorLine, currentLine);

        int startPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        int endPos;

        int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        if (endLine < totalLines - 1) {
            endPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0);
        }
        else {
            endPos = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        }

        ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
    }
    else if (state.isBlockVisual) {
        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);

        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONANCHOR, state.visualAnchor, 0);
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, state.visualAnchor, caret);
    }
}

void VisualMode::handleKey(HWND hwndEdit, char c) {
    // Handle f/F
    if (state.textObjectPending == 'f' && (state.opPending == 'f' || state.opPending == 'F')) {
        char searchChar = c;

        // Store the search state before performing the motion
        state.lastSearchChar = searchChar;
        state.lastSearchForward = (state.opPending == 'f');
        state.lastSearchTill = false;

        int count = (state.repeatCount > 0) ? state.repeatCount : 1;

        if (state.opPending == 'f')
            Motion::nextChar(hwndEdit, count, searchChar);
        else
            Motion::prevChar(hwndEdit, count, searchChar);

        state.recordLastOp(OP_MOTION, count, state.opPending, searchChar);

        state.opPending = 0;
        state.textObjectPending = 0;
        state.repeatCount = 0;

        Utils::setStatus(TEXT("-- VISUAL --"));
        return;
    }

    // Handle t/T
    if (state.textObjectPending == 't' && (state.opPending == 't' || state.opPending == 'T')) {
        char searchChar = c;

        // Store the search state before performing the motion
        state.lastSearchChar = searchChar;
        state.lastSearchForward = (state.opPending == 't');
        state.lastSearchTill = true;

        int count = (state.repeatCount > 0) ? state.repeatCount : 1;

        if (state.opPending == 't')
            Motion::tillChar(hwndEdit, count, searchChar);
        else
            Motion::tillCharBack(hwndEdit, count, searchChar);

        state.recordLastOp(OP_MOTION, count, state.opPending, searchChar);

        state.opPending = 0;
        state.textObjectPending = 0;
        state.repeatCount = 0;

        Utils::setStatus(TEXT("-- VISUAL --"));
        return;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
        int digit = c - '0';
        if (c == '0' && state.repeatCount == 0) {
            handleMotion(hwndEdit, '^', 1);
            return;
        }
        state.repeatCount = state.repeatCount * 10 + digit;
        return;
    }

    int count = (state.repeatCount > 0) ? state.repeatCount : 1;

    if (c == 'g') {
        if (state.opPending != 'g') {
            state.opPending = 'g';
            state.repeatCount = 0;
        }
        else {
            handleGCommand(hwndEdit, count);
            state.opPending = 0;
            state.repeatCount = 0;
        }
        return;
    }

    if (c == 'i' || c == 'a') {
        state.textObjectPending = c; // wait for object key next
        Utils::setStatus(c == 'i' ? TEXT("-- inner text object --") : TEXT("-- around text object --"));
        return;
    }

    if (state.textObjectPending && (c == 'w' || c == 'W' || c == 'p' || c == 's' ||
        c == '"' || c == '\'' || c == '`' ||
        c == '(' || c == ')' || c == '[' || c == ']' ||
        c == '{' || c == '}' || c == '<' || c == '>' || c == 't' /* tag */)) {

        TextObject::apply(hwndEdit, state, 'v', state.textObjectPending, c == 't' ? 't' : c);
        state.textObjectPending = 0;
        state.repeatCount = 0;
        Utils::setStatus(TEXT("-- VISUAL --"));
        return;
    }

    auto it = keyHandlers.find(c);
    if (it != keyHandlers.end()) {
        it->second(hwndEdit, count);
        state.repeatCount = 0;
    }
}

void VisualMode::handleDelete(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    if (state.isBlockVisual) {
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);

        int anchor = state.visualAnchor;
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int startPos = (std::min)(anchor, caret);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, startPos, 0);
        ::SendMessage(hwndEdit, SCI_SETANCHOR, startPos, 0);

        state.isBlockVisual = false;
    }
    else {
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

    state.recordLastOp(OP_MOTION, count, 'd');
    if (g_normalMode) {
        g_normalMode->enter();
    }
}

void VisualMode::handleYank(HWND hwndEdit, int count) {
    int start = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);

    if (state.isBlockVisual) {
        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
        state.isBlockVisual = false;
    }

    state.recordLastOp(OP_MOTION, count, 'y');
    if (g_normalMode) {
        g_normalMode->enter();
    }
}

void VisualMode::handleChange(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    if (state.isBlockVisual) {
        int anchor = (int)::SendMessage(hwndEdit, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
        int caret = (int)::SendMessage(hwndEdit, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);

        int anchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
        int caretLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
        int anchorColumn = anchor - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, anchorLine, 0);
        int caretColumn = caret - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, caretLine, 0);

        int startLine = (std::min)(anchorLine, caretLine);
        int endLine = (std::max)(anchorLine, caretLine);
        int startColumn = (std::min)(anchorColumn, caretColumn);
        int endColumn = (std::max)(anchorColumn, caretColumn);

        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);

        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
        ::SendMessage(hwndEdit, SCI_CLEARSELECTIONS, 0, 0);

        bool firstCursor = true;
        for (int line = startLine; line <= endLine; line++) {
            int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
            int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

            int cursorPos = lineStart + startColumn;

            if (cursorPos > lineEnd) {
                cursorPos = lineEnd;
            }

            if (firstCursor) {
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, cursorPos, 0);
                ::SendMessage(hwndEdit, SCI_SETANCHOR, cursorPos, 0);
                firstCursor = false;
            }
            else {
                ::SendMessage(hwndEdit, SCI_ADDSELECTION, cursorPos, cursorPos);
            }
        }

        state.isBlockVisual = false;
    }
    else {
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

    state.recordLastOp(OP_MOTION, count, 'c');
    if (g_normalMode) {
        g_normalMode->enterInsertMode();
    }
}

void VisualMode::handleSwapAnchor(HWND hwndEdit, int count) {
    int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, anchor, 0);
    ::SendMessage(hwndEdit, SCI_SETANCHOR, currentPos, 0);

    if (state.isLineVisual) {
        state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
        state.visualAnchor = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
    }
    else {
        state.visualAnchor = anchor;
        state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
    }
}

void VisualMode::handleVisualChar(HWND hwndEdit, int count) {
    if (!state.isLineVisual && !state.isBlockVisual) {
        if (g_normalMode) {
            g_normalMode->enter();
        }
    }
    else {
        enterChar(hwndEdit);
    }
}

void VisualMode::handleVisualLine(HWND hwndEdit, int count) {
    if (state.isLineVisual) {
        if (g_normalMode) {
            g_normalMode->enter();
        }
    }
    else {
        enterLine(hwndEdit);
    }
}

void VisualMode::handleVisualBlock(HWND hwndEdit, int count) {
    if (state.isBlockVisual) {
        if (g_normalMode) {
            g_normalMode->enter();
        }
    }
    else {
        enterBlock(hwndEdit);
    }
}

void VisualMode::handleMotion(HWND hwndEdit, char motionChar, int count) {
    if (count <= 0) count = 1;

    if (state.isBlockVisual) {
        handleBlockVisualMotion(hwndEdit, motionChar, count);
    }
    else {
        handleStandardVisualMotion(hwndEdit, motionChar, count);
    }

    updateVisualAnchor(hwndEdit);
}

void VisualMode::handleBlockVisualMotion(HWND hwndEdit, char motionChar, int count) {
    ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);

    int anchor = state.visualAnchor;
    int currentCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentCaret, 0);
    int currentColumn = currentCaret - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, currentLine, 0);

    switch (motionChar) {
    case 'h':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARLEFT, 0, 0);
        }
        break;
    case 'j':
        for (int i = 0; i < count; i++) {
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
                (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
            int nextLine = line + 1;
            int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
            if (nextLine < lineCount) {
                int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, nextLine, 0);
                int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, nextLine, 0);
                int targetPos = lineStart + currentColumn;
                if (targetPos > lineEnd) targetPos = lineEnd;
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, targetPos, 0);
            }
        }
        break;
    case 'k':
        for (int i = 0; i < count; i++) {
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
                (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
            int prevLine = line - 1;
            if (prevLine >= 0) {
                int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, prevLine, 0);
                int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, prevLine, 0);
                int targetPos = lineStart + currentColumn;
                if (targetPos > lineEnd) targetPos = lineEnd;
                ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, targetPos, 0);
            }
        }
        break;
    case 'l':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARRIGHT, 0, 0);
        }
        break;
    case 'w':
        for (int i = 0; i < count; i++) {
            handleBlockWordRight(hwndEdit, false);
        }
        break;
    case 'W':
        for (int i = 0; i < count; i++) {
            handleBlockWordRight(hwndEdit, true);
        }
        break;
    case 'b':
        for (int i = 0; i < count; i++) {
            handleBlockWordLeft(hwndEdit, false);
        }
        break;
    case 'B':
        for (int i = 0; i < count; i++) {
            handleBlockWordLeft(hwndEdit, true);
        }
        break;
    case 'e':
        for (int i = 0; i < count; i++) {
            handleBlockWordEnd(hwndEdit, false);
        }
        break;
    case 'E':
        for (int i = 0; i < count; i++) {
            handleBlockWordEnd(hwndEdit, true);
        }
        break;
    case '$':
        ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
        break;
    case '^':
    case '0':
        ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
        break;
    case '{':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARAUP, 0, 0);
        }
        break;
    case '}':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARADOWN, 0, 0);
        }
        break;
    case 'G':
        if (count == 1) {
            ::SendMessage(hwndEdit, SCI_DOCUMENTEND, 0, 0);
        }
        else {
            ::SendMessage(hwndEdit, SCI_GOTOLINE, count - 1, 0);
        }
        break;
    case 'H':
        ::SendMessage(hwndEdit, SCI_PAGEUP, 0, 0);
        break;
    case 'L':
        ::SendMessage(hwndEdit, SCI_PAGEDOWN, 0, 0);
        break;
    default:
        break;
    }

    int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
    ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
}

void VisualMode::handleBlockWordRight(HWND hwndEdit, bool bigWord) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    if (pos >= lineEnd) {
        int nextLine = line + 1;
        int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        if (nextLine < lineCount) {
            int nextLineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, nextLine, 0);
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, nextLineStart, 0);
        }
        return;
    }

    while (pos < lineEnd) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
        if (bigWord ? (ch == ' ' || ch == '\t') : !iswalnum(ch)) {
            break;
        }
        pos++;
    }

    while (pos < lineEnd) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
        if (!(ch == ' ' || ch == '\t')) {
            break;
        }
        pos++;
    }

    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
}

void VisualMode::handleBlockWordLeft(HWND hwndEdit, bool bigWord) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);

    if (pos <= lineStart) {
        int prevLine = line - 1;
        if (prevLine >= 0) {
            int prevLineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, prevLine, 0);
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, prevLineEnd, 0);
        }
        return;
    }

    pos--;

    while (pos > lineStart) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
        if (!(ch == ' ' || ch == '\t')) {
            break;
        }
        pos--;
    }

    while (pos > lineStart) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos - 1, 0);
        if (bigWord ? (ch == ' ' || ch == '\t') : !iswalnum(ch)) {
            break;
        }
        pos--;
    }

    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
}

void VisualMode::handleBlockWordEnd(HWND hwndEdit, bool bigWord) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    if (pos >= lineEnd) return;

    pos++;

    while (pos < lineEnd) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
        if (bigWord ? (ch == ' ' || ch == '\t') : !iswalnum(ch)) {
            break;
        }
        pos++;
    }

    if (pos > 0) pos--;

    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
}

void VisualMode::handleStandardVisualMotion(HWND hwndEdit, char motionChar, int count) {
    switch (motionChar) {
    case 'h': Motion::charLeft(hwndEdit, count); break;
    case 'j': Motion::lineDown(hwndEdit, count); break;
    case 'k': Motion::lineUp(hwndEdit, count); break;
    case 'l': Motion::charRight(hwndEdit, count); break;
    case 'w': Motion::wordRight(hwndEdit, count); break;
    case 'W': Motion::wordRightBig(hwndEdit, count); break;
    case 'b': Motion::wordLeft(hwndEdit, count); break;
    case 'B': Motion::wordLeftBig(hwndEdit, count); break;
    case 'e': Motion::wordEnd(hwndEdit, count); break;
    case 'E': Motion::wordEndBig(hwndEdit, count); break;
    case '$': Motion::lineEnd(hwndEdit, count); break;
    case '^': Motion::lineStart(hwndEdit, count); break;
    case '{': Motion::paragraphUp(hwndEdit, count); break;
    case '}': Motion::paragraphDown(hwndEdit, count); break;
    case 'H': Motion::pageUp(hwndEdit); break;
    case 'L': Motion::pageDown(hwndEdit); break;
    case 'G':
        if (count == 1) {
            Motion::documentEnd(hwndEdit);
        }
        else {
            Motion::gotoLine(hwndEdit, count);
        }
        break;
    case '~':
        Motion::toggleCase(hwndEdit, 1);
        if (g_normalMode) {
            g_normalMode->enter();
        }
        break;
    default:
        break;
    }
}

void VisualMode::updateVisualAnchor(HWND hwndEdit) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
    if (state.isLineVisual) {
        state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
        state.visualAnchor = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
    }
    else {
        state.visualAnchor = anchor;
        state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
    }
}

void VisualMode::handleGCommand(HWND hwndEdit, int count) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);

    if (count == 1) {
        Motion::documentStart(hwndEdit);
    }
    else {
        Motion::gotoLine(hwndEdit, count);
        int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    }

    if (state.isLineVisual) {
        setSelection(hwndEdit);
    }
}

// Visual-mode handlers for find/till
void VisualMode::handleFindChar(HWND hwndEdit, int count) {
    state.opPending = 'f';
    state.textObjectPending = 'f';
    Utils::setStatus(TEXT("-- find char --"));
}

void VisualMode::handleFindCharBack(HWND hwndEdit, int count) {
    state.opPending = 'F';
    state.textObjectPending = 'f';
    Utils::setStatus(TEXT("-- find char backward --"));
}

void VisualMode::handleTillChar(HWND hwndEdit, int count) {
    state.opPending = 't';
    state.textObjectPending = 't';
    Utils::setStatus(TEXT("-- till char --"));
}

void VisualMode::handleTillCharBack(HWND hwndEdit, int count) {
    state.opPending = 'T';
    state.textObjectPending = 't';
    Utils::setStatus(TEXT("-- till char backward --"));
}

void VisualMode::handleRepeatFind(HWND hwndEdit, int count) {
    if (state.lastSearchChar == 0) return;

    bool isForward = state.lastSearchForward;
    bool isTill = state.lastSearchTill;
    char searchChar = state.lastSearchChar;

    if (isTill) {
        if (isForward)
            Motion::tillChar(hwndEdit, count, searchChar);
        else
            Motion::tillCharBack(hwndEdit, count, searchChar);
    }
    else {
        if (isForward)
            Motion::nextChar(hwndEdit, count, searchChar);
        else
            Motion::prevChar(hwndEdit, count, searchChar);
    }

    Utils::setStatus(TEXT("-- VISUAL --"));
}

void VisualMode::handleRepeatFindReverse(HWND hwndEdit, int count) {
    if (state.lastSearchChar == 0) return;

    bool isForward = state.lastSearchForward;
    bool isTill = state.lastSearchTill;
    char searchChar = state.lastSearchChar;

    if (isTill) {
        if (isForward)
            Motion::tillCharBack(hwndEdit, count, searchChar);
        else
            Motion::tillChar(hwndEdit, count, searchChar);
    }
    else {
        if (isForward)
            Motion::prevChar(hwndEdit, count, searchChar);
        else
            Motion::nextChar(hwndEdit, count, searchChar);
    }

    Utils::setStatus(TEXT("-- VISUAL --"));
}

void VisualMode::handleSearchForward(HWND hwndEdit, int count) {
    if (g_commandMode) {
        // Store the current position as the anchor before entering search
        int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        state.visualSearchAnchor = state.visualAnchor;
        g_commandMode->enter('/');
    }
}

void VisualMode::handleSearchNext(HWND hwndEdit, int count) {
    if (!g_commandMode) return;

    // If no visual search anchor is set, set it now
    if (state.visualSearchAnchor == -1) {
        state.visualSearchAnchor = state.visualAnchor;
    }

    // Get current selection end to continue searching from there
    int currentEnd = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);

    // Perform search from current end position
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, currentEnd, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    // If not found, wrap to beginning
    if (found == -1) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1) {
            Utils::setStatus(TEXT("Search wrapped to top"));
        }
    }

    if (found != -1) {
        int matchStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int matchEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

        // Extend selection from original visual anchor to end of match
        if (state.visualSearchAnchor <= matchStart) {
            ::SendMessage(hwndEdit, SCI_SETSEL, state.visualSearchAnchor, matchEnd);
        }
        else {
            ::SendMessage(hwndEdit, SCI_SETSEL, matchEnd, state.visualSearchAnchor);
        }

        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    }
    else {
        Utils::setStatus(TEXT("Pattern not found"));
    }
}

void VisualMode::handleSearchPrevious(HWND hwndEdit, int count) {
    if (!g_commandMode) return;

    if (state.visualSearchAnchor == -1) {
        state.visualSearchAnchor = state.visualAnchor;
    }

    int currentStart = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    if (currentStart > 0) currentStart--;

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, currentStart, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    int lastFound = -1;
    int lastStart = -1, lastEnd = -1;

    // Find the last match before current position
    while (found != -1) {
        lastFound = found;
        lastStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        lastEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lastEnd, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());
    }

    if (lastFound != -1) {
        // Extend selection from original visual anchor to start of match
        if (state.visualSearchAnchor >= lastEnd) {
            ::SendMessage(hwndEdit, SCI_SETSEL, state.visualSearchAnchor, lastStart);
        }
        else {
            ::SendMessage(hwndEdit, SCI_SETSEL, lastStart, state.visualSearchAnchor);
        }

        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    }
    else {
        // Wrap to end
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, currentStart, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        lastFound = -1;
        while (found != -1) {
            lastFound = found;
            lastStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
            lastEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lastEnd, 0);
            found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());
        }

        if (lastFound != -1) {
            if (state.visualSearchAnchor >= lastEnd) {
                ::SendMessage(hwndEdit, SCI_SETSEL, state.visualSearchAnchor, lastStart);
            }
            else {
                ::SendMessage(hwndEdit, SCI_SETSEL, lastStart, state.visualSearchAnchor);
            }

            ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
            Utils::setStatus(TEXT("Search wrapped to bottom"));
            Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
        }
        else {
            Utils::setStatus(TEXT("Pattern not found"));
        }
    }
}

void VisualMode::handleToggleComment(HWND hwndEdit, int count) {
    ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_BLOCK_COMMENT, 0);

    if (g_normalMode) {
        g_normalMode->enter();
    }

    state.recordLastOp(OP_MOTION, count, 'c');
}

void VisualMode::handleBlockInsert(HWND hwndEdit, int count) {
    if (!state.isBlockVisual) {
        return;
    }

    int anchor = (int)::SendMessage(hwndEdit, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
    int caret = (int)::SendMessage(hwndEdit, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);

    int anchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
    int caretLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int anchorColumn = anchor - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, anchorLine, 0);
    int caretColumn = caret - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, caretLine, 0);

    int startLine = (std::min)(anchorLine, caretLine);
    int endLine = (std::max)(anchorLine, caretLine);
    int startColumn = (std::min)(anchorColumn, caretColumn);
    int endColumn = (std::max)(anchorColumn, caretColumn);

    ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
    ::SendMessage(hwndEdit, SCI_CLEARSELECTIONS, 0, 0);

    bool firstCursor = true;
    for (int line = startLine; line <= endLine; line++) {
        int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        int cursorPos = lineStart + startColumn;
        if (cursorPos > lineEnd) {
            cursorPos = lineEnd;
        }

        if (firstCursor) {
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, cursorPos, 0);
            ::SendMessage(hwndEdit, SCI_SETANCHOR, cursorPos, 0);
            firstCursor = false;
        } else {
            ::SendMessage(hwndEdit, SCI_ADDSELECTION, cursorPos, cursorPos);
        }
    }

    state.isBlockVisual = false;
    if (g_normalMode) {
        g_normalMode->enterInsertMode();
    }
}

void VisualMode::handleBlockAppend(HWND hwndEdit, int count) {
    if (!state.isBlockVisual) {
        return;
    }

    int anchor = (int)::SendMessage(hwndEdit, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
    int caret = (int)::SendMessage(hwndEdit, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);

    int anchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
    int caretLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    int anchorColumn = anchor - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, anchorLine, 0);
    int caretColumn = caret - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, caretLine, 0);

    int startLine = (std::min)(anchorLine, caretLine);
    int endLine = (std::max)(anchorLine, caretLine);
    int startColumn = (std::min)(anchorColumn, caretColumn);
    int endColumn = (std::max)(anchorColumn, caretColumn);

    ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
    ::SendMessage(hwndEdit, SCI_CLEARSELECTIONS, 0, 0);

    bool firstCursor = true;
    for (int line = startLine; line <= endLine; line++) {
        int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        int cursorPos = lineStart + endColumn;

        if (cursorPos > lineEnd) {
            cursorPos = lineEnd;
        }

        if (firstCursor) {
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, cursorPos, 0);
            ::SendMessage(hwndEdit, SCI_SETANCHOR, cursorPos, 0);
            firstCursor = false;
        } else {
            ::SendMessage(hwndEdit, SCI_ADDSELECTION, cursorPos, cursorPos);
        }
    }

    state.isBlockVisual = false;
    if (g_normalMode) {
        g_normalMode->enterInsertMode();
    }
}
