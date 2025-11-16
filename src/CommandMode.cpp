#include "../include/CommandMode.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../include/Marks.h"
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
    updateStatus();
}

void CommandMode::exit() {
    state.commandMode = false;
    state.commandBuffer.clear();

    if (state.mode != VISUAL) {
        Utils::clearSearchHighlights(Utils::getCurrentScintillaHandle());
        state.lastSearchMatchCount = -1;

        if (g_normalMode) {
            g_normalMode->enter();
        }
    }
    else {
        Utils::setStatus(TEXT("-- VISUAL --"));
    }
}
void CommandMode::updateStatus() {
    if (state.commandBuffer.empty()) {
        Utils::setStatus(TEXT(""));
        return;
    }

    std::wstring display(state.commandBuffer.begin(), state.commandBuffer.end());

    if (state.lastSearchMatchCount >= 0) {
        const int totalWidth = 60;
        int commandLength = static_cast<int>(display.length());

        std::wstring matchInfo;
        if (state.lastSearchMatchCount > 0) {
            matchInfo = L"[" + std::to_wstring(state.lastSearchMatchCount) + L" matches]";
        }
        else {
            matchInfo = L"[Pattern not found]";
        }

        int padding = totalWidth - commandLength - static_cast<int>(matchInfo.length());
        if (padding > 0) {
            display += std::wstring(padding, L' ');
        }
        display += matchInfo;
    }

    Utils::setStatus(display.c_str());
}

void CommandMode::handleKey(HWND hwndEdit, char c) {
    if (!hwndEdit) return;

    if (c == 13 || c == 10) {
        handleEnter(hwndEdit);
        return;
    }

    if (c == 27) {
        exit();
        return;
    }

    if (c >= 32 && c <= 126) {
        state.commandBuffer.push_back(c);
        updateStatus();

        if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
            std::string currentSearch = state.commandBuffer.substr(1);
            Utils::updateSearchHighlight(hwndEdit, currentSearch, false);
        }
        else if (state.commandBuffer[0] == '?' && state.commandBuffer.size() > 1) {
            std::string currentPattern = state.commandBuffer.substr(1);
            Utils::updateSearchHighlight(hwndEdit, currentPattern, true);
        }
        else if (state.commandBuffer.size() == 1) {
            Utils::clearSearchHighlights(hwndEdit);
            state.lastSearchMatchCount = -1;
        }
    }
}

void CommandMode::handleBackspace(HWND hwndEdit) {
    if (!hwndEdit) return;

    if (state.commandBuffer.size() > 1) {
        state.commandBuffer.pop_back();
        updateStatus();

        if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
            std::string currentSearch = state.commandBuffer.substr(1);
            Utils::updateSearchHighlight(hwndEdit, currentSearch, false);
        }
        else if (state.commandBuffer.size() > 3 &&
            state.commandBuffer[0] == ':' &&
            state.commandBuffer[1] == 's' &&
            state.commandBuffer[2] == ' ') {
            std::string currentPattern = state.commandBuffer.substr(3);
            Utils::updateSearchHighlight(hwndEdit, currentPattern, true);
        }
        else if (state.commandBuffer.size() == 1) {
            Utils::clearSearchHighlights(hwndEdit);
            state.lastSearchMatchCount = -1;
        }
    }
    else {
        exit();
    }
}

void CommandMode::handleEnter(HWND hwndEdit) {
    if (!hwndEdit) return;
    handleCommand(hwndEdit);
}

