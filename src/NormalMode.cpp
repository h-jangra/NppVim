#include "../include/NormalMode.h"
#include "../include/Motion.h"
#include "../include/Utils.h"
#include "../include/VisualMode.h"
#include "../include/CommandMode.h"
#include "../include/TextObject.h"

#include "../plugin/Scintilla.h"
#include "../plugin/menuCmdID.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/PluginInterface.h"

extern VisualMode* g_visualMode;
extern CommandMode* g_commandMode;

extern NppData nppData;


NormalMode::NormalMode(VimState& state) : state(state) {
    setupKeyHandlers();
}

void NormalMode::setupKeyHandlers() {
    // Motion keys
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
    keyHandlers['t'] = [this](HWND hwnd, int c) { handleTabCommand(hwnd, c); };
    keyHandlers['T'] = [this](HWND hwnd, int c) { handleTabReverseCommand(hwnd, c); };

    // Insert mode
    keyHandlers['i'] = [this](HWND hwnd, int c) { handleInsert(hwnd, c); };
    keyHandlers['a'] = [this](HWND hwnd, int c) { handleAppend(hwnd, c); };
    keyHandlers['A'] = [this](HWND hwnd, int c) { handleAppendEnd(hwnd, c); };
    keyHandlers['I'] = [this](HWND hwnd, int c) { handleInsertStart(hwnd, c); };
    keyHandlers['o'] = [this](HWND hwnd, int c) { handleOpenBelow(hwnd, c); };
    keyHandlers['O'] = [this](HWND hwnd, int c) { handleOpenAbove(hwnd, c); };

    // Edit operations
    keyHandlers['d'] = [this](HWND hwnd, int c) { handleDelete(hwnd, c); };
    keyHandlers['y'] = [this](HWND hwnd, int c) { handleYank(hwnd, c); };
    keyHandlers['c'] = [this](HWND hwnd, int c) { handleChange(hwnd, c); };
    keyHandlers['x'] = [this](HWND hwnd, int c) { handleDeleteChar(hwnd, c); };
    keyHandlers['X'] = [this](HWND hwnd, int c) { handleDeleteCharBack(hwnd, c); };
    keyHandlers['D'] = [this](HWND hwnd, int c) { handleDeleteToEnd(hwnd, c); };
    keyHandlers['C'] = [this](HWND hwnd, int c) { handleChangeToEnd(hwnd, c); };
    keyHandlers['J'] = [this](HWND hwnd, int c) { handleJoinLines(hwnd, c); };
    keyHandlers['r'] = [this](HWND hwnd, int c) { handleReplace(hwnd, c); };
    keyHandlers['R'] = [this](HWND hwnd, int c) { handleReplaceMode(hwnd, c); };
    keyHandlers['p'] = [this](HWND hwnd, int c) { handlePaste(hwnd, c); };
    keyHandlers['P'] = [this](HWND hwnd, int c) { handlePasteBefore(hwnd, c); };

    // Search
    keyHandlers['/'] = [this](HWND hwnd, int c) { handleSearchForward(hwnd, c); };
    keyHandlers['n'] = [this](HWND hwnd, int c) { handleSearchNext(hwnd, c); };
    keyHandlers['N'] = [this](HWND hwnd, int c) { handleSearchPrevious(hwnd, c); };
    keyHandlers['*'] = [this](HWND hwnd, int c) { handleSearchWordForward(hwnd, c); };
    keyHandlers['#'] = [this](HWND hwnd, int c) { handleSearchWordBackward(hwnd, c); };
    keyHandlers['f'] = [this](HWND hwnd, int c) { handleFindChar(hwnd, c); };
    keyHandlers['F'] = [this](HWND hwnd, int c) { handleFindCharBack(hwnd, c); };
    keyHandlers[';'] = [this](HWND hwnd, int c) { handleRepeatFind(hwnd, c); };
    keyHandlers[','] = [this](HWND hwnd, int c) { handleRepeatFindReverse(hwnd, c); };

    // Visual mode
    keyHandlers['v'] = [this](HWND hwnd, int c) { handleVisualChar(hwnd, c); };
    keyHandlers['V'] = [this](HWND hwnd, int c) { handleVisualLine(hwnd, c); };

    // Other
    keyHandlers['u'] = [this](HWND hwnd, int c) { handleUndo(hwnd, c); };
    keyHandlers['.'] = [this](HWND hwnd, int c) { handleRepeat(hwnd, c); };
    keyHandlers[':'] = [this](HWND hwnd, int c) { handleCommandMode(hwnd, c); };
    keyHandlers['`'] = [this](HWND hwnd, int count) { handleJumpBack(hwnd, count); };
    keyHandlers['\''] = [this](HWND hwnd, int count) { handleJumpBackToLine(hwnd, count); };
}

