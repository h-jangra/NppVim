#include "../include/VisualMode.h"
#include "../include/Motion.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../plugin/Scintilla.h"

extern NormalMode* g_normalMode;

VisualMode::VisualMode(VimState& state) : state(state) {
    setupKeyHandlers();
}

void VisualMode::setupKeyHandlers() {
    // Operations
    keyHandlers['d'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['x'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['y'] = [this](HWND hwnd, int c) { handleYank(hwnd, c); };
    keyHandlers['c'] = [this](HWND hwnd, int c) { handleChange(hwnd, c); };
    keyHandlers['o'] = [this](HWND hwnd, int c) { handleSwapAnchor(hwnd, c); };

    // Mode switches
    keyHandlers['v'] = [this](HWND hwnd, int c) { handleVisualChar(hwnd, c); };
    keyHandlers['V'] = [this](HWND hwnd, int c) { handleVisualLine(hwnd, c); };

    // Motions
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
    keyHandlers['{'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '{', c); };
    keyHandlers['}'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '}', c); };
    keyHandlers['%'] = [this](HWND hwnd, int c) { handleMotion(hwnd, '%', c); };
    keyHandlers['G'] = [this](HWND hwnd, int c) { handleMotion(hwnd, 'G', c); };
    keyHandlers['g'] = [this](HWND hwnd, int c) { handleGCommand(hwnd, c); };
}

void VisualMode::enterChar(HWND hwndEdit) {
    state.mode = VISUAL;
    state.isLineVisual = false;

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchor = caret;
    state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, caret, 0);

    Utils::setStatus(TEXT("-- VISUAL --"));
    setSelection(hwndEdit);
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
        int endPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);

        int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        if (endLine < totalLines - 1) {
            endPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0);
        } else {
            endPos = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        }
        ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
    } else {
        ::SendMessage(hwndEdit, SCI_SETANCHOR, state.visualAnchor, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);
    }
}

void VisualMode::handleKey(HWND hwndEdit, char c) {
    // Handle digits for repeat counts
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

    // Handle g command
    if (c == 'g') {
        if (state.opPending != 'g') {
            state.opPending = 'g';
            state.repeatCount = 0;
        } else {
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
    } else {
        state.visualAnchor = anchor;
        state.visualAnchorLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
    }
}

void VisualMode::handleVisualChar(HWND hwndEdit, int count) {
    if (!state.isLineVisual) {
        if (g_normalMode) {
            g_normalMode->enter();
        }
    } else {
        enterChar(hwndEdit);
    }
}

void VisualMode::handleVisualLine(HWND hwndEdit, int count) {
    if (state.isLineVisual) {
        if (g_normalMode) {
            g_normalMode->enter();
        }
    } else {
        enterLine(hwndEdit);
    }
}

void VisualMode::handleMotion(HWND hwndEdit, char motion, int count) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);

    switch (motion) {
        case 'h': Motion::charLeft(hwndEdit, count); break;
        case 'l': Motion::charRight(hwndEdit, count); break;
        case 'j': Motion::lineDown(hwndEdit, count); break;
        case 'k': Motion::lineUp(hwndEdit, count); break;
        case 'w': case 'W': Motion::wordRight(hwndEdit, count); break;
        case 'b': case 'B': Motion::wordLeft(hwndEdit, count); break;
        case 'e': case 'E': Motion::wordEnd(hwndEdit, count); break;
        case '$': Motion::lineEnd(hwndEdit, count); break;
        case '^': Motion::lineStart(hwndEdit, count); break;
        case '{': Motion::paragraphUp(hwndEdit, count); break;
        case '}': Motion::paragraphDown(hwndEdit, count); break;
        case '%': ::SendMessage(hwndEdit, SCI_BRACEMATCH, 0, 0); break;
        case 'G':
            if (count == 1) Motion::documentEnd(hwndEdit);
            else Motion::gotoLine(hwndEdit, count);
            break;
    }

    int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    setSelection(hwndEdit);
}

void VisualMode::handleGCommand(HWND hwndEdit, int count) {
    int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);

    if (count == 1) Motion::documentStart(hwndEdit);
    else Motion::gotoLine(hwndEdit, count);

    int newCaret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newCaret);
    setSelection(hwndEdit);
}