void CommandMode::handleCommand(HWND hwndEdit) {
    if (state.commandBuffer.empty()) {
        exit();
        return;
    }

    const std::string& buf = state.commandBuffer;
    char firstChar = buf[0];

    try {
        if (firstChar == '/') {
            if (buf.size() > 1) {
                handleSearchCommand(hwndEdit, buf.substr(1));
            }
            else {
                Utils::setStatus(TEXT("No search pattern"));
            }
        }
        else if (firstChar == '?') {
            if (buf.size() > 1) {
                handleSearchCommand(hwndEdit, buf.substr(1), true); // regex search
            }
            else {
                Utils::setStatus(TEXT("No regex pattern"));
            }
        }
        else if (firstChar == ':') {
            if (buf.size() > 1) {
                handleColonCommand(hwndEdit, buf.substr(1));
            }
            else {
                exit();
                return;
            }
        }
        else {
            Utils::setStatus(TEXT("Unknown command type"));
        }
    }
    catch (const std::exception& e) {
        std::string error = "Command error: " + std::string(e.what());
        Utils::setStatus(std::wstring(error.begin(), error.end()).c_str());
    }

    state.commandMode = false;
    state.commandBuffer.clear();

    if (state.mode == VISUAL) {
        Utils::setStatus(TEXT("-- VISUAL --"));
    }
}

void CommandMode::handleSearchCommand(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    performSearch(hwndEdit, searchTerm, useRegex);
}

void CommandMode::handleColonCommand(HWND hwndEdit, const std::string& cmd) {
    if (cmd.empty()) return;

    if (cmd == "marks" || cmd == "m" || cmd.find("delm") == 0 || cmd.find("dm") == 0) {
        handleMarksCommand(hwndEdit, cmd);
        return;
    }

    bool isNumber = true;
    for (char ch : cmd) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            isNumber = false;
            break;
        }
    }

    if (isNumber) {
        int lineNum = std::stoi(cmd);
        if (lineNum > 0) {
            int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
            if (lineNum <= lineCount) {
                ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
                ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
                std::wstring msg = L"Jumped to line " + std::to_wstring(lineNum);
                Utils::setStatus(msg.c_str());
            }
            else {
                Utils::setStatus(TEXT("Line number out of range"));
            }
        }
        return;
    }

    if (cmd.size() > 2 && cmd[0] == 's' && cmd[1] == ' ') {
        std::string pattern = cmd.substr(2);
        performSearch(hwndEdit, pattern, true);
        return;
    }

    if (cmd == "w" || cmd == "write") {
        ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        Utils::setStatus(TEXT("File saved"));
    }
    else if (cmd == "q" || cmd == "quit") {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
    }
    else if (cmd == "wq" || cmd == "x") {
        ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
    }
    else if (cmd == "q!" || cmd == "quit!") {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSE);
    }
    else if (cmd == "tutor" || cmd == "tut") {
        TCHAR nppPath[MAX_PATH] = { 0 };
        ::SendMessage(nppData._nppHandle, NPPM_GETNPPDIRECTORY, MAX_PATH, (LPARAM)nppPath);
        std::wstring tutorPath = std::wstring(nppPath) + L"\\plugins\\NppVim\\tutor.txt";
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)tutorPath.c_str());
        Utils::setStatus(TEXT("Opened tutor"));
    }
    else {
        std::wstring wcmd(cmd.begin(), cmd.end());
        std::wstring msg = L"Not an editor command: " + wcmd;
        Utils::setStatus(msg.c_str());
    }
}

void CommandMode::performSearch(HWND hwndEdit, const std::string& searchTerm, bool useRegex) {
    if (searchTerm.empty()) {
        Utils::setStatus(TEXT("Empty search pattern"));
        return;
    }

    state.lastSearchTerm = searchTerm;
    state.useRegex = useRegex;
    state.lastSearchMatchCount = Utils::countSearchMatches(hwndEdit, searchTerm, useRegex);
    Utils::updateSearchHighlight(hwndEdit, searchTerm, useRegex);

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int flags = (useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    int startPos;
    if (state.mode == VISUAL && state.visualSearchAnchor != -1) {
        startPos = state.visualSearchAnchor;
    }
    else {
        startPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    }

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found == -1) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

        if (found != -1) {
            Utils::setStatus(TEXT("Search wrapped to top"));
        }
    }

    if (found != -1) {
        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

        if (state.mode == VISUAL && state.visualSearchAnchor != -1) {
            if (state.visualSearchAnchor <= start) {
                ::SendMessage(hwndEdit, SCI_SETSEL, state.visualSearchAnchor, end);
            }
            else {
                ::SendMessage(hwndEdit, SCI_SETSEL, end, state.visualSearchAnchor);
            }
        }
        else {
            ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        }

        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, searchTerm, useRegex);
    }
    else {
        Utils::setStatus(TEXT("Pattern not found"));
    }
}

