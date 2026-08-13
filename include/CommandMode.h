#pragma once
#include "NppVim.h"
#include <windows.h>
#include <string>

#define IND_SUB_MATCH    20
#define IND_SUB_REPL     21

struct ExRange {
    bool hasRange = false;
    int startLine = 0; // 0-based
    int endLine = 0;   // 0-based
    std::string rawRange;
};

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
    void showMarks(HWND hwndEdit, const std::string& args = "");
    void handleDelmarksCommand(HWND hwndEdit, const std::string& baseCmd, const std::string& args);

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

    static void executeQuit(HWND hwndEdit, bool force, bool all, bool save);
    static void executeBufferClose(HWND hwndEdit, bool force);
    static void openHelp();
    static void openTutor();
    static void executeColumn(HWND hwndEdit, const ExRange& range, const std::string& args);
    static void showBuffers();
    static void executeBufferSwitch(HWND hwndEdit, const std::string& arg);

    static bool parseRange(const std::string& input, HWND hwndEdit, const VimState& state, ExRange& range, size_t& cmdStartPos);
    static std::string expandVimPathVariables(const std::string& cmdStr);

private:
    VimState& state;
    std::string lastPreviewBuffer;

    void handleCommand(HWND hwndEdit);
    void performSubstitution(HWND hwndEdit, const SubstitutionParsed& parsed);

    void initSubstitutionIndicators(HWND h);
    void showRegisters(const std::string& filterArgs = "");
    void previewSubstitution(HWND h, const SubstitutionParsed& parsed);

    // Ex Command handlers
    void executeDelete(HWND hwndEdit, const ExRange& range, const std::string& args);
    void executeYank(HWND hwndEdit, const ExRange& range, const std::string& args);
    void executePut(HWND hwndEdit, const ExRange& range, bool before, const std::string& args);
    void executeJoin(HWND hwndEdit, const ExRange& range, bool withSpace, const std::string& args);
    void executeSort(HWND hwndEdit, const ExRange& range, bool reverse, const std::string& args);
    void executeRetab(HWND hwndEdit, const ExRange& range, bool allSpaces, const std::string& args);
    void executeExternal(HWND hwndEdit, const ExRange& range, const std::string& cmdStr);
    void executeMove(HWND hwndEdit, const ExRange& range, const std::string& args);
    void executeCopy(HWND hwndEdit, const ExRange& range, const std::string& args);
    void executePrint(HWND hwndEdit, const ExRange& range);
    static void showOutputBuffer(const std::string& title, const std::string& output, int exitCode);
};
