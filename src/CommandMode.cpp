#include "../include/CommandMode.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/menuCmdID.h"

extern NormalMode* g_normalMode;
extern NppData nppData;

CommandMode::CommandMode(VimState& state) : state(state) {}

void CommandMode::enter(char prompt) {
    state.commandMode = true;
    state.commandBuffer.clear();
    state.commandBuffer.push_back(prompt);
    std::wstring display(1, prompt);
    Utils::setStatus(display.c_str());
}

void CommandMode::exit() {
    state.commandMode = false;
    state.commandBuffer.clear();
    if (g_normalMode) {
        g_normalMode->enter();
    }
}

void CommandMode::updateStatus() {
    std::wstring display(state.commandBuffer.begin(), state.commandBuffer.end());

    if (state.lastSearchMatchCount >= 0) {
        const int totalWidth = 80;
        int commandLength = display.length();

        std::wstring matchInfo;
        if (state.lastSearchMatchCount > 0) {
            matchInfo = L"Found " + std::to_wstring(state.lastSearchMatchCount) + L" match";
            if (state.lastSearchMatchCount > 1) matchInfo += L"es";
        } else {
            matchInfo = L"Pattern not found";
        }

        int rightPadding = totalWidth - display.length() - matchInfo.length();
        if (rightPadding > 0) {
            display += std::wstring(rightPadding, L' ');
        }
        display += matchInfo;
    }

    Utils::setStatus(display.c_str());
}

void CommandMode::handleKey(HWND hwndEdit, char c) {
    if (c >= 32 && c != 10 && c != 13) {
        state.commandBuffer.push_back(c);
        updateStatus();

        // Update search highlights in real-time
        if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
            std::string currentSearch = state.commandBuffer.substr(1);
            Utils::updateSearchHighlight(hwndEdit, currentSearch, false);
        } else if (state.commandBuffer[0] == ':' && state.commandBuffer.size() > 3) {
            if (state.commandBuffer[1] == 's' && state.commandBuffer[2] == ' ') {
                std::string currentPattern = state.commandBuffer.substr(3);
                Utils::updateSearchHighlight(hwndEdit, currentPattern, true);
            }
        }
    }
}

void CommandMode::handleBackspace(HWND hwndEdit) {
    if (state.commandBuffer.size() > 1) {
        state.commandBuffer.pop_back();
        updateStatus();

        // Update search highlights
        if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
            std::string currentSearch = state.commandBuffer.substr(1);
            Utils::updateSearchHighlight(hwndEdit, currentSearch, false);
        } else if (state.commandBuffer[0] == ':' && state.commandBuffer.size() > 3 &&
                   state.commandBuffer[1] == 's' && state.commandBuffer[2] == ' ') {
            std::string currentPattern = state.commandBuffer.substr(3);
            Utils::updateSearchHighlight(hwndEdit, currentPattern, true);
        } else if (state.commandBuffer.size() == 1) {
            Utils::clearSearchHighlights(hwndEdit);
            state.lastSearchMatchCount = -1;
        }
    } else {
        Utils::clearSearchHighlights(hwndEdit);
        state.lastSearchMatchCount = -1;
        exit();
    }
}

void CommandMode::handleEnter(HWND hwndEdit) {
    handleCommand(hwndEdit);
    if (g_normalMode) {
        g_normalMode->enter();
    }
}

void CommandMode::handleCommand(HWND hwndEdit) {
    if (state.commandBuffer.empty()) return;

    const std::string& buf = state.commandBuffer;

    if (buf[0] == '/' && buf.size() > 1) {
        handleSearchCommand(hwndEdit, buf.substr(1));
    } else if (buf[0] == ':' && buf.size() > 1) {
        handleColonCommand(hwndEdit, buf.substr(1));
    }

    state.commandMode = false;
    state.commandBuffer.clear();
}

void CommandMode::handleSearchCommand(HWND hwndEdit, const std::string& searchTerm) {
    performSearch(hwndEdit, searchTerm, false);
}

void CommandMode::handleColonCommand(HWND hwndEdit, const std::string& cmd) {
    if (cmd == "tutor") {
        TCHAR nppPath[MAX_PATH] = { 0 };
        ::SendMessage(nppData._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, (LPARAM)nppPath);
        std::wstring tutorPath = std::wstring(nppPath) + L"\\plugins\\NppVim\\tutor.txt";
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)tutorPath.c_str());
        return;
    }

    // Check if it's a line number
    bool isNumber = true;
    for (char ch : cmd) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            isNumber = false;
            break;
        }
    }

    if (isNumber && !cmd.empty()) {
        int lineNum = std::stoi(cmd);
        if (lineNum > 0) {
            ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
            std::wstring msg = L"Line " + std::to_wstring(lineNum);
            Utils::setStatus(msg.c_str());
        }
        return;
    }

    // Regex search
    if (cmd.size() > 2 && cmd[0] == 's' && cmd[1] == ' ') {
        std::string pattern = cmd.substr(2);
        performSearch(hwndEdit, pattern, true);
        return;
    }

    // File operations
    if (cmd == "w") {
        ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        Utils::setStatus(L"File saved");
    } else if (cmd == "q") {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
    } else if (cmd == "wq") {
        ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
    } else {
        std::wstring wcmd(cmd.begin(), cmd.end());
        std::wstring msg = L"Not an editor command: " + wcmd;
        Utils::setStatus(msg.c_str());
    }
}

void CommandMode::performSearch(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (searchTerm.empty()) return;

    state.lastSearchTerm = searchTerm;
    state.useRegex = useRegex;

    Utils::updateSearchHighlight(hwndEdit, searchTerm, useRegex);

    // Move to first match
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found != -1) {
        int s = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, s, e);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
    }
}

void CommandMode::searchNext(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1) {
        // Wrap around
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1) {
            Utils::setStatus(L"Search hit BOTTOM, continuing at TOP");
        }
    }

    if (found != -1) {
        int s = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, s, e);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    } else {
        Utils::setStatus(L"Pattern not found");
    }
}

void CommandMode::searchPrevious(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) return;

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    if (startPos > 0) startPos--;

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, 0, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1) {
        // Wrap around
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1) {
            Utils::setStatus(L"Search hit TOP, continuing at BOTTOM");
        }
    }

    if (found != -1) {
        int s = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, s, e);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    } else {
        Utils::setStatus(L"Pattern not found");
    }
}
