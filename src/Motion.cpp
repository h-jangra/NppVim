#include "../include/Motion.h"
#include "../plugin/Scintilla.h"

void Motion::charLeft(HWND hwndEdit, int count) {
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    for (int i = 0; i < count; i++) {
        if (caret > 0) caret--;
    }
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);
}

void Motion::charRight(HWND hwndEdit, int count) {
    int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    for (int i = 0; i < count; i++) {
        if (caret < docLen) caret++;
    }
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, caret, 0);
}

void Motion::lineUp(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEUP, 0, 0);
    }
}

void Motion::lineDown(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEDOWN, 0, 0);
    }
}

void Motion::wordRight(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDRIGHT, 0, 0);
    }
}

void Motion::wordLeft(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDLEFT, 0, 0);
    }
}

void Motion::wordEnd(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDRIGHTEND, 0, 0);
    }
}

void Motion::wordEndPrev(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_WORDLEFTEND, 0, 0);
    }
}

void Motion::lineEnd(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_LINEEND, 0, 0);
    }
}

void Motion::lineStart(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_VCHOME, 0, 0);
    }
}

void Motion::nextChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION,
        ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0), 0);
    for (int i = 0; i < count; ++i) {
        int found = -1;
        for (int j = pos + 1; j < lineEnd; ++j) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, j, 0);
            if (c == target) { found = j; break; }
        }
        if (found != -1) pos = found;
    }

    ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
}

void Motion::prevChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE,
        ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0), 0);
    for (int i = 0; i < count; ++i) {
        int found = -1;
        for (int j = pos - 1; j >= lineStart; --j) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, j, 0);
            if (c == target) { found = j; break; }
        }
        if (found != -1) pos = found;
    }
    ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
}

void Motion::paragraphUp(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_PARAUP, 0, 0);
    }
}

void Motion::paragraphDown(HWND hwndEdit, int count) {
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwndEdit, SCI_PARADOWN, 0, 0);
    }
}

void Motion::gotoLine(HWND hwndEdit, int lineNum) {
    ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
}

void Motion::documentStart(HWND hwndEdit) {
    ::SendMessage(hwndEdit, SCI_DOCUMENTSTART, 0, 0);
}

void Motion::documentEnd(HWND hwndEdit) {
    ::SendMessage(hwndEdit, SCI_DOCUMENTEND, 0, 0);
}
