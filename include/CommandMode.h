#pragma once
#include "NppVim.h"
#include <windows.h>
#include <string>

#define IND_SUB_MATCH    20
#define IND_SUB_REPL     21

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

private:
    VimState& state;
    std::string lastPreviewBuffer;

    void handleCommand(HWND hwndEdit);
    void performSubstitution(HWND hwndEdit, const std::string& pattern, const std::string& replacement,
         bool useRegex, bool caseInsensitive, bool replaceAll, bool confirmEach, bool globalReplace, int startPos, int endPos);

    void initSubstitutionIndicators(HWND h);
    void clearSubstitutionPreview(HWND h);
    void previewSubstitutionFromBuffer(HWND h);
    void showRegisters();
    void previewSubstitution(HWND h, const std::string &pat, const std::string &rep, bool regex, bool global);
    bool parseSubstitution(const std::string& buf,std::string& pat,std::string& rep,bool& regex,bool& global, bool& confirm);
};
