// Utils.cpp
#include "Utils.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include "NppVim.h"
#include "NormalMode.h"
#include "VisualMode.h"
#include "Notepad_plus_msgs.h"

#include "ConfigManager.h"

NppData Utils::nppData;

extern NormalMode* g_normalMode;
extern VisualMode* g_visualMode;

int Utils::sci(HWND h, int msg, WPARAM w, LPARAM l) {
    return (int)::SendMessage(h, msg, w, l);
}

HWND Utils::getCurrentScintillaHandle()
{
  int which = 0;
  ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
  return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void Utils::setStatus(const TCHAR *msg)
{
  if (!ConfigManager::getInstance().isShowStatusBar())
    return;
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
    
    // Scintilla SCI_WORDSTARTPOSITION/ENDPOSITION:
    // style (bool onlyWordCharacters):
    // true: use only word characters (small word)
    // false: use any non-whitespace characters (big word)
    bool onlyWordChars = !bigWord;
    
    int wordStart = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, onlyWordChars);
    int wordEnd = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, onlyWordChars);
    
    return {wordStart, wordEnd};
}

int Utils::findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar) {
    int m = (int)SendMessage(hwndEdit, SCI_BRACEMATCH, pos, 0);
    if (m == -1)
        m = (int)SendMessage(hwndEdit, SCI_BRACEMATCH, pos - 1, 0);
    return m;
}

std::pair<int, int> Utils::findQuoteBounds(HWND hwndEdit, int pos, char quoteChar) {
    int line = ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = ::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int lineEnd = ::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    int startPos = -1;
    int endPos = -1;
    int minDistance = lineEnd - lineStart;

    bool inQuote = false;
    int quoteStart = -1;

    for (int i = lineStart; i <= lineEnd; i++) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
        
        if (ch == quoteChar) {
            if (!inQuote) {
                quoteStart = i;
                inQuote = true;
            } else {
                if (pos >= quoteStart && pos <= i) {
                    return { quoteStart, i };
                } else {
                    int distance = (std::min)(abs(pos - quoteStart), abs(pos - i));
                    if (distance < minDistance) {
                        minDistance = distance;
                        startPos = quoteStart;
                        endPos = i;
                    }
                }
                inQuote = false;
            }
        }
    }

    if (startPos == -1 || endPos == -1) {
        return { -1, -1 };
    }

    return { startPos, endPos };
}

void Utils::updateSearchHighlight(HWND hwndEdit, const std::string &searchTerm, int searchFlags)
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

  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, searchFlags, 0);

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

void Utils::showCurrentMatchPosition(HWND hwndEdit, const std::string &searchTerm, int searchFlags)
{
  if (searchTerm.empty())
    return;

  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, searchFlags, 0);

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

int Utils::countSearchMatches(HWND hwndEdit, const std::string &searchTerm, int searchFlags)
{
  if (!hwndEdit || searchTerm.empty())
    return 0;

  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, searchFlags, 0);

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

int Utils::caretColumn(HWND hwnd) {
    return (int)SendMessage(hwnd, SCI_GETCOLUMN, caretPos(hwnd), 0);
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
    select(hwnd, a, b);
    SendMessage(hwnd, SCI_UPPERCASE, 0, 0);
}

void Utils::toLower(HWND hwnd, int a, int b) {
    select(hwnd, a, b);
    SendMessage(hwnd, SCI_LOWERCASE, 0, 0);
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
        if (startLine + i + 1 >= lineCount(hwnd)) break;
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
    out += "Welcome to NppVim! This tutor will guide you through the basics.\n\n";

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

    append("1. Normal Mode (Navigation & Editing)", g_normalKeymap.get());
    append("2. Visual Mode (Selection)", g_visualKeymap.get());
    append("3. Command Mode (System Commands)", g_commandKeymap.get());

    out += "4. Search and Replace\n";
    out += "---------------------\n";
    out += "  /pattern           - Search forward for pattern\n";
    out += "  ?pattern           - Search backward for pattern\n";
    out += "  n                  - Next match\n";
    out += "  N                  - Previous match\n";
    out += "  :s/old/new/        - Replace first 'old' with 'new' in current line\n";
    out += "  :s/old/new/g       - Replace all 'old' with 'new' in current line\n";
    out += "  :%s/old/new/g      - Replace all 'old' with 'new' in entire file\n";
    out += "  :%s/old/new/gc     - Replace all with confirmation\n";
    out += "\n";

    out += "5. Configuration and Persistence\n";
    out += "--------------------------------\n";
    out += "  :set <option>      - Set a temporary option\n";
    out += "  :set nu            - Show line numbers\n";
    out += "  :set nonu          - Hide line numbers\n";
    out += "\n";
    out += "  NppVim looks for configuration in two files in the plugin folder:\n";
    out += "  1. config.ini      - Basic settings and state\n";
    out += "  2. nppvim.rc       - Custom mappings and commands (Vim script style)\n";
    out += "\n";
    out += "  Use ':editrc' or ':editini' to modify these files directly.\n";
    out += "  Use ':reload' to apply changes made to nppvim.rc.\n";
    out += "\n";

    out += "6. Tips\n";
    out += "-------\n";
    out += "  - Press 'ESC' to return to Normal mode from any other mode.\n";
    out += "  - Use 'u' to undo and 'Ctrl+R' to redo changes.\n";
    out += "  - Use ':help' or ':h' to see the full list of commands.\n";

    return out;
}

