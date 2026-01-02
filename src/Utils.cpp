// Utils.cpp
#include "../include/Utils.h"
#include "../include/NppVim.h"
#include "../include/NormalMode.h"
#include "../include/VisualMode.h"
#include "../plugin/Notepad_plus_msgs.h"

NppData Utils::nppData;

extern NormalMode* g_normalMode;
extern VisualMode* g_visualMode;

HWND Utils::getCurrentScintillaHandle()
{
  int which = 0;
  ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
  return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void Utils::setStatus(const TCHAR *msg)
{
  ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)msg);
}

void Utils::clearSearchHighlights(HWND hwndEdit)
{
  if (!hwndEdit)
    return;

  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
  ::SendMessage(hwndEdit, SCI_INDICATORCLEARRANGE, 0, docLen);
}

std::pair<int, int> Utils::findWordBounds(HWND hwndEdit, int pos)
{
  // For lowercase 'w'
  return findWordBoundsEx(hwndEdit, pos, false);
}

std::pair<int, int> Utils::findWordBoundsEx(HWND hwndEdit, int pos, bool bigWord) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    if (pos >= docLen) return {pos, pos};
    
    int style = bigWord ? 1 : 0;
    
    int wordStart = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, style);
    int wordEnd = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, style);
    
    return {wordStart, wordEnd};
}

int Utils::findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar)
{
  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  if (pos >= docLen)
    return -1;

  char currentChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);

  if (currentChar == closeChar)
  {
    int depth = 1;
    for (int i = pos - 1; i >= 0; i--)
    {
      char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
      if (ch == closeChar)
        depth++;
      else if (ch == openChar)
      {
        depth--;
        if (depth == 0)
          return i;
      }
    }
  }
  else
  {
    int depth = 1;
    for (int i = pos + 1; i < docLen; i++)
    {
      char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
      if (ch == openChar)
        depth++;
      else if (ch == closeChar)
      {
        depth--;
        if (depth == 0)
          return i;
      }
    }
  }
  return -1;
}

std::pair<int, int> Utils::findQuoteBounds(HWND hwndEdit, int pos, char quoteChar)
{
  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  int start = -1, end = -1;

  for (int i = pos; i >= 0; i--)
  {
    char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
    if (ch == quoteChar)
    {
      start = i;
      break;
    }
  }

  if (start != -1)
  {
    for (int i = start + 1; i < docLen; i++)
    {
      char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
      if (ch == quoteChar)
      {
        end = i;
        break;
      }
    }
  }

  return {start, end};
}

void Utils::updateSearchHighlight(HWND hwndEdit, const std::string &searchTerm, bool useRegex)
{
  if (searchTerm.empty())
  {
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
  while (pos < docLen)
  {
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                   (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found == -1)
      break;

    int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETINDICATORCURRENT, 0, 0);
    ::SendMessage(hwndEdit, SCI_INDICATORFILLRANGE, start, end - start);

    pos = end;
    if (end <= start)
      pos = start + 1;
  }
}

void Utils::showCurrentMatchPosition(HWND hwndEdit, const std::string &searchTerm, bool useRegex)
{
  if (searchTerm.empty())
    return;

  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

  int flags = (useRegex ? SCFIND_REGEXP : 0);
  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

  int totalMatches = 0;
  int currentMatchIndex = 0;
  int pos = 0;

  while (pos < docLen)
  {
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                   (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found == -1)
      break;

    int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

    totalMatches++;

    if (currentPos >= start && currentPos <= end && currentMatchIndex == 0)
    {
      currentMatchIndex = totalMatches;
    }

    pos = end;
    if (end <= start)
      pos = start + 1;
  }

  if (totalMatches > 0 && currentMatchIndex > 0)
  {
    std::string status = "Match " + std::to_string(currentMatchIndex) + " of " + std::to_string(totalMatches);
    std::wstring wstatus(status.begin(), status.end());
    setStatus(wstatus.c_str());
  }
}

int Utils::countSearchMatches(HWND hwndEdit, const std::string &searchTerm, bool useRegex)
{
  if (!hwndEdit || searchTerm.empty())
    return 0;

  int flags = (useRegex ? SCFIND_REGEXP : 0);
  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  int count = 0;
  int pos = 0;

  while (pos < docLen)
  {
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, pos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                   (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found == -1)
      break;

    count++;
    pos = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
  }

  return count;
}

void Utils::handleIndent(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    ::SendMessage(hwndEdit, SCI_TAB, 0, 0);

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

}

void Utils::handleUnindent(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    ::SendMessage(hwndEdit, SCI_BACKTAB, 0, 0);

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

}

