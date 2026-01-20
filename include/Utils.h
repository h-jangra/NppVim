#pragma once
#include "../plugin/PluginInterface.h"
#include "../plugin/Scintilla.h"
#include <windows.h>
#include <string>
#include <utility>

struct VimState;

class Keymap;

struct BlockSelection {
    int startLine;
    int endLine;
    int startCol;
    int endCol;
};

class Utils {
public:
    static HWND getCurrentScintillaHandle();
    static void setStatus(const TCHAR* msg);
    static void clearSearchHighlights(HWND hwndEdit);
    static std::pair<int, int> findWordBounds(HWND hwndEdit, int pos);
    static std::pair<int, int> findWordBoundsEx(HWND hwndEdit, int pos, bool bigWord);
    static int findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar);
    static std::pair<int, int> findQuoteBounds(HWND hwndEdit, int pos, char quoteChar);

    static void updateSearchHighlight(HWND hwndEdit, const std::string& searchTerm, bool useRegex);
    static void showCurrentMatchPosition(HWND hwndEdit, const std::string& searchTerm, bool useRegex);

    static int countSearchMatches(HWND hwndEdit, const std::string& searchTerm, bool useRegex);

    static void handleIndent(HWND hwndEdit, int count);
    static void handleUnindent(HWND hwndEdit, int count);
    static void handleAutoIndent(HWND hwndEdit, int count);

    static int caretPos(HWND hwnd);
    static int caretLine(HWND hwnd);
    static int lineStart(HWND hwnd, int line);
    static int lineEnd(HWND hwnd, int line);
    static int lineCount(HWND hwnd);

    static std::pair<int,int> lineRange(HWND hwnd, int line, bool includeNewline);
    static std::pair<int,int> lineRangeFromPos(HWND hwnd, int pos, bool includeNewline);

    static void beginUndo(HWND hwnd);
    static void endUndo(HWND hwnd);

    static void select(HWND hwnd, int start, int end);
    static void cut(HWND hwnd, int start, int end);
    static void copy(HWND hwnd, int start, int end);
    static void clear(HWND hwnd, int start, int end);

    static void toUpper(HWND hwnd, int start, int end);
    static void toLower(HWND hwnd, int start, int end);

    static void replaceChar(HWND hwnd, int pos, char ch);
    static void replaceRange(HWND hwnd, int start, int end, char ch);

    static BlockSelection blockSelection(HWND hwnd);

    static void setBlockSelection(HWND hwnd, int anchor, int caret);
    static void clearBlockSelection(HWND hwnd);

    static void forEachBlockCell(HWND hwnd, const BlockSelection& blk, void (*fn)(HWND, int));

    static void pasteAfter(HWND hwnd, int count, bool linewise);
    static void pasteBefore(HWND hwnd, int count, bool linewise);

    static void joinLines(HWND hwnd, int startLine, int count, bool withSpace);

    static void charSearch(HWND hwnd, VimState& state, char type, char ch, int count);

    static void setClipboardText(const std::string& text);

    static std::string buildTutorText();
    static int getCharBlocking();

    static std::string getRegisterContent(char reg);
    static void setRegisterContent(char reg, const std::string &content);
    static void appendToRegister(char reg, const std::string &content);
    static bool isValidRegister(char c);
    static char getCurrentRegister();
    static void setCurrentRegister(char reg);

  private:
    static NppData nppData;
    friend void setNppData(NppData data);
};