std::string Utils::getPluginPath() {
    TCHAR path[MAX_PATH];
    HMODULE hModule = GetModuleHandle(TEXT("NppVim.dll"));
    if (hModule) {
        GetModuleFileName(hModule, path, MAX_PATH);
        std::wstring wFull(path);
        
        // Correct conversion from wstring to string
        int size = WideCharToMultiByte(CP_UTF8, 0, wFull.c_str(), -1, NULL, 0, NULL, NULL);
        if (size > 0) {
            std::string full(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, wFull.c_str(), -1, &full[0], size, NULL, NULL);
            while (!full.empty() && full.back() == '\0') full.pop_back();

            size_t lastSlash = full.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                return full.substr(0, lastSlash);
            }
        }
    }
    return "";
}

std::string Utils::readPluginFile(const std::string& filename) {
    std::string path = getPluginPath() + "\\" + filename;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return "";
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
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

std::string Utils::getRegisterContent(char reg) {
    if (state.registers.find(reg) != state.registers.end()) {
        return state.registers[reg];
    }
    return "";
}

void Utils::setRegisterContent(char reg, const std::string& content) {
    state.registers[reg] = content;
}

void Utils::appendToRegister(char reg, const std::string& content) {
    if (state.registers.find(reg) != state.registers.end()) {
        state.registers[reg] += content;
    } else {
        state.registers[reg] = content;
    }
}

bool Utils::isValidRegister(char c) {
    // Valid registers: a-z (named), 0-9 (numbered), " (default), _ (blackhole), 
    // +/* (clipboard), / (search), : (command), . (last inserted)
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || 
           c == '"' || c == '_' || c == '+' || c == '*' || 
           c == '/' || c == ':' || c == '.';
}

char Utils::getCurrentRegister() {
    return state.defaultRegister;
}

void Utils::setCurrentRegister(char reg) {
    if (isValidRegister(reg)) {
        state.defaultRegister = reg;
    }
}

void Utils::storeRegister(char reg, const std::string& text, bool syncClipboard){
    if (reg == '+' || reg == '*') {
        Utils::setClipboardText(text);
        return;
    }

    Utils::setRegisterContent(reg, text);

    if (syncClipboard && reg == '"') {
        Utils::setClipboardText(text);
    }
}

static std::unordered_map<wchar_t, char> g_langmap;

void Utils::parseLangmap(const std::string& langmapStr) {
    g_langmap.clear();
    if (langmapStr.empty()) return;

    // We need to decode the UTF-8 langmapStr into wide characters
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, langmapStr.c_str(), -1, NULL, 0);
    if (wideLen <= 0) return;
    std::wstring wStr(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, langmapStr.c_str(), -1, &wStr[0], wideLen);
    
    // Remove the null terminator added by MultiByteToWideChar
    while (!wStr.empty() && wStr.back() == L'\0') wStr.pop_back();

    std::vector<std::wstring> parts;
    std::wstring current;
    bool escaped = false;
    for (wchar_t c : wStr) {
        if (escaped) {
            current += c;
            escaped = false;
        } else if (c == L'\\') {
            escaped = true;
        } else if (c == L',') {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);

    for (const auto& part : parts) {
        if (part.empty()) continue;
        size_t semi = part.find(L';');
        if (semi != std::wstring::npos) {
            std::wstring from = part.substr(0, semi);
            std::wstring to = part.substr(semi + 1);
            for (size_t i = 0; i < from.length() && i < to.length(); ++i) {
                g_langmap[from[i]] = (char)to[i];
            }
        } else {
            for (size_t i = 0; i + 1 < part.length(); i += 2) {
                g_langmap[part[i]] = (char)part[i+1];
            }
        }
    }
}

std::string Utils::toUtf8(wchar_t wch) {
    if (wch == 0) return "";
    wchar_t buf[2] = {wch, 0};
    return toUtf8(std::wstring(buf));
}

std::string Utils::toUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
    while (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}

std::string Utils::trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, (last - first + 1));
}