void NormalMode::enter() {
    HWND hwndEdit = Utils::getCurrentScintillaHandle();
    state.mode = NORMAL;
    state.isLineVisual = false;
    state.visualAnchor = -1;
    state.visualAnchorLine = -1;
    state.reset();
    state.lastSearchMatchCount = -1;

    Utils::setStatus(TEXT("-- NORMAL --"));
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);

    Utils::clearSearchHighlights(hwndEdit);
}

void NormalMode::enterInsertMode() {
    HWND hwndEdit = Utils::getCurrentScintillaHandle();
    state.mode = INSERT;
    state.isLineVisual = false;
    state.visualAnchor = -1;
    state.visualAnchorLine = -1;
    state.reset();

    Utils::setStatus(TEXT("-- INSERT --"));
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}

void NormalMode::handleKey(HWND hwndEdit, char c) {
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
        state.recordLastOp(OP_REPLACE, 1, 'r');
        return;
    }

    // Handle character search after 'f' or 'F'
    if (state.textObjectPending == 'f' && (state.opPending == 'f' || state.opPending == 'F')) {
        char searchChar = c;
        bool found = false;

        if (state.opPending == 'f') {
            Motion::nextChar(hwndEdit, count, searchChar);
            found = true;
        } else if (state.opPending == 'F') {
            Motion::prevChar(hwndEdit, count, searchChar);
            found = true;
        }

        if (found) {
            state.lastSearchChar = searchChar;
            state.lastSearchForward = (state.opPending == 'f');
            state.recordLastOp(OP_MOTION, count, state.opPending, searchChar);
        } else {
            Utils::setStatus(TEXT("Character not found"));
        }

        state.opPending = 0;
        state.textObjectPending = 0;
        state.repeatCount = 0;
        return;
    }

    // Handle text object completion
    if (state.textObjectPending && state.opPending) {
        if (c == 'w' || c == 'W' || c == '"' || c == '\'' || c == '`' ||
            c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '<' || c == '>') {

            TextObject::apply(hwndEdit, state, state.opPending, state.textObjectPending, c);
            state.opPending = 0;
            state.textObjectPending = 0;
            state.repeatCount = 0;
            return;
        }
    }

    // Handle operator + text object modifier (i/a)
    if (state.opPending && (c == 'i' || c == 'a')) {
        state.textObjectPending = c;
        state.repeatCount = 0;
        return;
    }

    // Handle double operator commands (dd, yy, cc)
    if ((c == 'd' || c == 'y' || c == 'c') && state.opPending == c) {
        ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
        for (int i = 0; i < count; ++i) {
            if (c == 'd') deleteLineOnce(hwndEdit);
            else if (c == 'y') yankLineOnce(hwndEdit);
            else if (c == 'c') {
                deleteLineOnce(hwndEdit);
                enterInsertMode();
            }
        }
        ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

        if (c == 'd') state.recordLastOp(OP_DELETE_LINE, count);
        else if (c == 'y') state.recordLastOp(OP_YANK_LINE, count);
        else if (c == 'c') state.recordLastOp(OP_MOTION, count, 'c');

        state.opPending = 0;
        state.repeatCount = 0;
        return;
    }

    // Handle motion with pending operator
    if (state.opPending && state.opPending != 'f' && state.opPending != 'F') {
        if (c == 'w' || c == 'W' || c == '$' || c == 'e' || c == 'E' ||
            c == 'b' || c == 'B' || c == 'h' || c == 'l' || c == 'j' ||
            c == 'k' || c == '^' || c == 'G' ) {

            ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
            applyOperatorToMotion(hwndEdit, state.opPending, c, count);
            ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
            state.opPending = 0;
            state.repeatCount = 0;
            return;
        }
    }

    // Handle single-key commands
    auto it = keyHandlers.find(c);
    if (it != keyHandlers.end()) {
        it->second(hwndEdit, count);
        if (!state.opPending && !state.textObjectPending) {
            state.repeatCount = 0;
        }
    }
}

