#pragma once
#include "../plugin/PluginInterface.h"
#include "../plugin/Scintilla.h"
#include <windows.h>
#include <string>
#include <utility>

class Utils {
public:
    static HWND getCurrentScintillaHandle();
    static void setStatus(const TCHAR* msg);
    static void clearSearchHighlights(HWND hwndEdit);
    static std::string getClipboardText(HWND hwnd);
    static std::pair<int, int> findWordBounds(HWND hwndEdit, int pos);
    static int findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar);
    static std::pair<int, int> findQuoteBounds(HWND hwndEdit, int pos, char quoteChar);

    static void updateSearchHighlight(HWND hwndEdit, const std::string& searchTerm, bool useRegex);
    static void showCurrentMatchPosition(HWND hwndEdit, const std::string& searchTerm, bool useRegex);

    static int countSearchMatches(HWND hwndEdit, const std::string& searchTerm, bool useRegex);

private:
    static NppData nppData;
    friend void setNppData(NppData data);
};
