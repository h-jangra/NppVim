#include "../include/NppVim.h"
#include "../include/TextObject.h"
#include "../include/VisualMode.h"
#include "../include/Motion.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../plugin/Scintilla.h"
#include <algorithm>
#include <cctype>

extern NormalMode* g_normalMode;

VisualMode::VisualMode(VimState& state) : state(state) {
    setupKeyHandlers();
}

void VisualMode::setupKeyHandlers() {
    keyHandlers['d'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['x'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['y'] = [this](HWND hwnd, int c) { handleYank(hwnd, c); };
    keyHandlers['c'] = [this](HWND hwnd, int c) { handleChange(hwnd, c); };
    keyHandlers['o'] = [this](HWND hwnd, int c) { handleSwapAnchor(hwnd, c); };

    keyHandlers['v'] = [this](HWND hwnd, int c) { handleVisualChar(hwnd, c); };
    keyHandlers['V'] = [this](HWND hwnd, int c) { handleVisualLine(hwnd, c); };

    keyHandlers['H'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'H', c); };
    keyHandlers['L'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'L', c); };

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
    keyHandlers['g'] = [this](HWND hwnd, int c) { handleGCommand(hwnd, c); };
    keyHandlers['~'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '~', c); };
    keyHandlers['<'] = [this](HWND hwnd, int c) { TextObject::apply(hwnd, state, '<', 0, 0); };
    keyHandlers['>'] = [this](HWND hwnd, int c) { TextObject::apply(hwnd, state, '>', 0, 0); };
    keyHandlers['='] = [this](HWND hwnd, int c) { TextObject::apply(hwnd, state, '=', 0, 0); };
}

void VisualMode::enterChar(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = false;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETANCHOR, caret, 0);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);

    state.visualAnchor = caret;
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);

    Utils::setStatus(TEXT("-- VISUAL --"));
}

void VisualMode::enterLine(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = true;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);
    state.visualAnchor = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);

    Utils::setStatus(TEXT("-- VISUAL LINE --"));
    setSelection(hwndEdit);
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
    else {
        ::SendMessage(hwndEdit, SCI_SETSEL, state.visualAnchor, caret);
    }
}

void VisualMode::handleKey(HWND hwndEdit, char c) {
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

    auto it = keyHandlers.find(c);
    if (it != keyHandlers.end()) {
        it->second(hwndEdit, count);
        state.repeatCount = 0;
    }
}

void VisualMode::handleDelete(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
    state.recordLastOp(OP_MOTION, count, 'd');
    if (g_normalMode) {
        g_normalMode->enter();
    }
}

void VisualMode::handleYank(HWND hwndEdit, int count) {
    int start = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
    state.recordLastOp(OP_MOTION, count, 'y');
    if (g_normalMode) {
        g_normalMode->enter();
    }
}

void VisualMode::handleChange(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
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
    if (!state.isLineVisual) {
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

void VisualMode::handleMotion(HWND hwndEdit, char motionChar, int count) {
    if (count <= 0) count = 1;

    switch (motionChar) {
    case 'j':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEDOWNEXTEND, 0, 0);
        }
        break;
    case 'k':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEUPEXTEND, 0, 0);
        }
        break;
    case 'h':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARLEFTEXTEND, 0, 0);
        }
        break;
    case 'l':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARRIGHTEXTEND, 0, 0);
        }
        break;
    case 'w':
    case 'W':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDRIGHTEXTEND, 0, 0);
        }
        break;
    case 'b':
    case 'B':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDLEFTEXTEND, 0, 0);
        }
        break;
    case 'e':
    case 'E':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDRIGHTENDEXTEND, 0, 0);
        }
        break;
    case '$':
        ::SendMessage(hwndEdit, SCI_LINEENDEXTEND, 0, 0);
        break;
    case '^':
    case '0':
        ::SendMessage(hwndEdit, SCI_VCHOMEEXTEND, 0, 0);
        break;
    case '{':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARAUPEXTEND, 0, 0);
        }
        break;
    case '}':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARADOWNEXTEND, 0, 0);
        }
        break;
    case 'H': Motion::pageUp(hwndEdit); break;
    case 'L': Motion::pageDown(hwndEdit); break;
    case 'G':
        if (count == 1) {
            ::SendMessage(hwndEdit, SCI_DOCUMENTENDEXTEND, 0, 0);
        }
        else {
            int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
            ::SendMessage(hwndEdit, SCI_GOTOLINE, count - 1, 0);
            int newPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newPos);
        }
        break;
    case '~':
        motion.toggleCase(hwndEdit,1);
        g_normalMode->enter();
        break;
    default:
        break;
    }

    updateVisualAnchor(hwndEdit);
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
        ::SendMessage(hwndEdit, SCI_DOCUMENTSTARTEXTEND, 0, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_GOTOLINE, count - 1, 0);
        int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    }

    if (state.isLineVisual) {
        setSelection(hwndEdit);
    }
}