// Mode transitions
void NormalMode::handleInsert(HWND hwndEdit, int count) {
    enterInsertMode();
}

void NormalMode::handleAppend(HWND hwndEdit, int count) {
    Motion::charRight(hwndEdit, 1);
    enterInsertMode();
}

void NormalMode::handleAppendEnd(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
    enterInsertMode();
}

void NormalMode::handleInsertStart(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
    enterInsertMode();
}

void NormalMode::handleOpenBelow(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
    ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);
    enterInsertMode();
}

void NormalMode::handleOpenAbove(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_HOME, 0, 0);
    ::SendMessage(hwndEdit, SCI_NEWLINE, 0, 0);
    Motion::lineUp(hwndEdit, 1);
    enterInsertMode();
}

// Edit operations
void NormalMode::handleDelete(HWND hwndEdit, int count) {
    state.opPending = 'd';
}

void NormalMode::handleYank(HWND hwndEdit, int count) {
    state.opPending = 'y';
}

void NormalMode::handleChange(HWND hwndEdit, int count) {
    state.opPending = 'c';
}

void NormalMode::handleDeleteChar(HWND hwndEdit, int count) {
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
    state.recordLastOp(OP_MOTION, count, 'x');
}

void NormalMode::handleDeleteCharBack(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);
    for (int i = 0; i < count; ++i) {
        int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        if (pos > 0) {
            ::SendMessage(hwndEdit, SCI_SETSEL, pos - 1, pos);
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        }
    }
    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    state.recordLastOp(OP_MOTION, count, 'X');
}

void NormalMode::handleDeleteToEnd(HWND hwndEdit, int count) {
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
    state.recordLastOp(OP_MOTION, count, 'D');
}

void NormalMode::handleChangeToEnd(HWND hwndEdit, int count) {
    handleDeleteToEnd(hwndEdit, count);
    enterInsertMode();
}

void NormalMode::handleJoinLines(HWND hwndEdit, int count) {
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
    state.recordLastOp(OP_MOTION, count, 'J');
}

void NormalMode::handleReplace(HWND hwndEdit, int count) {
    state.replacePending = true;
    state.recordLastOp(OP_REPLACE, count, 'r');
}

void NormalMode::handleReplaceMode(HWND hwndEdit, int count) {
    state.mode = INSERT;
    Utils::setStatus(TEXT("-- REPLACE --"));
    ::SendMessage(hwndEdit, SCI_SETOVERTYPE, true, 0);
    ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
    state.recordLastOp(OP_MOTION, count, 'R');
}

