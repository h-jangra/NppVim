#include "NppVim.h"

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

std::pair<int, int> findWordBounds(HWND hwndEdit, int pos) {
    int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
    int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
    return { start, end };
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

void clearSearchHighlights(HWND hwndEdit) {
    if (!hwndEdit) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
    ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);
}