void Utils::handleAutoIndent(HWND hwndEdit, int count) {
    ::SendMessage(hwndEdit, SCI_BEGINUNDOACTION, 0, 0);

    int lineStart = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
                      (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0), 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
                    (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0), 0);

    for (int line = lineStart; line <= lineEnd; line++) {
        int lineStartPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
        int lineEndPos = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        int firstNonSpace = lineStartPos;
        while (firstNonSpace < lineEndPos) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, firstNonSpace, 0);
            if (ch != ' ' && ch != '\t') break;
            firstNonSpace++;
        }

        if (firstNonSpace > lineStartPos) {
            ::SendMessage(hwndEdit, SCI_DELETERANGE, lineStartPos, firstNonSpace - lineStartPos);
        }
    }

    ::SendMessage(hwndEdit, SCI_ENDUNDOACTION, 0, 0);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }
}

int Utils::caretPos(HWND hwnd) {
    return (int)::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
}

int Utils::caretLine(HWND hwnd) {
    int pos = caretPos(hwnd);
    return (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
}

int Utils::lineStart(HWND hwnd, int line) {
    return (int)::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
}

int Utils::lineEnd(HWND hwnd, int line) {
    return (int)::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0);
}

int Utils::lineCount(HWND hwnd) {
    return (int)::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
}

std::pair<int,int> Utils::lineRange(HWND hwnd, int line, bool includeNewline) {
    int start = lineStart(hwnd, line);
    int total = lineCount(hwnd);
    int end = (line < total - 1)
        ? lineStart(hwnd, line + 1)
        : lineEnd(hwnd, line) + 1;
    if (!includeNewline && end > start) end--;
    return {start, end};
}

std::pair<int,int> Utils::lineRangeFromPos(HWND hwnd, int pos, bool includeNewline) {
    int line = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
    return lineRange(hwnd, line, includeNewline);
}

void Utils::beginUndo(HWND hwnd) {
    ::SendMessage(hwnd, SCI_BEGINUNDOACTION, 0, 0);
}

void Utils::endUndo(HWND hwnd) {
    ::SendMessage(hwnd, SCI_ENDUNDOACTION, 0, 0);
}

void Utils::select(HWND hwnd, int a, int b) {
    ::SendMessage(hwnd, SCI_SETSEL, a, b);
}

void Utils::cut(HWND hwnd, int a, int b) {
    select(hwnd, a, b);
    ::SendMessage(hwnd, SCI_CUT, 0, 0);
}

void Utils::copy(HWND hwnd, int a, int b) {
    select(hwnd, a, b);
    ::SendMessage(hwnd, SCI_COPY, 0, 0);
}

void Utils::clear(HWND hwnd, int a, int b) {
    select(hwnd, a, b);
    ::SendMessage(hwnd, SCI_CLEAR, 0, 0);
}

void Utils::toUpper(HWND hwnd, int a, int b) {
    for (int i = a; i < b; i++) {
        char ch = (char)::SendMessage(hwnd, SCI_GETCHARAT, i, 0);
        if (std::islower((unsigned char)ch)) {
            ::SendMessage(hwnd, SCI_SETTARGETRANGE, i, i + 1);
            char c = (char)std::toupper(ch);
            ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&c);
        }
    }
}

void Utils::toLower(HWND hwnd, int a, int b) {
    for (int i = a; i < b; i++) {
        char ch = (char)::SendMessage(hwnd, SCI_GETCHARAT, i, 0);
        if (std::isupper((unsigned char)ch)) {
            ::SendMessage(hwnd, SCI_SETTARGETRANGE, i, i + 1);
            char c = (char)std::tolower(ch);
            ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&c);
        }
    }
}

void Utils::replaceChar(HWND hwnd, int pos, char ch) {
    ::SendMessage(hwnd, SCI_SETTARGETRANGE, pos, pos + 1);
    ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&ch);
}

void Utils::replaceRange(HWND hwnd, int a, int b, char ch) {
    for (int i = a; i < b; i++) {
        char c = (char)::SendMessage(hwnd, SCI_GETCHARAT, i, 0);
        if (c != '\r' && c != '\n') {
            replaceChar(hwnd, i, ch);
        }
    }
}

BlockSelection Utils::blockSelection(HWND hwnd) {
    int a = (int)::SendMessage(hwnd, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
    int c = (int)::SendMessage(hwnd, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);

    int la = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, a, 0);
    int lc = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, c, 0);

    int ca = (int)::SendMessage(hwnd, SCI_GETCOLUMN, a, 0);
    int cc = (int)::SendMessage(hwnd, SCI_GETCOLUMN, c, 0);

    return {
        (std::min)(la, lc),
        (std::max)(la, lc),
        (std::min)(ca, cc),
        (std::max)(ca, cc)
    };
}

void Utils::setBlockSelection(HWND hwnd, int anchor, int caret) {
    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);
    ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
    ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, caret, 0);
}

void Utils::clearBlockSelection(HWND hwnd) {
    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
}