void NormalMode::handlePaste(HWND hwndEdit, int count) {
    std::string clipText = Utils::getClipboardText(hwndEdit);
    if (clipText.empty()) return;

    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    LRESULT currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    LRESULT currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
    LRESULT pastePos = SendMessage(hwndEdit, SCI_POSITIONFROMLINE, currentLine + 1, 0);

    // If last line, append newline first
    if (pastePos == 0) {
        LRESULT lastPos = SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
        ::SendMessage(hwndEdit, SCI_INSERTTEXT, lastPos, (LPARAM)"\n");
        pastePos = lastPos + 1;
    }

    for (int i = 0; i < count; ++i) {
        ::SendMessage(hwndEdit, SCI_INSERTTEXT, pastePos, (LPARAM)clipText.c_str());
        pastePos += static_cast<LRESULT>(clipText.length());
    }

    // Position cursor at first non-blank character of pasted line
    int newLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pastePos - clipText.length(), 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, newLine, 0);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, lineStart, 0);

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    state.recordLastOp(OP_PASTE_LINE, count);
}

void NormalMode::handlePasteBefore(HWND hwndEdit, int count) {
    std::string clipText = Utils::getClipboardText(hwndEdit);
    if (clipText.empty()) return;

    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
        ::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
    int pastePos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, currentLine, 0);

    for (int i = 0; i < count; ++i) {
        ::SendMessage(hwndEdit, SCI_INSERTTEXT, pastePos, (LPARAM)clipText.c_str());
        pastePos += clipText.length();
    }

    // Position cursor at first non-blank character of pasted line
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, currentLine, 0);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, lineStart, 0);

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    state.recordLastOp(OP_PASTE_LINE, count);
}

// Motions
void NormalMode::handleMotion(HWND hwndEdit, char motion, int count) {
    bool shouldRecordJump = false;

    switch (motion) {
    case 'G':
    case 'g':
    case '{':
    case '}':
    case '%':
        shouldRecordJump = true;
        break;
    }

    if (shouldRecordJump) {
        long currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
        state.recordJump(currentPos, currentLine);
    }

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
        case 'G':
            if (count == 1) Motion::documentEnd(hwndEdit);
            else Motion::gotoLine(hwndEdit, count);
            break;
        case '%':
            ::SendMessage(hwndEdit, SCI_BRACEMATCHNEXT, 0, 0);
            break;
    }

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    state.recordLastOp(OP_MOTION, count, motion);
}

void NormalMode::handleGCommand(HWND hwnd, int count) {
    HWND hwndEdit = Utils::getCurrentScintillaHandle();

    if (state.opPending != 'g') {
        state.opPending = 'g';
        return;
    }

    long currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
    state.recordJump(currentPos, currentLine);

    int effectiveCount = (state.repeatCount > 0) ? state.repeatCount : count;
    if (effectiveCount > 1) {
        Motion::gotoLine(hwndEdit, effectiveCount);
        state.recordLastOp(OP_MOTION, effectiveCount, 'g');
    }
    else {
        Motion::documentStart(hwndEdit);
        state.recordLastOp(OP_MOTION, 1, 'g');
    }

    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    state.opPending = 0;
    state.repeatCount = 0;
}

void NormalMode::handleTabCommand(HWND hwnd, int count) {
    if (state.opPending == 'g') {
        for (int i = 0; i < count; i++) {
            ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_NEXT);
        }
        state.opPending = 0;
        state.recordLastOp(OP_MOTION, count, 't');
    }
}

void NormalMode::handleTabReverseCommand(HWND hwnd, int count) {
    if (state.opPending == 'g') {
        for (int i = 0; i < count; i++) {
            ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_PREV);
        }
        state.opPending = 0;
        state.recordLastOp(OP_MOTION, count, 'T');
    }
}

// Search operations
void NormalMode::handleSearchForward(HWND hwndEdit, int count) {
    if (g_commandMode) {
        g_commandMode->enter('/');
    }
}

void NormalMode::handleSearchNext(HWND hwndEdit, int count) {
    if (g_commandMode) {
        // Record position before search
        long currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
        state.recordJump(currentPos, currentLine);

        g_commandMode->searchNext(hwndEdit);
        state.recordLastOp(OP_MOTION, count, 'n');
    }
}

void NormalMode::handleSearchPrevious(HWND hwndEdit, int count) {
    if (g_commandMode) {
        // Record position before search
        long currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
        state.recordJump(currentPos, currentLine);

        g_commandMode->searchPrevious(hwndEdit);
        state.recordLastOp(OP_MOTION, count, 'N');
    }
}