void CommandMode::searchNext(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) {
        Utils::setStatus(TEXT("No previous search"));
        return;
    }

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1) {
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1) {
            Utils::setStatus(TEXT("Search wrapped to top"));
        }
    }

    if (found != -1) {
        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    }
    else {
        Utils::setStatus(TEXT("Pattern not found"));
    }
}

void CommandMode::searchPrevious(HWND hwndEdit) {
    if (state.lastSearchTerm.empty()) {
        Utils::setStatus(TEXT("No previous search"));
        return;
    }

    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    if (startPos > 0) startPos--;

    int flags = (state.useRegex ? SCFIND_REGEXP : 0);
    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    int lastFound = -1;
    int lastStart = -1, lastEnd = -1;

    while (found != -1) {
        lastFound = found;
        lastStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        lastEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lastEnd, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());
    }

    if (lastFound != -1) {
        ::SendMessage(hwndEdit, SCI_SETSEL, lastStart, lastEnd);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    }
    else {
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        lastFound = -1;
        while (found != -1) {
            lastFound = found;
            lastStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
            lastEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lastEnd, 0);
            found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());
        }

        if (lastFound != -1) {
            ::SendMessage(hwndEdit, SCI_SETSEL, lastStart, lastEnd);
            ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
            Utils::setStatus(TEXT("Search wrapped to bottom"));
            Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
        }
        else {
            Utils::setStatus(TEXT("Pattern not found"));
        }
    }
}

void CommandMode::handleMarksCommand(HWND hwndEdit, const std::string& commandLine) {
    if (commandLine == "marks" || commandLine == "m") {
        std::string marksList = Marks::listMarks(hwndEdit);

#ifdef UNICODE
        int len = MultiByteToWideChar(CP_UTF8, 0, marksList.c_str(), -1, NULL, 0);
        if (len > 0) {
            std::wstring wideList;
            wideList.resize(len);
            MultiByteToWideChar(CP_UTF8, 0, marksList.c_str(), -1, &wideList[0], len);
            ::MessageBox(nppData._nppHandle, wideList.c_str(), TEXT("Marks"), MB_OK | MB_ICONINFORMATION);
        }
#else
        ::MessageBox(nppData._nppHandle, marksList.c_str(), TEXT("Marks"), MB_OK | MB_ICONINFORMATION);
#endif

        Utils::setStatus(TEXT("-- Marks list shown --"));
        return;
    }

    if (commandLine.find("delm") == 0 || commandLine.find("dm") == 0) {
        size_t startPos = (commandLine.length() > 4 && commandLine[4] == ' ') ? 5 : 4;

        if (commandLine.find("!") != std::string::npos || commandLine.find("a") != std::string::npos) {
            Marks::clearAllMarks(hwndEdit);
            Utils::setStatus(TEXT("-- All marks deleted --"));
            return;
        }

        if (startPos < commandLine.length()) {
            std::string marks = commandLine.substr(startPos);
            int deletedCount = 0;

            for (char ch : marks) {
                if (ch != ' ' && Marks::isValidMark(ch)) {
                    Marks::deleteMark(hwndEdit, ch);
                    deletedCount++;
                }
            }

            if (deletedCount > 0) {
                Utils::setStatus(TEXT("-- Marks deleted --"));
            }
            else {
                Utils::setStatus(TEXT("-- No valid marks specified --"));
            }
        }
        else {
            Utils::setStatus(TEXT("-- Specify marks to delete --"));
        }
        return;
    }

    Utils::setStatus(TEXT("-- Unknown marks command --"));
}