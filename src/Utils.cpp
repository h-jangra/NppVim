//Utils.cpp
#include "../include/Utils.h"
#include "../plugin/Notepad_plus_msgs.h"

NppData Utils::nppData;

HWND Utils::getCurrentScintillaHandle() {
    int which = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void Utils::setStatus(const TCHAR* msg) {
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)msg);
}

void Utils::clearSearchHighlights(HWND hwndEdit) {
    if (!hwndEdit) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
    ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);
}

std::string Utils::getClipboardText(HWND hwnd) {
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

std::pair<int, int> Utils::findWordBounds(HWND hwndEdit, int pos) {
    int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
    int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
    return { start, end };
}

int Utils::findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    if (pos >= docLen) return -1;

    char currentChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);

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

std::pair<int, int> Utils::findQuoteBounds(HWND hwndEdit, int pos, char quoteChar) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int start = -1, end = -1;

    for (int i = pos; i >= 0; i--) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
        if (ch == quoteChar) {
            start = i;
            break;
        }
    }

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

void Utils::updateSearchHighlight(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (searchTerm.empty()) {
        clearSearchHighlights(hwndEdit);
        return;
    }

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
    ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);

    ::SendMessage(hwndEdit, SCI_INDICSETSTYLE, 0, INDIC_ROUNDBOX);
    ::SendMessage(hwndEdit, SCI_INDICSETFORE, 0, RGB(255, 255, 0));
    ::SendMessage(hwndEdit, SCI_INDICSETALPHA, 0, 100);
    ::SendMessage(hwndEdit, SCI_INDICSETOUTLINEALPHA, 0, 255);

    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    int pos = 0;
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

        pos = end;
        if (end <= start) pos = start + 1;
    }
}

void Utils::showCurrentMatchPosition(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (searchTerm.empty()) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    int totalMatches = 0;
    int currentMatchIndex = 0;
    int pos = 0;

    while (pos < docLen) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

        if (found == -1) break;

        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

        totalMatches++;

        if (currentPos >= start && currentPos <= end && currentMatchIndex == 0) {
            currentMatchIndex = totalMatches;
        }

        pos = end;
        if (end <= start) pos = start + 1;
    }

    if (totalMatches > 0 && currentMatchIndex > 0) {
        std::string status = "Match " + std::to_string(currentMatchIndex) + " of " + std::to_string(totalMatches);
        std::wstring wstatus(status.begin(), status.end());
        setStatus(wstatus.c_str());
    }
}

int Utils::countSearchMatches(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (!hwndEdit || searchTerm.empty()) return 0;

    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int count = 0;
    int pos = 0;

    while (pos < docLen) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

        if (found == -1) break;

        count++;
        pos = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
    }

    return count;
}