void NormalMode::handleSearchWordForward(HWND hwndEdit, int count) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    auto bounds = Utils::findWordBounds(hwndEdit, pos);
    if (bounds.first != bounds.second) {
        int len = bounds.second - bounds.first;
        std::vector<char> word(len + 1);
        Sci_TextRangeFull textRange;
        textRange.chrg.cpMin = bounds.first;
        textRange.chrg.cpMax = bounds.second;
        textRange.lpstrText = word.data();
        ::SendMessage(hwndEdit, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&textRange);

        if (g_commandMode) {
            g_commandMode->performSearch(hwndEdit, word.data(), false);
            g_commandMode->searchNext(hwndEdit);
            state.recordLastOp(OP_MOTION, count, '*');
        }
    }
}

void NormalMode::handleSearchWordBackward(HWND hwndEdit, int count) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    auto bounds = Utils::findWordBounds(hwndEdit, pos);
    if (bounds.first != bounds.second) {
        int len = bounds.second - bounds.first;
        std::vector<char> word(len + 1);
        Sci_TextRangeFull textRange;
        textRange.chrg.cpMin = bounds.first;
        textRange.chrg.cpMax = bounds.second;
        textRange.lpstrText = word.data();
        ::SendMessage(hwndEdit, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&textRange);

        if (g_commandMode) {
            g_commandMode->performSearch(hwndEdit, word.data(), false);
            g_commandMode->searchPrevious(hwndEdit);
            state.recordLastOp(OP_MOTION, count, '#');
        }
    }
}

void NormalMode::handleFindChar(HWND hwndEdit, int count) {
    state.opPending = 'f';
    state.textObjectPending = 'f';
}

void NormalMode::handleFindCharBack(HWND hwndEdit, int count) {
    state.opPending = 'F';
    state.textObjectPending = 'f';
}

void NormalMode::handleRepeatFind(HWND hwndEdit, int count) {
    if (state.lastSearchChar != 0) {
        if (state.lastSearchForward) {
            Motion::nextChar(hwndEdit, count, state.lastSearchChar);
        } else {
            Motion::prevChar(hwndEdit, count, state.lastSearchChar);
        }
        state.recordLastOp(OP_MOTION, count, state.lastSearchForward ? 'f' : 'F', state.lastSearchChar);
    }
}

void NormalMode::handleRepeatFindReverse(HWND hwndEdit, int count) {
    if (state.lastSearchChar != 0) {
        if (state.lastSearchForward) {
            Motion::prevChar(hwndEdit, count, state.lastSearchChar);
        } else {
            Motion::nextChar(hwndEdit, count, state.lastSearchChar);
        }
        state.recordLastOp(OP_MOTION, count, state.lastSearchForward ? 'F' : 'f', state.lastSearchChar);
    }
}

// Visual mode
void NormalMode::handleVisualChar(HWND hwndEdit, int count) {
    if (g_visualMode) {
        g_visualMode->enterChar(hwndEdit);
    }
}

void NormalMode::handleVisualLine(HWND hwndEdit, int count) {
    if (g_visualMode) {
        g_visualMode->enterLine(hwndEdit);
    }
}

// Other
void NormalMode::handleUndo(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_UNDO, 0, 0);
    state.recordLastOp(OP_MOTION, 1, 'u');
}