std::string Utils::getTextRange(HWND h, int start, int end) {
    if (start >= end) return "";
    std::vector<char> buffer(end - start + 1);
    Sci_TextRangeFull tr;
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = buffer.data();
    ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
    return std::string(buffer.data(), end - start);
}

void Utils::rot13(HWND hwnd, int start, int end) {
    if (start >= end) return;
    std::string text = getTextRange(hwnd, start, end);
    for (char &c : text) {
        if (c >= 'a' && c <= 'z') c = 'a' + (c - 'a' + 13) % 26;
        else if (c >= 'A' && c <= 'Z') c = 'A' + (c - 'A' + 13) % 26;
    }
    ::SendMessage(hwnd, SCI_SETTARGETSTART, start, 0);
    ::SendMessage(hwnd, SCI_SETTARGETEND, end, 0);
    ::SendMessage(hwnd, SCI_REPLACETARGET, text.size(), (LPARAM)text.c_str());
}

char Utils::applyLangmap(wchar_t c) {
    auto it = g_langmap.find(c);
    if (it != g_langmap.end()) return it->second;
    if (c <= 0x7F) return (char)c;
    return 0;
}

HKL Utils::resolveLayout(const std::string& layoutName) {
    if (layoutName == "system" || layoutName.empty()) return nullptr;

    // Mapping some common names to LCIDs
    static std::map<std::string, std::string> nameToLcid = {
        {"en-US", "00000409"}, {"en-GB", "00000809"},
        {"ru-RU", "00000419"}, {"hi-IN", "00000439"},
        {"fr-FR", "0000040c"}, {"de-DE", "00000407"},
        {"es-ES", "0000040a"}, {"it-IT", "00000410"},
        {"ja-JP", "00000411"}, {"ko-KR", "00000412"},
        {"zh-CN", "00000804"}
    };

    std::string lcid = "00000409"; // Default to US English
    auto it = nameToLcid.find(layoutName);
    if (it != nameToLcid.end()) {
        lcid = it->second;
    } else if (layoutName.length() == 8 && std::all_of(layoutName.begin(), layoutName.end(), ::isxdigit)) {
        lcid = layoutName;
    }

    std::wstring wlcid(lcid.begin(), lcid.end());
    return ::LoadKeyboardLayout(wlcid.c_str(), KLF_ACTIVATE);
}

std::string Utils::translateKeyNotation(const std::string& input) {
    std::string result;
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '<') {
            size_t end = input.find('>', i);
            if (end != std::string::npos) {
                std::string token = input.substr(i + 1, end - i - 1);
                std::string lowerToken = token;
                std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), ::tolower);
                
                char translatedChar = 0;
                bool found = false;

                if (lowerToken == "cr" || lowerToken == "enter" || lowerToken == "return") {
                    translatedChar = '\x0D';
                    found = true;
                } else if (lowerToken == "tab") {
                    translatedChar = '\x09';
                    found = true;
                } else if (lowerToken == "bs" || lowerToken == "backspace") {
                    translatedChar = '\x08';
                    found = true;
                } else if (lowerToken == "space") {
                    translatedChar = '\x20';
                    found = true;
                } else if (lowerToken == "esc" || lowerToken == "escape") {
                    translatedChar = '\x1B';
                    found = true;
                } else if (lowerToken == "leader") {
                    translatedChar = '\\';
                    found = true;
                } else if (lowerToken.find("c-") == 0 && lowerToken.size() > 2) {
                    char c = lowerToken[2];
                    if (c >= 'a' && c <= 'z') {
                        translatedChar = c - 'a' + 1;
                        found = true;
                    }
                } else if (lowerToken.find("s-") == 0 && lowerToken.size() > 2) {
                    std::string shiftKey = lowerToken.substr(2);
                    if (shiftKey == "tab") {
                        translatedChar = (char)0x8C;
                        found = true;
                    }
                } else if (lowerToken.size() >= 2 && lowerToken[0] == 'f') {
                    try {
                        int fNum = std::stoi(lowerToken.substr(1));
                        if (fNum >= 1 && fNum <= 12) {
                            translatedChar = (char)(0x80 + (fNum - 1));
                            found = true;
                        }
                    } catch (...) {}
                }

                if (found) {
                    result += translatedChar;
                    i = end + 1;
                    continue;
                }
            }
        }
        result += input[i];
        i++;
    }
    return result;
}