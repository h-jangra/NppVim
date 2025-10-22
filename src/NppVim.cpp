#include "NppVim.h"

HINSTANCE g_hInstance = nullptr;
NppData nppData;
const TCHAR PLUGIN_NAME[] = TEXT("NppVim");
const int nbFunc = 3;
FuncItem funcItem[nbFunc];
static std::map<HWND, WNDPROC> origProcMap;
State state;

HWND getCurrentScintillaHandle() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void setStatus(const TCHAR* msg) {
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)msg);
}

void recordLastOp(LastOpType type, int count, char motion, char searchChar) {
    state.lastOp.type = type;
    state.lastOp.count = count;
    state.lastOp.motion = motion;
    state.lastOp.searchChar = searchChar;
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