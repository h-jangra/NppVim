#include "NppVim.h"

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
        for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_PAGEUP, 0, 0);
        if (state.mode == VISUAL) setVisualSelection(hwndEdit);
        break;
    case 'L':
        for (int i = 0; i < count; i++) ::SendMessage(hwndEdit, SCI_PAGEDOWN, 0, 0);
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
        if (state.opPending == 'g' || state.mode == VISUAL) {
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