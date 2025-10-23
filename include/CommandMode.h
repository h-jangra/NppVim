#pragma once
#include "NppVim.h"
#include <windows.h>
#include <string>

class CommandMode {
public:
    CommandMode(VimState& state);

    void enter(char prompt);
    void exit();
    void handleKey(HWND hwndEdit, char c);
    void handleBackspace(HWND hwndEdit);
    void handleEnter(HWND hwndEdit);
    void updateStatus();

    // Search functions
    void performSearch(HWND hwndEdit, const std::string& searchTerm, bool useRegex = false);
    void searchNext(HWND hwndEdit);
    void searchPrevious(HWND hwndEdit);

private:
    VimState& state;

    void handleCommand(HWND hwndEdit);
    void handleColonCommand(HWND hwndEdit, const std::string& cmd);
    void handleSearchCommand(HWND hwndEdit, const std::string& searchTerm);
};
