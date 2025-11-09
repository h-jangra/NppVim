//Motion.cpp
#include "../include/Motion.h"
#include "../plugin/Scintilla.h"
#include "../include/NppVim.h"

Motion motion;

void Motion::charLeft(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARLEFTEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARLEFT, 0, 0);
        }
    }
}

void Motion::charRight(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CHARRIGHTEXTEND, 0, 0);
        }
    }
    else {
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        for (int i = 0; i < count; i++) {
            if (caret < docLen) caret++;
        }
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
    }
}

void Motion::lineUp(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEUPEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0);
        }
    }
}

void Motion::lineDown(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEDOWNEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0);
        }
    }
}

void Motion::wordRight(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDRIGHTEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0);
        }
    }
}

void Motion::wordLeft(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDLEFTEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0);
        }
    }
}

void Motion::wordEnd(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDRIGHTENDEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDRIGHTEND, 0, 0);
        }
    }
}

void Motion::wordEndPrev(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDLEFTENDEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_WORDLEFTEND, 0, 0);
        }
    }
}

void Motion::lineEnd(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEENDEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
        }
    }
}

void Motion::lineStart(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_VCHOMEEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
        }
    }
}

void Motion::nextChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION,
        ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0), 0);

    int foundPos = pos;

    for (int i = 0; i < count; ++i) {
        int searchPos = foundPos + 1;
        bool found = false;
        while (searchPos <= lineEnd) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (c == target) {
                foundPos = searchPos; // f moves TO the target character
                found = true;
                break;
            }
            searchPos++;
        }
        if (!found) return;
    }

    setCursorPosition(hwndEdit, foundPos, false); // false for f motion
}

void Motion::prevChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE,
        ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0), 0);

    int foundPos = pos;

    for (int i = 0; i < count; ++i) {
        int searchPos = foundPos - 1;
        bool found = false;
        while (searchPos >= lineStart) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (c == target) {
                foundPos = searchPos; // F moves TO the target character
                found = true;
                break;
            }
            searchPos--;
        }
        if (!found) return;
    }

    setCursorPosition(hwndEdit, foundPos, false); // false for F motion
}

void Motion::tillChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION,
        ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0), 0);

    int foundPos = pos;

    for (int i = 0; i < count; ++i) {
        int searchPos = foundPos + 1;
        bool found = false;
        while (searchPos <= lineEnd) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (c == target) {
                foundPos = searchPos - 1; // t moves to one position BEFORE the target
                found = true;
                break;
            }
            searchPos++;
        }
        if (!found) return;
    }

    setCursorPosition(hwndEdit, foundPos, true); // true for t motion
}

void Motion::tillCharBack(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE,
        ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0), 0);

    int foundPos = pos;

    for (int i = 0; i < count; ++i) {
        int searchPos = foundPos - 1;
        bool found = false;
        while (searchPos >= lineStart) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (c == target) {
                foundPos = searchPos + 1; // T moves to one position AFTER the target
                found = true;
                break;
            }
            searchPos--;
        }
        if (!found) return;
    }

    setCursorPosition(hwndEdit, foundPos, true); // true for T motion
}

void Motion::setCursorPosition(HWND hwndEdit, int pos, bool isTillMotion) {
    if (state.mode == VISUAL) {
        int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
        int selectionEnd = pos;

        bool isForward = (pos > anchor);

        if (isForward) {
            if (isTillMotion) {
                selectionEnd = pos + 1;
            }
            else {
                selectionEnd = pos + 1;
            }
        }
        else {
        }

        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, selectionEnd);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, selectionEnd, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
    }
}

void Motion::paragraphUp(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARAUPEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARAUP, 0, 0);
        }
    }
}

void Motion::paragraphDown(HWND hwndEdit, int count) {
    if (state.mode == VISUAL) {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARADOWNEXTEND, 0, 0);
        }
    }
    else {
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_PARADOWN, 0, 0);
        }
    }
}

void Motion::gotoLine(HWND hwndEdit, int lineNum) {
    if (state.mode == VISUAL) {
        int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
        ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
        int newPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newPos);
    }
    else {
        ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
    }
}

void Motion::documentStart(HWND hwndEdit) {
    if (state.mode == VISUAL) {
        ::SendMessage(hwndEdit, SCI_DOCUMENTSTARTEXTEND, 0, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_DOCUMENTSTART, 0, 0);
    }
}

void Motion::documentEnd(HWND hwndEdit) {
    if (state.mode == VISUAL) {
        ::SendMessage(hwndEdit, SCI_DOCUMENTENDEXTEND, 0, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_DOCUMENTEND, 0, 0);
    }
}

void Motion::pageUp(HWND hwndEdit) {
    if (state.mode == VISUAL) {
        ::SendMessage(hwndEdit, SCI_PAGEUPEXTEND, 0, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_PAGEUP, 0, 0);
    }
}

void Motion::pageDown(HWND hwndEdit) {
    if (state.mode == VISUAL) {
        ::SendMessage(hwndEdit, SCI_PAGEDOWNEXTEND, 0, 0);
    }
    else {
        ::SendMessage(hwndEdit, SCI_PAGEDOWN, 0, 0);
    }
}

void Motion::matchPair(HWND hwndEdit) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    int matchPos = (int)::SendMessage(hwndEdit, SCI_BRACEMATCH, pos, 0);

    if (matchPos == -1)
        matchPos = (int)::SendMessage(hwndEdit, SCI_BRACEMATCH, pos - 1, 0);

    if (matchPos != -1) {
        if (state.mode == VISUAL) {
            int anchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);

            if (matchPos > anchor)
                ::SendMessage(hwndEdit, SCI_SETSEL, anchor, matchPos + 1);
            else
                ::SendMessage(hwndEdit, SCI_SETSEL, matchPos, anchor);
        }
        else {
            ::SendMessage(hwndEdit, SCI_SETSEL, matchPos, matchPos);
        }
    }
}

void Motion::toggleCase(HWND hwndEdit, int count) {
    int startPos, endPos;

    if (state.mode == VISUAL) {
        startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
        endPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
    }
    else {
        int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        startPos = pos;
        endPos = pos + count;

        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (endPos > docLen) {
            endPos = docLen;
        }
    }

    for (int i = startPos; i < endPos; i++) {
        char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);

        if (isalpha(c)) {
            if (islower(c)) {
                char upperChar = toupper(c);
                ::SendMessage(hwndEdit, SCI_SETTARGETSTART, i, 0);
                ::SendMessage(hwndEdit, SCI_SETTARGETEND, i + 1, 0);
                ::SendMessage(hwndEdit, SCI_REPLACETARGET, 1, (LPARAM)&upperChar);
            }
            else {
                char lowerChar = tolower(c);
                ::SendMessage(hwndEdit, SCI_SETTARGETSTART, i, 0);
                ::SendMessage(hwndEdit, SCI_SETTARGETEND, i + 1, 0);
                ::SendMessage(hwndEdit, SCI_REPLACETARGET, 1, (LPARAM)&lowerChar);
            }
        }
    }
    if (state.mode == NORMAL) {
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, endPos, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, endPos, endPos);
    }
    else {
        ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
    }
}