void NormalMode::handleRepeat(HWND hwndEdit, int count) {
    if (state.lastOp.type == OP_NONE) return;

    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    int repeatCount = (state.repeatCount > 0) ? state.repeatCount : state.lastOp.count;

    switch (state.lastOp.type) {
        case OP_DELETE_LINE:
            for (int i = 0; i < repeatCount; ++i) deleteLineOnce(hwndEdit);
            break;
        case OP_YANK_LINE:
            for (int i = 0; i < repeatCount; ++i) yankLineOnce(hwndEdit);
            break;
        case OP_PASTE_LINE:
            for (int i = 0; i < repeatCount; ++i) ::SendMessage(hwndEdit, SCI_PASTE, 0, 0);
            break;
        case OP_MOTION:
            if (state.lastOp.motion == 'x' || state.lastOp.motion == 'X') {
                for (int i = 0; i < repeatCount; ++i) {
                    if (state.lastOp.motion == 'x') {
                        handleDeleteChar(hwndEdit, 1);
                    } else {
                        handleDeleteCharBack(hwndEdit, 1);
                    }
                }
            } else {
                applyOperatorToMotion(hwndEdit, 'd', state.lastOp.motion, repeatCount);
            }
            break;
        case OP_REPLACE:
            state.replacePending = true;
            break;
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);
    state.repeatCount = 0;
}

void NormalMode::handleCommandMode(HWND hwndEdit, int count) {
    if (g_commandMode) {
        g_commandMode->enter(':');
    }
}

// Helper functions
void NormalMode::deleteLineOnce(HWND hwndEdit) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int end = (line < totalLines - 1)
        ? (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line + 1, 0)
        : (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0) + 1;

    // Copy to clipboard before deleting
    ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);

    // Delete the line
    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);

    // Position cursor at start of next line or current line if at end
    int newPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
}

void NormalMode::yankLineOnce(HWND hwndEdit) {
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

void NormalMode::applyOperatorToMotion(HWND hwndEdit, char op, char motion, int count) {
    int start = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

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
            state.recordLastOp(OP_MOTION, count, motion);
            break;
        case 'y':
            ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
            ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
            state.recordLastOp(OP_MOTION, count, motion);
            break;
        case 'c':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            enterInsertMode();
            state.recordLastOp(OP_MOTION, count, motion);
            break;
    }
}

void NormalMode::handleJumpBack(HWND hwndEdit, int count) {
    auto jump = state.getLastJump();
    if (jump.position != -1) {
        // Record current position before jumping back
        long currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
        state.recordJump(currentPos, currentLine);

        // Perform the jump to previous position
        SendMessage(hwndEdit, SCI_GOTOPOS, jump.position, 0);
        SendMessage(hwndEdit, SCI_SETSEL, jump.position, jump.position);
    }
}

void NormalMode::handleJumpBackToLine(HWND hwndEdit, int count) {
    auto jump = state.getLastJump();
    if (jump.lineNumber != -1) {
        // Record current position before jumping
        long currentPos = SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int currentLine = SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);
        state.recordJump(currentPos, currentLine);

        // Jump to the beginning of the line
        int lineStart = SendMessage(hwndEdit, SCI_POSITIONFROMLINE, jump.lineNumber, 0);
        SendMessage(hwndEdit, SCI_GOTOPOS, lineStart, 0);
        SendMessage(hwndEdit, SCI_SETSEL, lineStart, lineStart);
    }
}

void VimState::recordJump(long position, int lineNumber) {
    JumpPosition jump(position, lineNumber);

    // Don't record consecutive jumps to same position
    if (!jumpList.empty() && jumpList.back().position == position) {
        return;
    }

    // Remove any forward jumps if we're jumping from middle of list
    if (currentJumpIndex < jumpList.size()) {
        jumpList.resize(currentJumpIndex);
    }

    jumpList.push_back(jump);
    if (jumpList.size() > MAX_JUMPS) {
        jumpList.erase(jumpList.begin());
    }
    currentJumpIndex = jumpList.size();
}

JumpPosition VimState::getLastJump() {
    if (jumpList.empty() || currentJumpIndex == 0) {
        return JumpPosition();
    }

    currentJumpIndex--;
    if (currentJumpIndex >= jumpList.size()) {
        currentJumpIndex = jumpList.size() - 1;
    }

    return jumpList[currentJumpIndex];
}

void VimState::clearJumps() {
    jumpList.clear();
    currentJumpIndex = 0;
}