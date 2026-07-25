#pragma once
#include "NppVim.h"
#include <windows.h>
#include <string>

#define IND_SUB_MATCH    20
#define IND_SUB_REPL     21

struct SubstitutionParsed {
    bool isSubstitution = false;
    std::string rangeStr;
    int startLine = 0;
    int endLine = 0;
    char delimiter = '/';
    std::string pattern;
    std::string replacement;
    std::string flags;
    bool hasSecondDelimiter = false;
    bool hasThirdDelimiter = false;
    bool useRegex = true;
    bool caseInsensitive = false;
    bool replaceAll = false;
    bool confirmEach = false;
    bool countOnly = false;
    bool suppressError = false;
};

class CommandMode {
public:
    CommandMode(VimState& state);

    void enter(char prompt);
    void exit();
    void handleKey(HWND hwndEdit, wchar_t wChar);
    void handleBackspace(HWND hwndEdit);
    void handleEnter(HWND hwndEdit);
    void updateStatus();

    // Search functions
    void performSearch(HWND hwndEdit, const std::string& searchTerm, int searchFlags = 0);
    void searchNext(HWND hwndEdit);
    void searchPrevious(HWND hwndEdit);
    void handleMarksCommand(HWND hwndEdit, const std::string& commandLine);

    // User Commands (Aliases)
    static void addUserCommand(const std::string& alias, const std::string& target);
    static void clearUserCommands();
    static std::string resolveUserCommand(const std::string& alias);

    void handleColonCommand(HWND hwndEdit, const std::string& cmd);
    void handleSearchCommand(HWND hwndEdit, const std::string& searchTerm, int searchFlags = 0);
    void handleSubstitutionCommand(HWND hwndEdit, const std::string& cmd);

    void previewSubstitutionFromBuffer(HWND h);
    void clearSubstitutionPreview(HWND h);
    bool parseSubstitutionCommand(const std::string& buf, HWND hwndEdit, SubstitutionParsed& parsed);

private:
    VimState& state;
    std::string lastPreviewBuffer;

    void handleCommand(HWND hwndEdit);
    void performSubstitution(HWND hwndEdit, const SubstitutionParsed& parsed);

    void initSubstitutionIndicators(HWND h);
    void showRegisters();
    void previewSubstitution(HWND h, const SubstitutionParsed& parsed);
};

