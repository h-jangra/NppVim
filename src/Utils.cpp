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

std::pair<int, int> Utils::findWordBoundsEx(HWND hwndEdit, int pos, bool bigWord)
{
  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  if (pos >= docLen)
    return {pos, pos};

  char charAtPos = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);

  if (charAtPos == ' ' || charAtPos == '\t' || charAtPos == '\r' || charAtPos == '\n')
  {
    return {pos, pos};
  }

  int start = pos;
  int end = pos;

  if (bigWord)
  {
    // Find start
    while (start > 0)
    {
      char prevChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0);
      if (prevChar == ' ' || prevChar == '\t' || prevChar == '\r' || prevChar == '\n')
      {
        break;
      }
      start--;
    }

    // Find end
    while (end < docLen)
    {
      char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0);
      if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
      {
        break;
      }
      end++;
    }
  }
  else
  {
    // For w: only alphanumeric and underscore
    // Find start
    while (start > 0)
    {
      char prevChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0);
      if (!(std::isalnum(static_cast<unsigned char>(prevChar)) || prevChar == '_'))
      {
        break;
      }
      start--;
    }

    // Find end
    while (end < docLen)
    {
      char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0);
      if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_'))
      {
        break;
      }
      end++;
    }
  }

  return {start, end};
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