void Utils::forEachBlockCell(HWND hwnd, const BlockSelection& blk, void (*fn)(HWND,int)) {
    for (int line = blk.startLine; line <= blk.endLine; line++) {
        int ls = lineStart(hwnd, line);
        int le = lineEnd(hwnd, line);
        int cs = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, line, blk.startCol);
        int ce = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, line, blk.endCol);
        if (cs > le) cs = le;
        if (ce > le) ce = le;
        for (int p = cs; p < ce; p++) fn(hwnd, p);
    }
}

void Utils::pasteAfter(HWND hwnd, int count, bool linewise) {
    int pos = caretPos(hwnd);
    int line = caretLine(hwnd);
    if (linewise) {
        auto r = lineRange(hwnd, line, true);
        ::SendMessage(hwnd, SCI_GOTOPOS, r.second, 0);
    } else {
        ::SendMessage(hwnd, SCI_GOTOPOS, (int)::SendMessage(hwnd, SCI_POSITIONAFTER, pos, 0), 0);
    }
    for (int i = 0; i < count; i++) { ::SendMessage(hwnd, SCI_PASTE, 0, 0); }

     int newPos = caretPos(hwnd);
    ::SendMessage(hwnd, SCI_SETSEL, newPos, newPos);
}

void Utils::pasteBefore(HWND hwnd, int count, bool linewise) {
    int pos = caretPos(hwnd);
    int line = caretLine(hwnd);
    if (linewise) {
        ::SendMessage(hwnd, SCI_GOTOPOS, lineStart(hwnd, line), 0);
    } else {
        ::SendMessage(hwnd, SCI_GOTOPOS, pos, 0);
    }
    for (int i = 0; i < count; i++) {
        ::SendMessage(hwnd, SCI_PASTE, 0, 0);
    }
}

void Utils::joinLines(HWND hwnd, int startLine, int count, bool withSpace) {
    for (int i = 0; i < count; i++) {
        int end = lineEnd(hwnd, startLine);
        int next = lineStart(hwnd, startLine + 1);
        if (next <= end) break;
        select(hwnd, end, next);
        ::SendMessage(hwnd, SCI_REPLACESEL, 0, (LPARAM)(withSpace ? " " : ""));
    }
}

void Utils::charSearch(HWND hwnd, VimState& state, char type, char ch, int count) {
    bool forward = (type == 'f' || type == 't');
    bool till = (type == 't' || type == 'T');

    if (till) {
        if (forward) Motion::tillChar(hwnd, count, ch);
        else Motion::tillCharBack(hwnd, count, ch);
    } else {
        if (forward) Motion::nextChar(hwnd, count, ch);
        else Motion::prevChar(hwnd, count, ch);
    }

    state.lastSearchChar = ch;
    state.lastSearchForward = forward;
    state.lastSearchTill = till;
}

void Utils::setClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (h) {
        char* p = (char*)GlobalLock(h);
        memcpy(p, text.c_str(), text.size() + 1);
        GlobalUnlock(h);
        SetClipboardData(CF_TEXT, h);
    }
    CloseClipboard();
}

static void appendSection(std::string& out, const std::string& title, const Keymap& km) {
    out += "\n";
    out += title;
    out += "\n";
    out += std::string(title.size(), '-');
    out += "\n";

    size_t maxKey = 0;
    for (const auto& b : km.getBindings())
        maxKey = (std::max)(maxKey, b.keys.size());

    for (const auto& b : km.getBindings()) {
        out += ":";
        out += b.keys;
        out += std::string(maxKey - b.keys.size() + 2, ' ');
        out += "- ";
        out += b.desc;
        out += "\n";
    }
}

std::string Utils::buildTutorText() {
    std::string out;

    out += "NppVim Tutor\n";
    out += "============\n\n";

    auto append = [&](const char* title, const Keymap* km) {
        if (!km) return;

        out += title;
        out += "\n";
        out += std::string(strlen(title), '-') + "\n";

        size_t pad = 0;
        for (const auto& b : km->getBindings())
            pad = (std::max)(pad, b.keys.size());

        for (const auto& b : km->getBindings()) {
            out += "  ";
            out += b.keys;
            out += std::string(pad - b.keys.size() + 2, ' ');
            out += "- ";
            out += b.desc;
            out += "\n";
        }
        out += "\n";
    };

    append("Normal Mode", g_normalKeymap.get());
    append("Visual Mode", g_visualKeymap.get());
    append("Command Mode", g_commandKeymap.get());
    out += "\n";
    out += ":s/foo/bar/g        - Replace all matches in line\n";
    out += ":s/foo/bar/c        - Replace with confirmation\n";
    out += ":s/foo/bar/gc       - Replace all in line (confirm)\n";
    out += ":s/foo/bar/i        - Case-insensitive replace\n";
    out += ":s/foo/bar/l        - Literal (no regex) replace\n";
    out += ":%s/foo/bar/gc      - Replace in whole file (confirm)\n";

    return out;
}

int Utils::getCharBlocking() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_CHAR)
            return (int)msg.wParam;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
