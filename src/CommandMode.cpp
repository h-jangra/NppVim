#include "../include/CommandMode.h"
#include <shlwapi.h>
#include <fstream>
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../include/Keymap.h"
#include "../include/Registers.h"
#include "../include/EditorOps.h"
#include "../include/NppVim.h"
#include "../include/Marks.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/PluginInterface.h"
#include "../plugin/menuCmdID.h"
#include <sstream>
#include <algorithm>
#include <vector>
#include <regex>
#include <cctype>
#include <unordered_map>
#include <set>

extern NormalMode *g_normalMode;
extern NppData nppData;
extern HINSTANCE g_hInstance;

static std::unordered_map<std::string, std::string> g_userCommands;

void CommandMode::addUserCommand(const std::string& alias, const std::string& target) {
    g_userCommands[alias] = target;
}

void CommandMode::clearUserCommands() {
    g_userCommands.clear();
}

std::string CommandMode::resolveUserCommand(const std::string& alias) {
    auto it = g_userCommands.find(alias);
    if (it != g_userCommands.end()) {
        return it->second;
    }
    return alias;
}

static void appendNonKeymapHelp(std::string& help);

auto toggleSplit = [](HWND, int) {
    HWND npp = nppData._nppHandle;

    int before = ::SendMessage(npp, NPPM_GETCURRENTVIEW, 0, 0);

    ::SendMessage(npp, WM_COMMAND, IDM_VIEW_SWITCHTO_OTHER_VIEW, 0);

    int after = ::SendMessage(npp, NPPM_GETCURRENTVIEW, 0, 0);

    if (before == after) {
        ::SendMessage(npp, WM_COMMAND, IDM_VIEW_CLONE_TO_ANOTHER_VIEW, 0);
    }
};

void CommandMode::enter(char prompt) {
  state.commandMode = true;
  state.commandBuffer.clear();
  state.commandBuffer.push_back(prompt);

  if (g_config.enableKeyboardLayoutSwitching) {
    HWND focusWnd = ::GetFocus();
    HKL targetLayout = Utils::resolveLayout(g_config.normallayout);
    if (!targetLayout) targetLayout = ::LoadKeyboardLayout(L"00000409", KLF_ACTIVATE);

    DWORD threadId = GetWindowThreadProcessId(focusWnd,nullptr);
    HKL currentLayout = GetKeyboardLayout(threadId);
    
    if (currentLayout != targetLayout) {
      state.savedInsertLayout = currentLayout;
      ::ActivateKeyboardLayout(targetLayout, 0);
      ::PostMessage(focusWnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)targetLayout);
    }
  }

  HWND h = Utils::getCurrentScintillaHandle();
  if (h) {
      initSubstitutionIndicators(h);
  }

  updateStatus();
}

void CommandMode::exit() {
  state.commandMode = false;
  
  HWND h = Utils::getCurrentScintillaHandle();
  if (h) {
      clearSubstitutionPreview(h);
  }

  state.commandBuffer.clear();
  lastPreviewBuffer.clear();

  if (state.mode != VISUAL) {
    Utils::clearSearchHighlights(Utils::getCurrentScintillaHandle());
    state.lastSearchMatchCount = -1;

    if (g_normalMode) {
      g_normalMode->enter();
    }
  } else {
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
    std::wstring matchInfo;
    if (state.lastSearchMatchCount > 0) {
      matchInfo = L"  [" + std::to_wstring(state.lastSearchMatchCount) + L" matches]";
    } else {
      matchInfo = L"  [Pattern not found]";
    }
    display += matchInfo;
  }

  Utils::setStatus(display.c_str());
}

void CommandMode::handleKey(HWND hwndEdit, wchar_t wChar) {
  if (!hwndEdit) return;

  if (wChar == 13 || wChar == 10) {
    handleEnter(hwndEdit);
    return;
  }

  if (wChar == 27) {
    exit();
    return;
  }

  if (wChar == 8) {
    handleBackspace(hwndEdit);
    return;
  }

  if (wChar == 23) { // Ctrl-W: delete word left
    if (state.commandBuffer.size() > 1) {
      while (state.commandBuffer.size() > 1 && state.commandBuffer.back() == ' ') {
        state.commandBuffer.pop_back();
      }
      while (state.commandBuffer.size() > 1 && state.commandBuffer.back() != ' ') {
        state.commandBuffer.pop_back();
      }
      updateStatus();
      previewSubstitutionFromBuffer(hwndEdit);
    }
    return;
  }

  if (wChar == 21) { // Ctrl-U: clear line
    if (state.commandBuffer.size() > 1) {
      state.commandBuffer = state.commandBuffer.substr(0, 1);
      updateStatus();
      previewSubstitutionFromBuffer(hwndEdit);
    }
    return;
  }

  if (wChar >= 32) {
    std::string utf8 = Utils::toUtf8(wChar);
    state.commandBuffer += utf8;
    updateStatus();

    if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
      std::string currentSearch = state.commandBuffer.substr(1);
      Utils::updateSearchHighlight(hwndEdit, currentSearch, false);
    } else if (state.commandBuffer[0] == '?' && state.commandBuffer.size() > 1) {
      std::string currentPattern = state.commandBuffer.substr(1);
      Utils::updateSearchHighlight(hwndEdit, currentPattern, true);
    } else if (state.commandBuffer.size() == 1) {
      Utils::clearSearchHighlights(hwndEdit);
      state.lastSearchMatchCount = -1;
    }

    previewSubstitutionFromBuffer(hwndEdit);
  }
}

void CommandMode::handleBackspace(HWND hwndEdit) {
  if (!hwndEdit) return;

  if (state.commandBuffer.size() > 1) {
    // Correctly handle UTF-8 backspace by removing the last multi-byte character
    if (!state.commandBuffer.empty()) {
        size_t last = state.commandBuffer.size() - 1;
        while (last > 0 && (state.commandBuffer[last] & 0xC0) == 0x80) {
            last--;
        }
        state.commandBuffer.erase(last);
    }
    
    updateStatus();

    if (state.commandBuffer[0] == '/' && state.commandBuffer.size() > 1) {
      std::string currentSearch = state.commandBuffer.substr(1);
      Utils::updateSearchHighlight(hwndEdit, currentSearch, false);
    } else if (state.commandBuffer.size() == 1) {
      Utils::clearSearchHighlights(hwndEdit);
      state.lastSearchMatchCount = -1;
    }

    previewSubstitutionFromBuffer(hwndEdit);
  }
  else {
    this->exit();
  }
}

void CommandMode::handleEnter(HWND hwndEdit) {
  if (!hwndEdit) return;
  handleCommand(hwndEdit);
}

void CommandMode::handleCommand(HWND hwndEdit) {
  if (state.commandBuffer.empty()) {
    this->exit();
    return;
  }

  const std::string &buf = state.commandBuffer;
  char firstChar = buf[0];

  try {
    if (firstChar == '/') {
      if (buf.size() > 1) {
        handleSearchCommand(hwndEdit, buf.substr(1), 0);
      } else {
        Utils::setStatus(TEXT("No search pattern"));
      }
    } else if (firstChar == '?') {
      if (buf.size() > 1) {
        handleSearchCommand(hwndEdit, buf.substr(1), SCFIND_REGEXP);
      } else {
        Utils::setStatus(TEXT("No regex pattern"));
      }
    } else if (firstChar == ':') {
      if (buf.size() > 1) {
        handleColonCommand(hwndEdit, buf.substr(1));
      } else {
        this->exit();
      }
    } else {
      Utils::setStatus(TEXT("Unknown command type"));
    }
  } catch (const std::exception &e) {
    std::string error = "Command error: " + std::string(e.what());
    Utils::setStatus(std::wstring(error.begin(), error.end()).c_str());
  }

  state.commandMode = false;
  state.commandBuffer.clear();

  if (state.mode == VISUAL) {
    Utils::setStatus(TEXT("-- VISUAL --"));
  }
}

void CommandMode::handleSearchCommand(HWND hwndEdit, const std::string &searchTerm, int searchFlags) {
  performSearch(hwndEdit, searchTerm, searchFlags);
}

#include "../include/ConfigManager.h"
#include "../include/OptionRegistry.h"
#include "../include/RcParser.h"

void CommandMode::initCommandRegistry() {
    auto registerCmd = [this](const std::vector<std::string>& names, std::function<void(HWND, const ParsedExCommand&)> handler) {
        for (const auto& name : names) {
            commandRegistry[name] = handler;
        }
    };

    // 0. Quit & Close commands
    registerCmd({"q", "quit", "close", "clo"}, [this](HWND h, const ParsedExCommand& cmd) {
        executeQuit(h, cmd.force, false, false);
    });
    registerCmd({"qa", "quitall", "qall"}, [this](HWND h, const ParsedExCommand& cmd) {
        executeQuit(h, cmd.force, true, false);
    });
    registerCmd({"wq", "x", "exit"}, [this](HWND h, const ParsedExCommand& cmd) {
        executeQuit(h, cmd.force, false, true);
    });
    registerCmd({"wqa", "xa", "xall", "wqall"}, [this](HWND h, const ParsedExCommand& cmd) {
        executeQuit(h, cmd.force, true, true);
    });

    // Write & Update
    registerCmd({"w", "write"}, [](HWND, const ParsedExCommand& cmd) {
        if (!cmd.args.empty()) {
            std::wstring wPath = Utils::toWide(cmd.args);
            wchar_t currentFile[MAX_PATH] = {0};
            ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)currentFile);
            wchar_t currentDir[MAX_PATH] = {0};
            wcscpy_s(currentDir, currentFile);
            PathRemoveFileSpecW(currentDir);
            wchar_t fullPath[MAX_PATH] = {0};
            if (PathIsRelativeW(wPath.c_str())) {
                PathCombineW(fullPath, currentDir, wPath.c_str());
            } else {
                wcscpy_s(fullPath, wPath.c_str());
            }
            ::SendMessageW(nppData._nppHandle, NPPM_SAVECURRENTFILEAS, FALSE, (LPARAM)fullPath);
        } else {
            ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        }
        Utils::setStatus(TEXT("File saved"));
    });
    registerCmd({"wa", "wall"}, [](HWND, const ParsedExCommand&) {
        ::SendMessage(nppData._nppHandle, NPPM_SAVEALLFILES, 0, 0);
        Utils::setStatus(TEXT("All files saved"));
    });
    registerCmd({"up", "update"}, [](HWND h, const ParsedExCommand&) {
        if ((int)::SendMessage(h, SCI_GETMODIFY, 0, 0)) {
            ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
            Utils::setStatus(TEXT("File saved"));
        } else {
            Utils::setStatus(TEXT("No changes made"));
        }
    });

    // Buffers & Tabs
    registerCmd({"bd", "bdelete", "bw", "bwipeout", "bunload"}, [](HWND h, const ParsedExCommand& cmd) {
        executeBufferClose(h, cmd.force);
    });
    registerCmd({"bn", "bnext"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_NEXT, 0, 0); });
    registerCmd({"bp", "bprev", "bprevious"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_PREV, 0, 0); });
    registerCmd({"bf", "bfirst"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_START); });
    registerCmd({"bl", "blast"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_END); });
    registerCmd({"b", "buffer"}, [](HWND h, const ParsedExCommand& cmd) { executeBufferSwitch(h, cmd.args); });
    registerCmd({"buffers", "ls", "files"}, [](HWND, const ParsedExCommand&) { showBuffers(); });

    registerCmd({"tabnew", "tabe", "tabedit"}, [this](HWND h, const ParsedExCommand& cmd) {
        if (cmd.args.empty()) {
            ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
        } else {
            handleColonCommand(h, "edit " + cmd.args);
        }
    });
    registerCmd({"tabclose", "tabc"}, [](HWND h, const ParsedExCommand& cmd) { executeQuit(h, cmd.force, false, false); });
    registerCmd({"tabnext", "tabn"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_NEXT, 0, 0); });
    registerCmd({"tabprev", "tabp", "tabprevious"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_PREV, 0, 0); });
    registerCmd({"tabfirst", "tabfir"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_START); });
    registerCmd({"tablast"}, [](HWND, const ParsedExCommand&) { ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_END); });
    registerCmd({"tabonly", "tabo", "only", "on"}, [](HWND, const ParsedExCommand&) {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_CLOSEALL_BUT_CURRENT);
        Utils::setStatus(TEXT("Other tabs closed"));
    });

    // Info & Help & Marks
    registerCmd({"h", "help"}, [](HWND, const ParsedExCommand&) { openHelp(); });
    registerCmd({"tutor", "tut"}, [](HWND, const ParsedExCommand&) { openTutor(); });
    registerCmd({"reg", "registers", "display", "di"}, [this](HWND, const ParsedExCommand& cmd) { showRegisters(cmd.args); });
    registerCmd({"marks"}, [this](HWND h, const ParsedExCommand& cmd) { showMarks(h, cmd.args); });
    registerCmd({"delmarks", "delm", "dm"}, [this](HWND h, const ParsedExCommand& cmd) {
        handleDelmarksCommand(h, cmd.name + (cmd.force ? "!" : ""), cmd.args);
    });
    registerCmd({"mark", "ma", "k"}, [this](HWND h, const ParsedExCommand& cmd) {
        if (cmd.args.empty()) {
            Utils::setStatus(TEXT("E471: Argument required"));
        } else {
            char m = cmd.args[0];
            if (Marks::isValidSetMark(m)) {
                int targetLine = cmd.range.hasRange ? cmd.range.endLine : Utils::caretLine(h);
                Marks::setMarkAtLine(h, m, targetLine);
                std::wstring msg = L"Mark '" + std::wstring(1, (wchar_t)m) + L"' set at line " + std::to_wstring(targetLine + 1);
                Utils::setStatus(msg.c_str());
            } else {
                Utils::setStatus(TEXT("E191: Argument must be a letter"));
            }
        }
    });

    // Editing operations
    registerCmd({"d", "delete"}, [this](HWND h, const ParsedExCommand& cmd) { executeDelete(h, cmd.range, cmd.args); });
    registerCmd({"y", "yank"}, [this](HWND h, const ParsedExCommand& cmd) { executeYank(h, cmd.range, cmd.args); });
    registerCmd({"pu", "put"}, [this](HWND h, const ParsedExCommand& cmd) { executePut(h, cmd.range, cmd.force, cmd.args); });
    registerCmd({"p"}, [this](HWND h, const ParsedExCommand& cmd) {
        if (!cmd.args.empty() && Registers::isValidRegister(cmd.args[0])) {
            executePut(h, cmd.range, cmd.force, cmd.args);
        } else if (cmd.range.hasRange) {
            executePrint(h, cmd.range);
        } else {
            executePut(h, cmd.range, cmd.force, cmd.args);
        }
    });
    registerCmd({"print"}, [this](HWND h, const ParsedExCommand& cmd) { executePrint(h, cmd.range); });
    registerCmd({"j", "join"}, [this](HWND h, const ParsedExCommand& cmd) { executeJoin(h, cmd.range, !cmd.force, cmd.args); });
    registerCmd({"sort"}, [this](HWND h, const ParsedExCommand& cmd) { executeSort(h, cmd.range, cmd.force, cmd.args); });
    registerCmd({"retab"}, [this](HWND h, const ParsedExCommand& cmd) { executeRetab(h, cmd.range, cmd.force, cmd.args); });
    registerCmd({"m", "move"}, [this](HWND h, const ParsedExCommand& cmd) { executeMove(h, cmd.range, cmd.args); });
    registerCmd({"co", "copy", "t"}, [this](HWND h, const ParsedExCommand& cmd) { executeCopy(h, cmd.range, cmd.args); });
    registerCmd({"column", "col"}, [](HWND h, const ParsedExCommand& cmd) { executeColumn(h, cmd.range, cmd.args); });

    // Config & Options & Mappings
    registerCmd({"set"}, [](HWND, const ParsedExCommand& cmd) {
        if (cmd.args.empty()) {
            Utils::setStatus(TEXT("Options listed in Help (use :h for now)"));
        } else {
            if (!OptionRegistry::getInstance().setOptionFromString(cmd.args)) {
                Utils::setStatus(TEXT("E518: Unknown option"));
            }
        }
    });
    registerCmd({"command", "com"}, [](HWND h, const ParsedExCommand& cmd) {
        RcParser::getInstance().executeLine(cmd.fullRaw, h);
    });
    registerCmd({"map", "nmap", "imap", "vmap", "cmap", "noremap", "nnoremap", "inoremap", "vnoremap", "cnoremap",
                 "unmap", "nunmap", "iunmap", "vunmap", "cunmap"}, [this](HWND h, const ParsedExCommand& cmd) {
        if (cmd.args.empty() && cmd.name.find("un") == std::string::npos) {
            MappingMode m = MappingMode::All;
            if (cmd.name[0] == 'n') m = MappingMode::Normal;
            else if (cmd.name[0] == 'i') m = MappingMode::Insert;
            else if (cmd.name[0] == 'v') m = MappingMode::Visual;
            else if (cmd.name[0] == 'c') m = MappingMode::Command;

            auto list = Keymap::getAllUserMappings(m);
            std::string out = "--- Mappings ---\n";
            for (auto& mapping : list) {
                out += (mapping.recursive ? "map " : "noremap ") + mapping.from + " -> " + mapping.to + "\n";
            }
            if (list.empty()) out += "No mappings defined.\n";
            showOutputBuffer("Mappings", out, 0);
        } else {
            RcParser::getInstance().executeLine(cmd.fullRaw, h);
        }
    });
    registerCmd({"source", "so"}, [](HWND h, const ParsedExCommand& cmd) {
        std::string path;
        std::stringstream pss(cmd.args);
        pss >> path;
        if (!RcParser::getInstance().parseFile(path, h)) {
            Utils::setStatus(TEXT("E484: Cannot open file"));
        } else {
            Utils::setStatus(TEXT("Configuration sourced"));
        }
    });
    registerCmd({"NppVimReload", "reload"}, [](HWND, const ParsedExCommand&) {
        loadConfig();
        Utils::setStatus(TEXT("NppVim reloaded"));
    });
    registerCmd({"undo", "u"}, [](HWND h, const ParsedExCommand&) {
        ::SendMessage(h, SCI_UNDO, 0, 0);
        Utils::setStatus(TEXT("1 change undone"));
    });
    registerCmd({"redo", "red"}, [](HWND h, const ParsedExCommand&) {
        ::SendMessage(h, SCI_REDO, 0, 0);
        Utils::setStatus(TEXT("1 change redone"));
    });
    registerCmd({"split", "sp", "vsplit", "vs"}, [](HWND h, const ParsedExCommand&) {
        toggleSplit(h, 0);
    });
    registerCmd({"noh", "nohl", "nohls", "nohlsearch"}, [this](HWND h, const ParsedExCommand&) {
        Utils::clearSearchHighlights(h);
        state.lastSearchMatchCount = -1;
        Utils::setStatus(TEXT("Search highlight cleared"));
    });
    registerCmd({"edit", "e"}, [](HWND, const ParsedExCommand& cmd) {
        std::string path = cmd.args;
        if (path.empty()) {
            ::SendMessage(nppData._nppHandle, IDM_FILE_RELOAD, 0, 0);
            Utils::setStatus(TEXT("File reloaded"));
        } else if (path == "rc" || path == "nppvim.rc" || path == ".nppvimrc") {
            ConfigManager::getInstance().editRc();
        } else if (path == "ini" || path == "config.ini") {
            ConfigManager::getInstance().editIni();
        } else {
            std::wstring wPath = Utils::toWide(path);
            wchar_t currentFile[MAX_PATH] = {0};
            ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)currentFile);
            wchar_t currentDir[MAX_PATH] = {0};
            wcscpy_s(currentDir, currentFile);
            PathRemoveFileSpecW(currentDir);
            wchar_t fullPath[MAX_PATH] = {0};
            if (PathIsRelativeW(wPath.c_str())) {
                PathCombineW(fullPath, currentDir, wPath.c_str());
            } else {
                wcscpy_s(fullPath, wPath.c_str());
            }
            if (!PathFileExistsW(fullPath)) {
                HANDLE hFile = CreateFileW(fullPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    CloseHandle(hFile);
                }
            }
            ::SendMessageW(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)fullPath);
        }
    });
    registerCmd({"editrc", "erc", "rc"}, [](HWND, const ParsedExCommand&) { ConfigManager::getInstance().editRc(); });
    registerCmd({"editini", "eini", "ini"}, [](HWND, const ParsedExCommand&) { ConfigManager::getInstance().editIni(); });
    registerCmd({"pwd"}, [](HWND, const ParsedExCommand&) {
        wchar_t cur[MAX_PATH] = {0};
        ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)cur);
        PathRemoveFileSpecW(cur);
        Utils::setStatus(cur[0] ? cur : TEXT("No active file directory"));
    });
    registerCmd({"cd", "chdir"}, [](HWND, const ParsedExCommand& cmd) {
        if (cmd.args.empty()) {
            wchar_t cur[MAX_PATH] = {0};
            ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)cur);
            PathRemoveFileSpecW(cur);
            if (cur[0]) {
                SetCurrentDirectoryW(cur);
                Utils::setStatus((std::wstring(L"Directory: ") + cur).c_str());
            }
        } else {
            std::wstring wPath = Utils::toWide(cmd.args);
            if (SetCurrentDirectoryW(wPath.c_str())) {
                wchar_t buf[MAX_PATH] = {0};
                GetCurrentDirectoryW(MAX_PATH, buf);
                Utils::setStatus((std::wstring(L"Directory: ") + buf).c_str());
            } else {
                Utils::setStatus(TEXT("E344: Can't find directory"));
            }
        }
    });
    registerCmd({"echo", "echom"}, [](HWND, const ParsedExCommand& cmd) {
        std::string echoStr = cmd.args;
        if (echoStr.size() >= 2 && ((echoStr.front() == '"' && echoStr.back() == '"') || (echoStr.front() == '\'' && echoStr.back() == '\''))) {
            echoStr = echoStr.substr(1, echoStr.size() - 2);
        }
        Utils::setStatus(Utils::toWide(echoStr).c_str());
    });
    registerCmd({"about"}, [](HWND, const ParsedExCommand&) { about(); });
    registerCmd({"config"}, [](HWND, const ParsedExCommand&) { showConfigDialog(); });
    registerCmd({"version", "ver"}, [](HWND, const ParsedExCommand&) {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW((HMODULE)g_hInstance, path, MAX_PATH);
        DWORD handle = 0;
        DWORD size = GetFileVersionInfoSizeW(path, &handle);
        if (size) {
            std::vector<BYTE> data(size);
            if (GetFileVersionInfoW(path, 0, size, data.data())) {
                VS_FIXEDFILEINFO* info = nullptr;
                UINT len = 0;
                if (VerQueryValueW(data.data(), L"\\", (LPVOID*)&info, &len)) {
                    WCHAR version[64];
                    wsprintfW(version, L"NppVim Version: %d.%d.%d.%d",
                        HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                        HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
                    Utils::setStatus(version);
                    return;
                }
            }
        }
        Utils::setStatus(TEXT("NppVim"));
    });
    registerCmd({"paypal", "donate"}, [](HWND, const ParsedExCommand&) {
        ShellExecuteW(NULL, L"open", L"https://paypal.me/h8imansh8u", NULL, NULL, SW_SHOWNORMAL);
    });
    registerCmd({"gh", "github"}, [](HWND, const ParsedExCommand&) {
        ShellExecuteW(NULL, L"open", L"https://github.com/h-jangra/nppvim", NULL, NULL, SW_SHOWNORMAL);
    });
}

bool CommandMode::parseExCommand(const std::string& input, HWND hwndEdit, ParsedExCommand& outCmd) {
    outCmd = ParsedExCommand();
    outCmd.fullRaw = Utils::trim(input);
    if (outCmd.fullRaw.empty() || !hwndEdit) return false;

    // Check substitution command first (supports :s/old/new/, :%s/old/new/g, :'<,'>s/old/new/g, :<>s/old/new/g, etc.)
    SubstitutionParsed parsedSub;
    if (parseSubstitutionCommand(outCmd.fullRaw, hwndEdit, parsedSub)) {
        outCmd.name = "s";
        return true;
    }

    size_t cmdStart = 0;
    parseRange(outCmd.fullRaw, hwndEdit, state, outCmd.range, cmdStart);

    std::string remaining = Utils::trim(outCmd.fullRaw.substr(cmdStart));
    if (remaining.empty()) {
        outCmd.name = "";
        return true;
    }

    if (remaining[0] == '!') {
        outCmd.name = "!";
        outCmd.args = Utils::trim(remaining.substr(1));
        return true;
    }

    if (remaining[0] == '#' || remaining[0] == '>' || remaining[0] == '<') {
        outCmd.name = std::string(1, remaining[0]);
        outCmd.args = Utils::trim(remaining.substr(1));
        return true;
    }

    size_t nameEnd = 0;
    while (nameEnd < remaining.size() && (std::isalpha(static_cast<unsigned char>(remaining[nameEnd])) || remaining[nameEnd] == '_')) {
        nameEnd++;
    }

    std::string base = remaining.substr(0, nameEnd);
    if (nameEnd < remaining.size() && remaining[nameEnd] == '!') {
        outCmd.force = true;
        nameEnd++;
    }

    outCmd.name = base;
    outCmd.args = Utils::trim(remaining.substr(nameEnd));
    return true;
}

void CommandMode::handleColonCommand(HWND hwndEdit, const std::string &cmd) {
    if (cmd.empty() || !hwndEdit) return;

    std::string fullCmd = Utils::trim(cmd);
    if (fullCmd.empty()) return;

    SubstitutionParsed parsedSub;
    if (parseSubstitutionCommand(fullCmd, hwndEdit, parsedSub)) {
        handleSubstitutionCommand(hwndEdit, fullCmd);
        return;
    }

    ParsedExCommand exCmd;
    if (!parseExCommand(fullCmd, hwndEdit, exCmd)) return;

    if (exCmd.name.empty()) {
        if (exCmd.range.hasRange) {
            int targetLine = exCmd.range.endLine;
            ::SendMessage(hwndEdit, SCI_GOTOLINE, targetLine, 0);
            ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
            std::wstring msg = L"Jumped to line " + std::to_wstring(targetLine + 1);
            Utils::setStatus(msg.c_str());
        }
        return;
    }

    if (exCmd.name == "!") {
        executeExternal(hwndEdit, exCmd.range, exCmd.args);
        return;
    }

    std::string resolved = resolveUserCommand(exCmd.name);
    if (resolved != exCmd.name) {
        handleColonCommand(hwndEdit, resolved + (exCmd.args.empty() ? "" : (" " + exCmd.args)));
        return;
    }

    if (exCmd.name == ">") {
        int sL = exCmd.range.hasRange ? exCmd.range.startLine : Utils::caretLine(hwndEdit);
        int eL = exCmd.range.hasRange ? exCmd.range.endLine : Utils::caretLine(hwndEdit);
        EditorOps::indent(hwndEdit, sL, eL);
        return;
    }
    if (exCmd.name == "<") {
        int sL = exCmd.range.hasRange ? exCmd.range.startLine : Utils::caretLine(hwndEdit);
        int eL = exCmd.range.hasRange ? exCmd.range.endLine : Utils::caretLine(hwndEdit);
        EditorOps::unindent(hwndEdit, sL, eL);
        return;
    }
    if (exCmd.name == "#") {
        executePrint(hwndEdit, exCmd.range);
        return;
    }

    auto it = commandRegistry.find(exCmd.name);
    if (it != commandRegistry.end()) {
        it->second(hwndEdit, exCmd);
        return;
    }

    // Keymap fallback
    bool matched = false;
    if (g_commandKeymap) {
        g_commandKeymap->reset();
        for (char c : fullCmd) {
            if (!g_commandKeymap->handleKey(hwndEdit, c)) {
                break;
            }
        }
        matched = !g_commandKeymap->hasPending();
        g_commandKeymap->reset();
    }

    if (!matched && !exCmd.name.empty()) {
        std::wstring wCmd(exCmd.name.begin(), exCmd.name.end());
        Utils::setStatus((L"E492: Not an editor command: " + wCmd).c_str());
    }
}

void CommandMode::openTutor() {
    int openCount = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);
    HWND curH = Utils::getCurrentScintillaHandle();
    bool isSingleEmptyDoc = false;
    if (openCount <= 1 && curH) {
        int len = (int)::SendMessage(curH, SCI_GETTEXTLENGTH, 0, 0);
        int mod = (int)::SendMessage(curH, SCI_GETMODIFY, 0, 0);
        wchar_t path[MAX_PATH] = {0};
        ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)path);
        if (len == 0 && mod == 0 && (path[0] == L'\0' || !PathFileExistsW(path))) {
            isSingleEmptyDoc = true;
        }
    }

    if (!isSingleEmptyDoc) {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    }

    HWND h = Utils::getCurrentScintillaHandle();
    if (!h) return;

    std::string tutor = Utils::buildTutorText();

    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);
    ::SendMessage(h, SCI_SETTEXT, 0, (LPARAM)tutor.c_str());
    ::SendMessage(h, SCI_SETSAVEPOINT, 0, 0);
    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);
    ::SendMessage(h, SCI_GOTOPOS, 0, 0);

    Utils::setStatus(TEXT("-- TUTOR -- (Type :q to close)"));
}

void CommandMode::openHelp() {
    int openCount = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);
    HWND curH = Utils::getCurrentScintillaHandle();
    bool isSingleEmptyDoc = false;
    if (openCount <= 1 && curH) {
        int len = (int)::SendMessage(curH, SCI_GETTEXTLENGTH, 0, 0);
        int mod = (int)::SendMessage(curH, SCI_GETMODIFY, 0, 0);
        wchar_t path[MAX_PATH] = {0};
        ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)path);
        if (len == 0 && mod == 0 && (path[0] == L'\0' || !PathFileExistsW(path))) {
            isSingleEmptyDoc = true;
        }
    }

    if (!isSingleEmptyDoc) {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    }

    HWND h = Utils::getCurrentScintillaHandle();
    if (!h) return;

    std::string help = Utils::readPluginFile("docs\\help.txt");
    if (help.empty()) {
        help = Utils::readPluginFile("docs\\help.md");
    }
    if (help.empty()) {
        help = "NppVim — Help\n"
               "=========================\n\n";
    }

    auto appendKeymap = [&](const char* title, const Keymap* km, bool isCommandMode = false) {
        if (!km) return;
        help += "\n";
        help += title;
        help += "\n";
        help += std::string(strlen(title), '-') + "\n";

        size_t pad = 0;
        for (const auto& b : km->getBindings())
            pad = (std::max)(pad, b.keys.size());

        for (const auto& b : km->getBindings()) {
            help += "  ";
            if (isCommandMode) help += ":";
            help += b.keys;
            help += std::string(pad - b.keys.size() + (isCommandMode ? 1 : 2), ' ');
            help += "- ";
            help += b.desc;
            help += "\n";
        }
    };

    appendKeymap("Normal Mode Mappings", g_normalKeymap.get());
    appendKeymap("Visual Mode Mappings", g_visualKeymap.get());
    appendKeymap("Command Mode Commands", g_commandKeymap.get(), true);

    appendNonKeymapHelp(help);

    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);
    ::SendMessage(h, SCI_SETTEXT, 0, (LPARAM)help.c_str());
    ::SendMessage(h, SCI_SETSAVEPOINT, 0, 0);
    ::SendMessage(h, SCI_SETREADONLY, TRUE, 0);
    ::SendMessage(h, SCI_GOTOPOS, 0, 0);

    Utils::setStatus(TEXT("-- HELP -- (Type :q to close)"));
}

void CommandMode::executeQuit(HWND hwndEdit, bool force, bool all, bool save) {
    if (!hwndEdit) hwndEdit = Utils::getCurrentScintillaHandle();

    // 1. If save requested (:wq, :x, :wqa, :xa, ZZ)
    if (save) {
        if (all) {
            ::SendMessage(nppData._nppHandle, NPPM_SAVEALLFILES, 0, 0);
        } else {
            ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        }
    }

    int isReadOnly = hwndEdit ? (int)::SendMessage(hwndEdit, SCI_GETREADONLY, 0, 0) : 0;
    int isModified = hwndEdit ? (int)::SendMessage(hwndEdit, SCI_GETMODIFY, 0, 0) : 0;

    // 2. If not saving and not forcing, check if current buffer is modified
    if (!save && !force && isModified != 0 && isReadOnly == 0) {
        Utils::setStatus(TEXT("E37: No write since last change (add ! to override)"));
        return;
    }

    // 3. Clear read-only and set savepoint so Scintilla / Notepad++ allows closing without dirty prompt
    if (hwndEdit && (isReadOnly != 0 || force || isModified == 0)) {
        ::SendMessage(hwndEdit, SCI_SETREADONLY, FALSE, 0);
        ::SendMessage(hwndEdit, SCI_SETSAVEPOINT, 0, 0);
    }

    int openCount = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);

    if (all || openCount <= 1) {
        // When closing all or when closing the only open tab/buffer, exit Notepad++
        ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_FILE_EXIT, 0);
    } else {
        // Multiple tabs open: close current tab
        ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_FILE_CLOSE, 0);
    }
}

void CommandMode::executeBufferClose(HWND hwndEdit, bool force) {
    if (!hwndEdit) hwndEdit = Utils::getCurrentScintillaHandle();

    int isReadOnly = hwndEdit ? (int)::SendMessage(hwndEdit, SCI_GETREADONLY, 0, 0) : 0;
    int isModified = hwndEdit ? (int)::SendMessage(hwndEdit, SCI_GETMODIFY, 0, 0) : 0;

    if (!force && isModified != 0 && isReadOnly == 0) {
        Utils::setStatus(TEXT("E89: No write since last change for buffer (add ! to override)"));
        return;
    }

    if (hwndEdit && (isReadOnly != 0 || force || isModified == 0)) {
        ::SendMessage(hwndEdit, SCI_SETREADONLY, FALSE, 0);
        ::SendMessage(hwndEdit, SCI_SETSAVEPOINT, 0, 0);
    }

    int openCount = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);
    if (openCount <= 1) {
        if (hwndEdit) {
            ::SendMessage(hwndEdit, SCI_SETREADONLY, FALSE, 0);
            ::SendMessage(hwndEdit, SCI_CLEARALL, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETSAVEPOINT, 0, 0);
        }
        Utils::setStatus(TEXT("Buffer closed"));
    } else {
        ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_FILE_CLOSE, 0);
    }
}

CommandMode::CommandMode(VimState &state) : state(state)
{
  initCommandRegistry();
  g_commandKeymap = std::make_unique<Keymap>(state);
  g_commandKeymap->setAllowCount(false);

  g_commandKeymap
    ->set("w", "Save current file", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        Utils::setStatus(TEXT("File saved"));
    })
    .set("e", "Reload current file", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_FILE_RELOAD, 0, 0);
    })
    .set("q", "Close current file", [](HWND h, int) {
        executeQuit(h, false, false, false);
    })
    .set("qa", "Close all files", [](HWND h, int) {
        executeQuit(h, false, true, false);
    })
    .set("wq", "Save and close file", [](HWND h, int) {
        executeQuit(h, false, false, true);
    })
    .set("wqa", "Save all and close all files", [](HWND h, int) {
        executeQuit(h, false, true, true);
    })
    .set("bn", "Next tab", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_NEXT, 0, 0);
    })
    .set("bp", "Previous tab", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_PREV, 0, 0);
    })
    .set("bd", "Close current tab", [](HWND h, int) {
        executeBufferClose(h, false);
    })
    .set("vsplit", "Toggle split", toggleSplit)
    .set("vs", "Toggle split", toggleSplit)
    .set("split", "Toggle split", toggleSplit)
    .set("sp", "Toggle split", toggleSplit)
    .set("gh", "Open GitHub", [](HWND, int) {
        ShellExecuteW(NULL, L"open", L"https://github.com/h-jangra/nppvim", NULL, NULL, SW_SHOWNORMAL);
    })
    .set("paypal", "Donate via PayPal", [](HWND, int) {
        ShellExecuteW(NULL, L"open", L"https://paypal.me/h8imansh8u", NULL, NULL, SW_SHOWNORMAL);
    })
    .set("donate", "Donate via PayPal", [](HWND, int) {
        ShellExecuteW(NULL, L"open", L"https://paypal.me/h8imansh8u", NULL, NULL, SW_SHOWNORMAL);
    })
    .set("about", "About NppVim", [](HWND, int) {
        about();
    })
    .set("config", "NppVim Configuration", [](HWND, int) {
        showConfigDialog();
    })
    .set("noh", "Clear search highlight", [](HWND hwnd, int) {
      Utils::clearSearchHighlights(hwnd);
      Utils::setStatus(TEXT("Search highlight cleared"));
    })
    .set("nohl", "Clear search highlight", [](HWND hwnd, int) {
        Utils::clearSearchHighlights(hwnd);
        Utils::setStatus(TEXT("Search highlight cleared"));
    })
    .set("nohlsearch", "Clear search highlight", [](HWND hwnd, int) {
        Utils::clearSearchHighlights(hwnd);
        Utils::setStatus(TEXT("Search highlight cleared"));
    })
    .set("set nu", "Enable line numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("number", true);
        Utils::setStatus(TEXT("Line numbers enabled"));
    })
    .set("set number", "Enable line numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("number", true);
        Utils::setStatus(TEXT("Line numbers enabled"));
    })
    .set("set nonu", "Disable line numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("number", false);
        Utils::setStatus(TEXT("Line numbers disabled"));
    })
    .set("set nonumber", "Disable line numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("number", false);
        Utils::setStatus(TEXT("Line numbers disabled"));
    })
    .set("set rnu", "Enable relative numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("relativenumber", true);
        Utils::setStatus(TEXT("Relative numbers enabled"));
    })
    .set("set relativenumber", "Enable relative numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("relativenumber", true);
        Utils::setStatus(TEXT("Relative numbers enabled"));
    })
    .set("set nornu", "Disable relative numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("relativenumber", false);
        Utils::setStatus(TEXT("Relative numbers disabled"));
    })
    .set("set norelativenumber", "Disable relative numbers", [](HWND hwnd, int) {
        OptionRegistry::getInstance().setOption("relativenumber", false);
        Utils::setStatus(TEXT("Relative numbers disabled"));
    })
    .set("reg", "Show registers", [this](HWND, int) {
        showRegisters();
    })
    .set("registers", "Show registers", [this](HWND, int) {
        showRegisters();
    })
    .set("di", "Show registers", [this](HWND, int) {
        showRegisters();
    })
    .set("display", "Show registers", [this](HWND, int) {
        showRegisters();
    })
    .set("h",    "Open command help", [](HWND, int) { openHelp(); })
    .set("help", "Open command help", [](HWND, int) { openHelp(); })
    .set("tutor", "Open tutor", [](HWND, int) { openTutor(); })
    .set("tut",   "Open tutor", [](HWND, int) { openTutor(); });
}

static void appendNonKeymapHelp(std::string& help) {
    help += "\nCommon Commands\n";
    help += "---------------\n";
    help += ":e, :edit <file>   - Open file\n";
    help += ":w, :write         - Save file\n";
    help += ":q, :quit          - Close file\n";
    help += ":wq, :x            - Save and close\n";
    help += ":qa, :quitall      - Close all\n";
    help += ":wqa               - Save and close all\n";
    help += ":d, :delete [reg]  - Delete lines in range (e.g. :10,20d, :'<,'>d, :%d)\n";
    help += ":y, :yank [reg]    - Yank lines in range (e.g. :10,20y, :'<,'>y, :%y)\n";
    help += ":pu, :put [!] [reg]- Put register text after/before line\n";
    help += ":j, :join [!]      - Join lines in range (e.g. :10,20j, :'<,'>j)\n";
    help += ":sort [!][flags]   - Sort lines in range or file (flags: i, u, n, x, o, b, f, /pat/)\n";
    help += ":column [-t][-s][-o]- Format tabular data into aligned columns\n";
    help += ":retab [!] [tab]   - Retabulate spaces/tabs in range or file\n";
    help += ":m, :move {addr}   - Move lines in range to address\n";
    help += ":co, :t {addr}     - Copy lines in range to address\n";
    help += ":!cmd              - Execute external Windows command (e.g. :!python %, :!npm test)\n";
    help += ":[range]!cmd       - Filter lines through external program (e.g. :'<,'>!sort, :<>!column -t)\n";
    help += ":erc, :rc, :editrc - Open active nppvim.rc startup script\n";
    help += ":eini, :ini        - Open active config.ini configuration file\n";
    help += ":reload, :so       - Reload nppvim.rc configuration and mappings\n";
    help += ":config            - Open settings dialog\n";
    help += ":tutor             - Open interactive tutor\n";
    help += "\nConfiguration\n";
    help += "-------------\n";
    help += "NppVim uses two configuration files:\n";
    help += "  1. nppvim.rc  - Vim startup script for mappings and ':set' options\n";
    help += "                  Resolved order: 'rc_file' in config.ini, ~/.nppvimrc,\n";
    help += "                  or %APPDATA%\\Notepad++\\plugins\\Config\\NppVim\\nppvim.rc\n";
    help += "  2. config.ini - Plugin settings (rc_file, escape_key, overrides, layout)\n";
    help += "                  Located at %APPDATA%\\Notepad++\\plugins\\Config\\NppVim\\config.ini\n";
    help += "\nUse :set <option> to change settings. Available options:\n";
    help += "  number, relativenumber, hlsearch, ignorecase, smartcase,\n";
    help += "  expandtab, tabstop, shiftwidth, textwidth (tw), wrap,\n";
    help += "  cursorline, list, scrolloff, keylayout, normallayout,\n";
    help += "  insertlayout, langmap, clipboard\n";
    help += "\nExamples:\n";
    help += "  :erc\n";
    help += "  :eini\n";
    help += "  :reload\n";
    help += "  :set number\n";
    help += "  :set tabstop=4\n";
    help += "  :10,20sort n\n";
    help += "  :'<,'>!column -t\n";
    help += "  :!python %\n";
    help += "  :map K 5j\n";
    help += "  :nmap <C-S> :w<CR>\n";
}

void CommandMode::handleSubstitutionCommand(HWND hwndEdit, const std::string &cmd) {
  clearSubstitutionPreview(hwndEdit);

  SubstitutionParsed parsed;
  if (!parseSubstitutionCommand(cmd, hwndEdit, parsed)) {
    Utils::setStatus(TEXT("Invalid substitution command"));
    return;
  }

  performSubstitution(hwndEdit, parsed);
}

void CommandMode::performSubstitution(HWND hwndEdit, const SubstitutionParsed& parsed) {
  if (!hwndEdit || parsed.pattern.empty()) {
    Utils::setStatus(TEXT("Empty pattern"));
    return;
  }

  int searchFlags = 0;
  if (parsed.useRegex) searchFlags |= SCFIND_REGEXP;
  if (!parsed.caseInsensitive) searchFlags |= SCFIND_MATCHCASE;
  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, searchFlags, 0);

  // Save pattern as last search term
  state.lastSearchTerm = parsed.pattern;

  if (parsed.countOnly) {
    int totalMatches = 0;
    int matchingLinesCount = 0;

    for (int line = parsed.startLine; line <= parsed.endLine; line++) {
      int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
      int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
      if (lineStart >= lineEnd) continue;

      ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lineStart, 0);
      ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);

      bool lineMatched = false;
      int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());

      while (found != -1) {
        int mStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int mEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        if (mStart >= mEnd) break;

        totalMatches++;
        lineMatched = true;

        if (!parsed.replaceAll) break;

        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, mEnd, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());
      }

      if (lineMatched) matchingLinesCount++;
    }

    std::wstring msg = std::to_wstring(totalMatches) + L" match" + (totalMatches != 1 ? L"es" : L"") +
                       L" on " + std::to_wstring(matchingLinesCount) + L" line" + (matchingLinesCount != 1 ? L"s" : L"");
    Utils::setStatus(msg.c_str());
    return;
  }

  Utils::beginUndo(hwndEdit);

  std::string replacement = parsed.replacement;

  // Expand unescaped & to \0 for Scintilla regex replacement
  if (parsed.useRegex) {
    std::string expandedRep;
    for (size_t i = 0; i < replacement.size(); i++) {
      if (replacement[i] == '\\' && i + 1 < replacement.size()) {
        expandedRep += replacement[i];
        expandedRep += replacement[++i];
      } else if (replacement[i] == '&') {
        expandedRep += "\\0";
      } else {
        expandedRep += replacement[i];
      }
    }
    replacement = expandedRep;
  }

  int replacements = 0;
  int skipped = 0;
  int linesAffected = 0;

  if (parsed.confirmEach) {
    for (int line = parsed.startLine; line <= parsed.endLine; line++) {
      int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
      int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
      if (lineStart >= lineEnd) continue;

      ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lineStart, 0);
      ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);

      bool lineChanged = false;

      while (true) {
        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());
        if (found == -1) break;

        int mStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int mEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        if (mStart >= mEnd) break;

        ::SendMessage(hwndEdit, SCI_SETSEL, mStart, mEnd);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

        std::wstring prompt = L"Replace with \"" +
                              std::wstring(replacement.begin(), replacement.end()) +
                              L"\"? (y/n/a/q)";
        Utils::setStatus(prompt.c_str());

        char resp = (char)Utils::getCharBlocking();

        if (resp == 'y' || resp == 'Y') {
          ::SendMessage(hwndEdit, parsed.useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                        replacement.length(), (LPARAM)replacement.c_str());
          replacements++;
          lineChanged = true;

          int newEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
          lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

          if (!parsed.replaceAll) break;

          ::SendMessage(hwndEdit, SCI_SETTARGETSTART, newEnd, 0);
          ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);
        } else if (resp == 'n' || resp == 'N') {
          skipped++;
          if (!parsed.replaceAll) break;

          ::SendMessage(hwndEdit, SCI_SETTARGETSTART, mEnd, 0);
          ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);
        } else if (resp == 'a' || resp == 'A') {
          ::SendMessage(hwndEdit, parsed.useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                        replacement.length(), (LPARAM)replacement.c_str());
          replacements++;
          lineChanged = true;

          int newEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
          lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

          if (parsed.replaceAll) {
            ::SendMessage(hwndEdit, SCI_SETTARGETSTART, newEnd, 0);
            ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);

            int subFound = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());
            while (subFound != -1) {
              int nextStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
              int nextEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
              if (nextStart >= nextEnd) break;

              ::SendMessage(hwndEdit, parsed.useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                            replacement.length(), (LPARAM)replacement.c_str());
              replacements++;

              int afterEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
              lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

              ::SendMessage(hwndEdit, SCI_SETTARGETSTART, afterEnd, 0);
              ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);

              subFound = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());
            }
          }
          break;
        } else if (resp == 'q' || resp == 'Q') {
          line = parsed.endLine + 1;
          break;
        }
      }

      if (lineChanged) linesAffected++;
    }
  } else {
    for (int line = parsed.startLine; line <= parsed.endLine; line++) {
      int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
      int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
      if (lineStart >= lineEnd) continue;

      ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lineStart, 0);
      ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);

      bool lineChanged = false;
      int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());

      while (found != -1) {
        int mStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int mEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        if (mStart >= mEnd) break;

        ::SendMessage(hwndEdit, parsed.useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                      replacement.length(), (LPARAM)replacement.c_str());
        replacements++;
        lineChanged = true;

        int newEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        if (!parsed.replaceAll) break;

        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, newEnd, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, lineEnd, 0);

        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());
      }

      if (lineChanged) linesAffected++;
    }
  }

  Utils::endUndo(hwndEdit);

  if (replacements > 0) {
    std::wstring msg = std::to_wstring(replacements) + L" substitution" + (replacements > 1 ? L"s" : L"") +
                       L" on " + std::to_wstring(linesAffected) + L" line" + (linesAffected > 1 ? L"s" : L"");
    if (skipped > 0) {
      msg += L", " + std::to_wstring(skipped) + L" skipped";
    }
    Utils::setStatus(msg.c_str());
  } else if (!parsed.suppressError) {
    Utils::setStatus(TEXT("Pattern not found"));
  }
}

void CommandMode::performSearch(HWND hwndEdit, const std::string &searchTerm, int searchFlags)
{
  if (searchTerm.empty())
  {
    Utils::setStatus(TEXT("Empty search pattern"));
    return;
  }

  state.lastSearchTerm = searchTerm;
  state.searchFlags = searchFlags;
  state.lastSearchMatchCount = Utils::countSearchMatches(hwndEdit, searchTerm, searchFlags);
  Utils::updateSearchHighlight(hwndEdit, searchTerm, searchFlags);

  int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, searchFlags, 0);

  int startPos;
  if (state.mode == VISUAL && state.visualSearchAnchor != -1)
  {
    startPos = state.visualSearchAnchor;
  }
  else
  {
    startPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
  }

  ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
  ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

  int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                 (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

  if (found == -1)
  {
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, startPos, 0);
    found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                               (WPARAM)searchTerm.length(), (LPARAM)searchTerm.c_str());

    if (found != -1)
    {
      Utils::setStatus(TEXT("Search wrapped to top"));
    }
  }

  if (found != -1)
  {
    int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

    if (state.mode == VISUAL && state.visualSearchAnchor != -1)
    {
      if (state.visualSearchAnchor <= start)
      {
        ::SendMessage(hwndEdit, SCI_SETSEL, state.visualSearchAnchor, end);
      }
      else
      {
        ::SendMessage(hwndEdit, SCI_SETSEL, end, state.visualSearchAnchor);
      }
    }
    else
    {
      ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
    }

    ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
    Utils::showCurrentMatchPosition(hwndEdit, searchTerm, searchFlags);
  }
  else
  {
    Utils::setStatus(TEXT("Pattern not found"));
  }
}

void CommandMode::searchNext(HWND hwndEdit)
{
    if (state.lastSearchTerm.empty())
    {
        Utils::setStatus(TEXT("No previous search"));
        return;
    }

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, state.searchFlags, 0);

    // Search from current selection end to end of document
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1)
    {
        // Wrap to top: search from 0 to end of document
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1)
        {
            Utils::setStatus(TEXT("Search wrapped to top"));
        }
    }

    if (found != -1)
    {
        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.searchFlags);
    }
    else
    {
        Utils::setStatus(TEXT("Pattern not found"));
    }
}

void CommandMode::searchPrevious(HWND hwndEdit)
{
    if (state.lastSearchTerm.empty())
    {
        Utils::setStatus(TEXT("No previous search"));
        return;
    }

    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int startPos = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);

    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, state.searchFlags, 0);

    // Search backwards from selection start to beginning of document
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, 0, 0);

    int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
        (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    if (found == -1)
    {
        // Wrap to bottom: search backwards from end of document to beginning
        ::SendMessage(hwndEdit, SCI_SETTARGETSTART, docLen, 0);
        ::SendMessage(hwndEdit, SCI_SETTARGETEND, 0, 0);
        found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
            (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

        if (found != -1)
        {
            Utils::setStatus(TEXT("Search wrapped to bottom"));
        }
    }

    if (found != -1)
    {
        int start = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
        int end = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
        Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.searchFlags);
    }
    else
    {
        Utils::setStatus(TEXT("Pattern not found"));
    }
}

void CommandMode::showMarks(HWND hwndEdit, const std::string& args)
{
    std::string marksList = Marks::listMarks(hwndEdit, args);
    showOutputBuffer("Marks", marksList, 0);
    Utils::setStatus(TEXT("-- Marks list --"));
}

void CommandMode::handleDelmarksCommand(HWND hwndEdit, const std::string &baseCmd, const std::string &args)
{
    bool force = (baseCmd.find('!') != std::string::npos || args == "!");
    if (force)
    {
        Marks::clearLocalMarks(hwndEdit);
        Utils::setStatus(TEXT("-- Buffer marks deleted --"));
        return;
    }

    std::string marksArgs = args;
    if (marksArgs.empty())
    {
        Utils::setStatus(TEXT("E471: Argument required"));
        return;
    }

    int count = Marks::deleteMarks(hwndEdit, marksArgs);
    if (count > 0)
    {
        std::wstring msg = std::to_wstring(count) + L" mark" + (count > 1 ? L"s" : L"") + L" deleted";
        Utils::setStatus(msg.c_str());
    }
    else
    {
        Utils::setStatus(TEXT("E283: No marks matching"));
    }
}

void CommandMode::showBuffers()
{
    int count = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);
    if (count <= 0)
    {
        showOutputBuffer("Buffers", "No open buffers\n", 0);
        return;
    }

    std::vector<std::vector<TCHAR>> paths(count, std::vector<TCHAR>(MAX_PATH, 0));
    std::vector<TCHAR*> names(count);
    for (int i = 0; i < count; i++)
    {
        names[i] = paths[i].data();
    }
    ::SendMessage(nppData._nppHandle, NPPM_GETOPENFILENAMES, (WPARAM)names.data(), count);

    int currentDocIdx = (int)::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDOCINDEX, 0, MAIN_VIEW);

    std::string out;
    out += "  # Type Buffer Filename                                  Path\n";
    out += "─── ──── ────── ───────────────────────────────────────── ─────\n";

    for (int i = 0; i < count; i++)
    {
        std::string fullPath;
#ifdef UNICODE
        fullPath = Utils::toUtf8(paths[i].data());
#else
        fullPath = paths[i].data();
#endif
        std::string filename = fullPath;
        size_t lastSlash = filename.find_last_of("\\/");
        if (lastSlash != std::string::npos)
        {
            filename = filename.substr(lastSlash + 1);
        }
        if (filename.empty()) filename = "[No Name]";

        char ind[4] = "   ";
        if (i == currentDocIdx)
        {
            ind[0] = '%';
            ind[1] = 'a'; // active
        }
        else if (i == currentDocIdx - 1 || (currentDocIdx == 0 && i == 1))
        {
            ind[0] = '#';
            ind[1] = 'h'; // alternate
        }

        char lineBuf[512];
        sprintf_s(lineBuf, "%3d %s  \"%-35s\" %s\n",
                  i + 1, ind, filename.c_str(), fullPath.c_str());
        out += lineBuf;
    }

    showOutputBuffer("Buffers", out, 0);
    Utils::setStatus(TEXT("-- Buffers list --"));
}

void CommandMode::executeBufferSwitch(HWND hwndEdit, const std::string &arg)
{
    std::string target = Utils::trim(arg);
    if (target.empty())
    {
        showBuffers();
        return;
    }

    int count = (int)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);
    if (count <= 0) return;

    // Check if target is an integer (1-based buffer number)
    bool isNum = true;
    for (char c : target)
    {
        if (!std::isdigit(static_cast<unsigned char>(c)))
        {
            isNum = false;
            break;
        }
    }

    if (isNum)
    {
        try
        {
            int num = std::stoi(target);
            if (num >= 1 && num <= count)
            {
                ::SendMessage(nppData._nppHandle, NPPM_ACTIVATEDOC, MAIN_VIEW, num - 1);
                return;
            }
            else
            {
                Utils::setStatus(TEXT("E86: Buffer number out of range"));
                return;
            }
        }
        catch (...) {}
    }

    // Match by substring on filename
    std::vector<std::vector<TCHAR>> paths(count, std::vector<TCHAR>(MAX_PATH, 0));
    std::vector<TCHAR*> names(count);
    for (int i = 0; i < count; i++)
    {
        names[i] = paths[i].data();
    }
    ::SendMessage(nppData._nppHandle, NPPM_GETOPENFILENAMES, (WPARAM)names.data(), count);

    std::string targetLower = target;
    std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

    for (int i = 0; i < count; i++)
    {
        std::string fullPath;
#ifdef UNICODE
        fullPath = Utils::toUtf8(paths[i].data());
#else
        fullPath = paths[i].data();
#endif
        std::string lowerPath = fullPath;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

        if (lowerPath.find(targetLower) != std::string::npos)
        {
            ::SendMessage(nppData._nppHandle, NPPM_ACTIVATEDOC, MAIN_VIEW, i);
            return;
        }
    }

    Utils::setStatus(TEXT("E94: No matching buffer"));
}

void CommandMode::initSubstitutionIndicators(HWND h) {
    if (!h) return;
    ::SendMessage(h, SCI_INDICSETSTYLE, IND_SUB_MATCH, INDIC_ROUNDBOX);
    ::SendMessage(h, SCI_INDICSETFORE, IND_SUB_MATCH, RGB(255, 165, 0)); // Amber Orange
    ::SendMessage(h, SCI_INDICSETOUTLINEALPHA, IND_SUB_MATCH, 255);
    ::SendMessage(h, SCI_INDICSETALPHA, IND_SUB_MATCH, 100);

    ::SendMessage(h, SCI_INDICSETSTYLE, IND_SUB_REPL, INDIC_STRAIGHTBOX);
    ::SendMessage(h, SCI_INDICSETFORE, IND_SUB_REPL, RGB(0, 200, 120)); // Emerald Green
    ::SendMessage(h, SCI_INDICSETOUTLINEALPHA, IND_SUB_REPL, 255);
    ::SendMessage(h, SCI_INDICSETALPHA, IND_SUB_REPL, 120);
}

void CommandMode::clearSubstitutionPreview(HWND h) {
    if (!h) return;
    int textLen = (int)::SendMessage(h, SCI_GETTEXTLENGTH, 0, 0);
    if (textLen <= 0) return;

    ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_MATCH, 0);
    ::SendMessage(h, SCI_INDICATORCLEARRANGE, 0, textLen);

    ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_REPL, 0);
    ::SendMessage(h, SCI_INDICATORCLEARRANGE, 0, textLen);
}

bool CommandMode::parseSubstitutionCommand(
    const std::string& buf,
    HWND hwndEdit,
    SubstitutionParsed& res
) {
    res = SubstitutionParsed();
    if (buf.empty() || !hwndEdit) return false;

    std::string str = Utils::trim(buf);
    if (!str.empty() && str[0] == ':') str = Utils::trim(str.substr(1));
    if (str.empty()) return false;

    // Use parseRange to extract any range prefix (%, '<,'>, 1,10, etc.)
    ExRange exRange;
    size_t cmdStartPos = 0;
    bool hasRange = parseRange(str, hwndEdit, state, exRange, cmdStartPos);

    std::string cmdRem = str.substr(cmdStartPos);
    if (cmdRem.empty()) return false;

    // Now cmdRem must start with "substitute" or 's'
    size_t cmdLen = 0;
    if (cmdRem.rfind("substitute", 0) == 0) {
        cmdLen = 10;
        // Next char after "substitute" cannot be alphanumeric or underscore
        if (cmdRem.size() > 10 && (std::isalnum(static_cast<unsigned char>(cmdRem[10])) || cmdRem[10] == '_')) {
            return false;
        }
    } else if (cmdRem[0] == 's') {
        cmdLen = 1;
        // Next char after 's' cannot be alphanumeric or underscore (e.g. "sp", "sort", "set", "source", "so", "split")
        if (cmdRem.size() > 1 && (std::isalnum(static_cast<unsigned char>(cmdRem[1])) || cmdRem[1] == '_')) {
            return false;
        }
    } else {
        return false;
    }

    // It is indeed a substitution command!
    res.isSubstitution = true;
    res.rangeStr = str.substr(0, cmdStartPos);

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);

    if (hasRange) {
        res.startLine = exRange.startLine;
        res.endLine = exRange.endLine;
    } else {
        res.startLine = currentLine;
        res.endLine = currentLine;
    }

    if (res.startLine < 0) res.startLine = 0;
    if (res.endLine >= totalLines) res.endLine = (totalLines > 0) ? totalLines - 1 : 0;
    if (res.startLine > res.endLine) std::swap(res.startLine, res.endLine);

    size_t parseIdx = cmdLen;
    // Skip optional spaces before delimiter
    while (parseIdx < cmdRem.size() && (cmdRem[parseIdx] == ' ' || cmdRem[parseIdx] == '\t')) {
        parseIdx++;
    }

    if (parseIdx >= cmdRem.size()) {
        // Just ":s" or ":%s" alone
        if (!state.lastSearchTerm.empty()) {
            res.pattern = state.lastSearchTerm;
        }
        return true;
    }

    res.delimiter = cmdRem[parseIdx++];

    // Extract pattern
    bool escaped = false;
    while (parseIdx < cmdRem.size()) {
        char ch = cmdRem[parseIdx++];
        if (escaped) {
            res.pattern += ch;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == res.delimiter) {
            res.hasSecondDelimiter = true;
            break;
        } else {
            res.pattern += ch;
        }
    }

    if (!res.hasSecondDelimiter) {
        if (res.pattern.empty() && !state.lastSearchTerm.empty()) {
            res.pattern = state.lastSearchTerm;
        }
        return true;
    }

    // Extract replacement
    escaped = false;
    while (parseIdx < cmdRem.size()) {
        char ch = cmdRem[parseIdx++];
        if (escaped) {
            res.replacement += ch;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == res.delimiter) {
            res.hasThirdDelimiter = true;
            break;
        } else {
            res.replacement += ch;
        }
    }

    if (res.pattern.empty() && !state.lastSearchTerm.empty()) {
        res.pattern = state.lastSearchTerm;
    }

    if (!res.hasThirdDelimiter) return true;

    // Flags
    while (parseIdx < cmdRem.size()) {
        res.flags += cmdRem[parseIdx++];
    }

    for (char f : res.flags) {
        switch (f) {
            case 'g': res.replaceAll = true; break;
            case 'i': res.caseInsensitive = true; break;
            case 'I': res.caseInsensitive = false; break;
            case 'c': res.confirmEach = true; break;
            case 'n': res.countOnly = true; break;
            case 'e': res.suppressError = true; break;
            case 'l': res.useRegex = false; break;
            default: break;
        }
    }

    return true;
}

void CommandMode::previewSubstitution(HWND h, const SubstitutionParsed& parsed) {
    clearSubstitutionPreview(h);
    if (!h || !parsed.isSubstitution || parsed.pattern.empty()) {
        state.lastSearchMatchCount = -1;
        updateStatus();
        return;
    }

    int searchFlags = 0;
    if (parsed.useRegex) searchFlags |= SCFIND_REGEXP;
    if (!parsed.caseInsensitive) searchFlags |= SCFIND_MATCHCASE;
    ::SendMessage(h, SCI_SETSEARCHFLAGS, searchFlags, 0);

    int totalMatches = 0;
    int matchingLinesCount = 0;

    for (int line = parsed.startLine; line <= parsed.endLine; line++) {
        int lineStart = (int)::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
        int lineEnd = (int)::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
        if (lineStart >= lineEnd) continue;

        ::SendMessage(h, SCI_SETTARGETSTART, lineStart, 0);
        ::SendMessage(h, SCI_SETTARGETEND, lineEnd, 0);

        bool lineMatched = false;
        int found = (int)::SendMessage(h, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());

        while (found != -1) {
            int mStart = (int)::SendMessage(h, SCI_GETTARGETSTART, 0, 0);
            int mEnd = (int)::SendMessage(h, SCI_GETTARGETEND, 0, 0);

            if (mStart >= mEnd) break;

            totalMatches++;
            lineMatched = true;

            ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_MATCH, 0);
            ::SendMessage(h, SCI_INDICATORFILLRANGE, mStart, mEnd - mStart);

            if (parsed.hasSecondDelimiter) {
                ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_REPL, 0);
                ::SendMessage(h, SCI_INDICATORFILLRANGE, mStart, mEnd - mStart);
            }

            if (!parsed.replaceAll) {
                break;
            }

            ::SendMessage(h, SCI_SETTARGETSTART, mEnd, 0);
            ::SendMessage(h, SCI_SETTARGETEND, lineEnd, 0);
            found = (int)::SendMessage(h, SCI_SEARCHINTARGET, (WPARAM)parsed.pattern.length(), (LPARAM)parsed.pattern.c_str());
        }

        if (lineMatched) {
            matchingLinesCount++;
        }
    }

    state.lastSearchMatchCount = totalMatches;

    std::wstring display(state.commandBuffer.begin(), state.commandBuffer.end());
    if (totalMatches > 0) {
        display += L"  [" + std::to_wstring(totalMatches) + L" match" + (totalMatches > 1 ? L"es" : L"") +
                   L" on " + std::to_wstring(matchingLinesCount) + L" line" + (matchingLinesCount > 1 ? L"s" : L"") + L"]";
    } else if (!parsed.pattern.empty()) {
        display += L"  [Pattern not found]";
    }
    Utils::setStatus(display.c_str());
}

void CommandMode::previewSubstitutionFromBuffer(HWND h) {
    if (!h) return;
    if (state.commandBuffer == lastPreviewBuffer) return;
    lastPreviewBuffer = state.commandBuffer;

    SubstitutionParsed parsed;
    if (!parseSubstitutionCommand(state.commandBuffer, h, parsed)) {
        clearSubstitutionPreview(h);
        return;
    }

    previewSubstitution(h, parsed);
}

void CommandMode::showRegisters(const std::string& filterArgs) {
    std::string registersText = Registers::getInstance().formatRegisters(filterArgs);
    showOutputBuffer("Registers", registersText, 0);
    Utils::setStatus(TEXT("-- REGISTERS --"));
}

bool CommandMode::parseRange(const std::string& input, HWND hwndEdit, const VimState& state, ExRange& range, size_t& cmdStartPos) {
    range = ExRange();
    cmdStartPos = 0;
    if (input.empty() || !hwndEdit) return false;

    size_t offset = 0;
    while (offset < input.size() && (input[offset] == ' ' || input[offset] == '\t')) offset++;
    if (offset < input.size() && input[offset] == ':') {
        offset++;
        while (offset < input.size() && (input[offset] == ' ' || input[offset] == '\t')) offset++;
    }
    if (offset >= input.size()) return false;

    std::string str = input.substr(offset);

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);

    int selStart = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
    int selEnd = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
    int sL = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, (std::min)(selStart, selEnd), 0);
    int eL = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, (std::max)(selStart, selEnd), 0);
    if (selEnd > selStart && eL > sL) {
        int eLStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, eL, 0);
        if (eLStart == (std::max)(selStart, selEnd)) {
            eL--;
        }
    }

    // % -> whole file (1,$)
    if (str[0] == '%') {
        range.hasRange = true;
        range.startLine = 0;
        range.endLine = totalLines > 0 ? totalLines - 1 : 0;
        cmdStartPos = offset + 1;
        while (cmdStartPos < input.size() && (input[cmdStartPos] == ' ' || input[cmdStartPos] == '\t')) cmdStartPos++;
        range.rawRange = "%";
        return true;
    }

    // '<,'> or <,> or <> or '\<,'\>
    if (str.rfind("'<,'>", 0) == 0) {
        range.hasRange = true;
        range.startLine = sL;
        range.endLine = eL;
        cmdStartPos = offset + 5;
        while (cmdStartPos < input.size() && (input[cmdStartPos] == ' ' || input[cmdStartPos] == '\t')) cmdStartPos++;
        range.rawRange = "'<,'>";
        return true;
    }

    if (str.rfind("<,>", 0) == 0) {
        range.hasRange = true;
        range.startLine = sL;
        range.endLine = eL;
        cmdStartPos = offset + 3;
        while (cmdStartPos < input.size() && (input[cmdStartPos] == ' ' || input[cmdStartPos] == '\t')) cmdStartPos++;
        range.rawRange = "<,>";
        return true;
    }

    if (str.rfind("<>", 0) == 0) {
        range.hasRange = true;
        range.startLine = sL;
        range.endLine = eL;
        cmdStartPos = offset + 2;
        while (cmdStartPos < input.size() && (input[cmdStartPos] == ' ' || input[cmdStartPos] == '\t')) cmdStartPos++;
        range.rawRange = "<>";
        return true;
    }

    if (str.rfind("'\\<,'\\>", 0) == 0) {
        range.hasRange = true;
        range.startLine = sL;
        range.endLine = eL;
        cmdStartPos = offset + 7;
        while (cmdStartPos < input.size() && (input[cmdStartPos] == ' ' || input[cmdStartPos] == '\t')) cmdStartPos++;
        range.rawRange = "'\\<,'\\>";
        return true;
    }

    auto parseLineAddress = [&](const std::string& s, size_t& pos, int& outLine) -> bool {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
        if (pos >= s.size()) return false;

        int baseLine = -1;
        char c = s[pos];

        if (c == '.') {
            baseLine = currentLine;
            pos++;
        } else if (c == '$') {
            baseLine = totalLines > 0 ? totalLines - 1 : 0;
            pos++;
        } else if (c == '\'' && pos + 1 < s.size()) {
            char m = s[pos + 1];
            if (m == '<') {
                baseLine = sL;
                pos += 2;
            } else if (m == '>') {
                baseLine = eL;
                pos += 2;
            } else if (m == '\\' && pos + 2 < s.size() && (s[pos + 2] == '<' || s[pos + 2] == '>')) {
                baseLine = (s[pos + 2] == '<') ? sL : eL;
                pos += 3;
            } else {
                int mLine = Marks::getMarkLine(m);
                baseLine = (mLine >= 0) ? mLine : currentLine;
                pos += 2;
            }
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            int num = 0;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                num = num * 10 + (s[pos] - '0');
                pos++;
            }
            baseLine = num - 1;
        } else if (c == '+' || c == '-') {
            baseLine = currentLine;
        } else {
            return false;
        }

        // Parse optional offsets +N, -N
        while (pos < s.size()) {
            while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
            if (pos >= s.size()) break;
            if (s[pos] == '+') {
                pos++;
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
                int off = 0;
                if (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                        off = off * 10 + (s[pos] - '0');
                        pos++;
                    }
                } else {
                    off = 1;
                }
                baseLine += off;
            } else if (s[pos] == '-') {
                pos++;
                while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
                int off = 0;
                if (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                        off = off * 10 + (s[pos] - '0');
                        pos++;
                    }
                } else {
                    off = 1;
                }
                baseLine -= off;
            } else {
                break;
            }
        }

        outLine = baseLine;
        return true;
    };

    size_t pos = 0;
    int line1 = 0;
    if (parseLineAddress(str, pos, line1)) {
        range.hasRange = true;
        range.startLine = line1;
        range.endLine = line1;

        size_t sepPos = pos;
        while (sepPos < str.size() && (str[sepPos] == ' ' || str[sepPos] == '\t')) sepPos++;

        if (sepPos < str.size() && (str[sepPos] == ',' || str[sepPos] == ';')) {
            sepPos++;
            int line2 = 0;
            if (parseLineAddress(str, sepPos, line2)) {
                range.endLine = line2;
                pos = sepPos;
            }
        }

        cmdStartPos = offset + pos;
        while (cmdStartPos < input.size() && (input[cmdStartPos] == ' ' || input[cmdStartPos] == '\t')) cmdStartPos++;

        if (range.startLine < 0) range.startLine = 0;
        if (range.endLine < 0) range.endLine = 0;
        if (range.startLine >= totalLines) range.startLine = (totalLines > 0 ? totalLines - 1 : 0);
        if (range.endLine >= totalLines) range.endLine = (totalLines > 0 ? totalLines - 1 : 0);
        if (range.startLine > range.endLine) std::swap(range.startLine, range.endLine);

        range.rawRange = input.substr(offset, cmdStartPos - offset);
        return true;
    }

    if (state.mode == VISUAL) {
        range.hasRange = true;
        range.startLine = sL;
        range.endLine = eL;
        range.rawRange = "'<,'>";
        cmdStartPos = offset;
        return true;
    }

    return false;
}

std::string CommandMode::expandVimPathVariables(const std::string& cmdStr) {
    wchar_t fullPathW[MAX_PATH] = {0};
    ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)fullPathW);

    std::string fullPath = Utils::toUtf8(fullPathW);
    if (fullPath.empty()) {
        return cmdStr;
    }

    std::string p = fullPath;
    std::string h, t, r, e;

    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        h = fullPath.substr(0, lastSlash);
        t = fullPath.substr(lastSlash + 1);
    } else {
        h = ".";
        t = fullPath;
    }

    size_t lastDot = fullPath.find_last_of('.');
    if (lastDot != std::string::npos && (lastSlash == std::string::npos || lastDot > lastSlash)) {
        r = fullPath.substr(0, lastDot);
        e = fullPath.substr(lastDot + 1);
    } else {
        r = fullPath;
        e = "";
    }

    std::string tr;
    size_t tLastDot = t.find_last_of('.');
    if (tLastDot != std::string::npos) {
        tr = t.substr(0, tLastDot);
    } else {
        tr = t;
    }

    auto quoteIfSpaces = [](const std::string& s) -> std::string {
        if (s.find(' ') != std::string::npos && !s.empty() && s.front() != '"' && s.front() != '\'') {
            return "\"" + s + "\"";
        }
        return s;
    };

    std::string q_p = quoteIfSpaces(p);
    std::string q_h = quoteIfSpaces(h);
    std::string q_t = quoteIfSpaces(t);
    std::string q_r = quoteIfSpaces(r);
    std::string q_tr = quoteIfSpaces(tr);

    std::vector<std::pair<std::string, std::string>> replacements = {
        {"%:p:h", q_h},
        {"%:p:r", q_r},
        {"%:t:r", q_tr},
        {"%:p", q_p},
        {"%:t", q_t},
        {"%:h", q_h},
        {"%:r", q_r},
        {"%:e", e},
        {"%", q_p}
    };

    std::string result;
    for (size_t i = 0; i < cmdStr.size(); ) {
        if (cmdStr[i] == '\\' && i + 1 < cmdStr.size() && cmdStr[i + 1] == '%') {
            result += '%';
            i += 2;
            continue;
        }

        bool matched = false;
        for (const auto& rep : replacements) {
            if (cmdStr.compare(i, rep.first.size(), rep.first) == 0) {
                result += rep.second;
                i += rep.first.size();
                matched = true;
                break;
            }
        }

        if (!matched) {
            result += cmdStr[i];
            i++;
        }
    }

    return result;
}

void CommandMode::executeDelete(HWND hwndEdit, const ExRange& range, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int currentLine = Utils::caretLine(hwndEdit);
    int startLine = range.hasRange ? range.startLine : currentLine;
    int endLine = range.hasRange ? range.endLine : currentLine;

    char reg = '"';
    std::string regArg = Utils::trim(args);
    if (!regArg.empty() && !std::isdigit(static_cast<unsigned char>(regArg[0]))) {
        if (Registers::isValidRegister(regArg[0])) {
            reg = regArg[0];
            regArg = Utils::trim(regArg.substr(1));
        }
    } else {
        reg = Registers::getInstance().getActiveRegister();
    }

    if (!range.hasRange && !regArg.empty() && std::isdigit(static_cast<unsigned char>(regArg[0]))) {
        try {
            int count = std::stoi(regArg);
            if (count > 1) {
                endLine = (std::min)(totalLines - 1, currentLine + count - 1);
            }
        } catch (...) {}
    }

    if (totalLines <= 0 || startLine > endLine || startLine >= totalLines) return;
    if (endLine >= totalLines) endLine = totalLines - 1;

    EditRange editRange = EditRange::fromLines(hwndEdit, startLine, endLine);
    EditorOps::erase(hwndEdit, editRange, reg, g_config.dStoreClipboard);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    int lineCount = endLine - startLine + 1;
    if (lineCount == 1) {
        Utils::setStatus(TEXT("1 line deleted"));
    } else {
        Utils::setStatus((std::to_wstring(lineCount) + L" fewer lines").c_str());
    }
}

void CommandMode::executeYank(HWND hwndEdit, const ExRange& range, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int currentLine = Utils::caretLine(hwndEdit);
    int startLine = range.hasRange ? range.startLine : currentLine;
    int endLine = range.hasRange ? range.endLine : currentLine;

    char reg = '"';
    std::string regArg = Utils::trim(args);
    if (!regArg.empty() && !std::isdigit(static_cast<unsigned char>(regArg[0]))) {
        if (Registers::isValidRegister(regArg[0])) {
            reg = regArg[0];
            regArg = Utils::trim(regArg.substr(1));
        }
    } else {
        reg = Registers::getInstance().getActiveRegister();
    }

    if (!range.hasRange && !regArg.empty() && std::isdigit(static_cast<unsigned char>(regArg[0]))) {
        try {
            int count = std::stoi(regArg);
            if (count > 1) {
                endLine = (std::min)(totalLines - 1, currentLine + count - 1);
            }
        } catch (...) {}
    }

    if (totalLines <= 0 || startLine > endLine || startLine >= totalLines) return;
    if (endLine >= totalLines) endLine = totalLines - 1;

    EditRange editRange = EditRange::fromLines(hwndEdit, startLine, endLine);
    EditorOps::yank(hwndEdit, editRange, reg, true);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    int lineCount = endLine - startLine + 1;
    if (lineCount == 1) {
        Utils::setStatus(TEXT("1 line yanked"));
    } else {
        Utils::setStatus((std::to_wstring(lineCount) + L" lines yanked").c_str());
    }
}

void CommandMode::executePut(HWND hwndEdit, const ExRange& range, bool before, const std::string& args) {
    if (!hwndEdit) return;

    char reg = '"';
    std::string regArg = Utils::trim(args);
    if (!regArg.empty() && Registers::isValidRegister(regArg[0])) {
        reg = regArg[0];
    } else {
        reg = Registers::getInstance().getActiveRegister();
    }

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int targetLine = range.hasRange ? range.endLine : Utils::caretLine(hwndEdit);
    if (targetLine < 0) targetLine = 0;
    if (targetLine >= totalLines) targetLine = (totalLines > 0 ? totalLines - 1 : 0);

    int pos = Utils::lineStart(hwndEdit, targetLine);
    EditorOps::put(hwndEdit, pos, reg, before, true);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    Utils::setStatus(TEXT("1 line inserted"));
}

void CommandMode::executeJoin(HWND hwndEdit, const ExRange& range, bool withSpace, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int currentLine = Utils::caretLine(hwndEdit);
    int startLine = range.hasRange ? range.startLine : currentLine;
    int endLine = range.hasRange ? range.endLine : (currentLine + 1);

    if (!range.hasRange && !args.empty()) {
        try {
            int count = std::stoi(args);
            if (count > 1) {
                endLine = (std::min)(totalLines - 1, currentLine + count - 1);
            }
        } catch (...) {}
    }

    if (startLine == endLine && startLine + 1 < totalLines) {
        endLine = startLine + 1;
    }

    if (startLine >= endLine || startLine >= totalLines) {
        return;
    }
    if (endLine >= totalLines) endLine = totalLines - 1;

    int count = endLine - startLine;
    EditorOps::joinLines(hwndEdit, startLine, count, withSpace);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    int joinedCount = endLine - startLine + 1;
    Utils::setStatus((std::to_wstring(joinedCount) + L" lines joined").c_str());
}

void CommandMode::executeSort(HWND hwndEdit, const ExRange& range, bool reverse, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int startLine = range.hasRange ? range.startLine : 0;
    int endLine = range.hasRange ? range.endLine : (totalLines > 0 ? totalLines - 1 : 0);

    if (totalLines <= 0 || startLine > endLine || startLine >= totalLines) return;
    if (endLine >= totalLines) endLine = totalLines - 1;

    bool optReverse = reverse;
    bool optIgnoreCase = false;
    bool optUnique = false;
    enum NumType { None, Decimal, Hex, Octal, Binary, Float } numType = None;
    bool optSortOnPattern = false;
    std::string pattern;
    bool hasPattern = false;

    std::string argStr = args;
    size_t patStart = argStr.find('/');
    if (patStart != std::string::npos) {
        size_t patEnd = argStr.find('/', patStart + 1);
        if (patEnd != std::string::npos) {
            pattern = argStr.substr(patStart + 1, patEnd - patStart - 1);
            hasPattern = true;
            argStr.erase(patStart, patEnd - patStart + 1);
        }
    }

    for (char c : argStr) {
        switch (c) {
            case '!': optReverse = true; break;
            case 'i': optIgnoreCase = true; break;
            case 'u': optUnique = true; break;
            case 'n': numType = Decimal; break;
            case 'x': numType = Hex; break;
            case 'o': numType = Octal; break;
            case 'b': numType = Binary; break;
            case 'f': numType = Float; break;
            case 'r': optSortOnPattern = true; break;
            default: break;
        }
    }

    std::regex reg;
    if (hasPattern && !pattern.empty()) {
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (optIgnoreCase) flags |= std::regex_constants::icase;
            reg = std::regex(pattern, flags);
        } catch (...) {
            hasPattern = false;
        }
    }

    struct LineRecord {
        std::string originalText;
        std::string compareStr;
        long long numInt = 0;
        double numFloat = 0.0;
        int origIndex = 0;
    };

    std::vector<LineRecord> items;
    items.reserve(endLine - startLine + 1);

    for (int line = startLine; line <= endLine; line++) {
        int ls = Utils::lineStart(hwndEdit, line);
        int le = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
        std::string lineContent = Utils::getTextRange(hwndEdit, ls, le);

        LineRecord rec;
        rec.originalText = lineContent;
        rec.origIndex = line - startLine;

        std::string key = lineContent;
        if (hasPattern) {
            std::smatch m;
            if (std::regex_search(lineContent, m, reg)) {
                if (optSortOnPattern) {
                    key = m.str();
                } else {
                    key = lineContent.substr(m.position() + m.length());
                }
            }
        }

        if (numType == Decimal) {
            size_t idx = 0;
            while (idx < key.size() && !std::isdigit(static_cast<unsigned char>(key[idx])) && key[idx] != '-' && key[idx] != '+') idx++;
            if (idx < key.size()) {
                try {
                    rec.numInt = std::stoll(key.substr(idx));
                } catch (...) {
                    rec.numInt = 0;
                }
            }
        } else if (numType == Hex) {
            size_t idx = 0;
            while (idx < key.size() && !std::isxdigit(static_cast<unsigned char>(key[idx]))) idx++;
            if (idx >= 2 && (key[idx-1] == 'x' || key[idx-1] == 'X') && key[idx-2] == '0') idx -= 2;
            if (idx < key.size()) {
                try {
                    rec.numInt = std::stoll(key.substr(idx), nullptr, 16);
                } catch (...) {
                    rec.numInt = 0;
                }
            }
        } else if (numType == Octal) {
            size_t idx = 0;
            while (idx < key.size() && (key[idx] < '0' || key[idx] > '7')) idx++;
            if (idx < key.size()) {
                try {
                    rec.numInt = std::stoll(key.substr(idx), nullptr, 8);
                } catch (...) {
                    rec.numInt = 0;
                }
            }
        } else if (numType == Binary) {
            size_t idx = 0;
            while (idx < key.size() && key[idx] != '0' && key[idx] != '1') idx++;
            if (idx < key.size()) {
                try {
                    rec.numInt = std::stoll(key.substr(idx), nullptr, 2);
                } catch (...) {
                    rec.numInt = 0;
                }
            }
        } else if (numType == Float) {
            size_t idx = 0;
            while (idx < key.size() && !std::isdigit(static_cast<unsigned char>(key[idx])) && key[idx] != '-' && key[idx] != '+' && key[idx] != '.') idx++;
            if (idx < key.size()) {
                try {
                    rec.numFloat = std::stod(key.substr(idx));
                } catch (...) {
                    rec.numFloat = 0.0;
                }
            }
        }

        if (optIgnoreCase) {
            std::string lower = key;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            rec.compareStr = lower;
        } else {
            rec.compareStr = key;
        }

        items.push_back(rec);
    }

    std::stable_sort(items.begin(), items.end(), [&](const LineRecord& a, const LineRecord& b) -> bool {
        if (numType == Decimal || numType == Hex || numType == Octal || numType == Binary) {
            if (a.numInt != b.numInt) {
                return optReverse ? (a.numInt > b.numInt) : (a.numInt < b.numInt);
            }
        } else if (numType == Float) {
            if (a.numFloat != b.numFloat) {
                return optReverse ? (a.numFloat > b.numFloat) : (a.numFloat < b.numFloat);
            }
        }
        if (a.compareStr != b.compareStr) {
            return optReverse ? (a.compareStr > b.compareStr) : (a.compareStr < b.compareStr);
        }
        return false;
    });

    int originalCount = (int)items.size();
    if (optUnique) {
        auto it = std::unique(items.begin(), items.end(), [&](const LineRecord& a, const LineRecord& b) {
            if (numType == Decimal || numType == Hex || numType == Octal || numType == Binary) {
                return a.numInt == b.numInt && a.compareStr == b.compareStr;
            }
            if (numType == Float) {
                return a.numFloat == b.numFloat && a.compareStr == b.compareStr;
            }
            return a.compareStr == b.compareStr;
        });
        items.erase(it, items.end());
    }
    int duplicatesRemoved = originalCount - (int)items.size();

    int posStart = Utils::lineStart(hwndEdit, startLine);
    auto endRange = Utils::lineRange(hwndEdit, endLine, true);
    int posEnd = endRange.second;

    int eolMode = (int)::SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
    std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");

    std::string sortedBlock;
    for (size_t i = 0; i < items.size(); i++) {
        sortedBlock += items[i].originalText;
        if (i + 1 < items.size() || (endLine < totalLines - 1)) {
            sortedBlock += eolStr;
        }
    }

    Utils::beginUndo(hwndEdit);
    Utils::select(hwndEdit, posStart, posEnd);
    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)sortedBlock.c_str());
    Utils::endUndo(hwndEdit);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    std::wstring status = std::to_wstring(items.size()) + L" lines sorted";
    if (optUnique && duplicatesRemoved > 0) {
        status += L", " + std::to_wstring(duplicatesRemoved) + L" duplicates removed";
    }
    Utils::setStatus(status.c_str());
}

void CommandMode::executeRetab(HWND hwndEdit, const ExRange& range, bool allSpaces, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int startLine = range.hasRange ? range.startLine : 0;
    int endLine = range.hasRange ? range.endLine : (totalLines > 0 ? totalLines - 1 : 0);

    if (totalLines <= 0 || startLine > endLine || startLine >= totalLines) return;
    if (endLine >= totalLines) endLine = totalLines - 1;

    int tabstop = (int)::SendMessage(hwndEdit, SCI_GETTABWIDTH, 0, 0);
    if (!args.empty()) {
        try {
            int t = std::stoi(args);
            if (t > 0) tabstop = t;
        } catch (...) {}
    }
    if (tabstop <= 0) tabstop = 4;

    bool expandTab = false;
    try {
        OptionValue val = OptionRegistry::getInstance().getOption("expandtab");
        if (std::holds_alternative<bool>(val)) {
            expandTab = std::get<bool>(val);
        } else {
            expandTab = (::SendMessage(hwndEdit, SCI_GETUSETABS, 0, 0) == 0);
        }
    } catch (...) {
        expandTab = (::SendMessage(hwndEdit, SCI_GETUSETABS, 0, 0) == 0);
    }

    int posStart = Utils::lineStart(hwndEdit, startLine);
    auto endRange = Utils::lineRange(hwndEdit, endLine, true);
    int posEnd = endRange.second;

    int eolMode = (int)::SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
    std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");

    std::vector<std::string> newLines;
    for (int line = startLine; line <= endLine; line++) {
        int ls = Utils::lineStart(hwndEdit, line);
        int le = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
        std::string lineStr = Utils::getTextRange(hwndEdit, ls, le);

        std::string converted;
        if (expandTab) {
            int col = 0;
            bool leadingOnly = !allSpaces;
            bool seenNonSpace = false;
            for (char ch : lineStr) {
                if (ch == '\t' && (!leadingOnly || !seenNonSpace)) {
                    int spacesToAdd = tabstop - (col % tabstop);
                    converted.append(spacesToAdd, ' ');
                    col += spacesToAdd;
                } else {
                    if (ch != ' ' && ch != '\t') seenNonSpace = true;
                    converted.push_back(ch);
                    col++;
                }
            }
        } else {
            int col = 0;
            bool leadingOnly = !allSpaces;
            bool seenNonSpace = false;
            int pendingSpaces = 0;

            for (size_t i = 0; i < lineStr.size(); i++) {
                char ch = lineStr[i];
                if (ch == ' ' && (!leadingOnly || !seenNonSpace)) {
                    pendingSpaces++;
                    col++;
                    if (col % tabstop == 0) {
                        converted.push_back('\t');
                        pendingSpaces = 0;
                    }
                } else {
                    if (pendingSpaces > 0) {
                        converted.append(pendingSpaces, ' ');
                        pendingSpaces = 0;
                    }
                    if (ch == '\t') {
                        converted.push_back('\t');
                        col += tabstop - (col % tabstop);
                    } else {
                        if (ch != ' ' && ch != '\t') seenNonSpace = true;
                        converted.push_back(ch);
                        col++;
                    }
                }
            }
            if (pendingSpaces > 0) {
                converted.append(pendingSpaces, ' ');
            }
        }
        newLines.push_back(converted);
    }

    std::string fullNewText;
    for (size_t i = 0; i < newLines.size(); i++) {
        fullNewText += newLines[i];
        if (i + 1 < newLines.size() || (endLine < totalLines - 1)) {
            fullNewText += eolStr;
        }
    }

    Utils::beginUndo(hwndEdit);
    Utils::select(hwndEdit, posStart, posEnd);
    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)fullNewText.c_str());
    Utils::endUndo(hwndEdit);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    int affected = endLine - startLine + 1;
    Utils::setStatus((L"Retabbed " + std::to_wstring(affected) + L" line" + (affected > 1 ? L"s" : L"") + L" (tabstop " + std::to_wstring(tabstop) + L")").c_str());
}

void CommandMode::executeColumn(HWND hwndEdit, const ExRange& range, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    if (totalLines <= 0) return;

    int startLine = 0;
    int endLine = totalLines - 1;

    if (range.hasRange) {
        startLine = range.startLine;
        endLine = range.endLine;
    } else {
        int selStart = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
        int selEnd = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
        if (selEnd > selStart) {
            startLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, (std::min)(selStart, selEnd), 0);
            endLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, (std::max)(selStart, selEnd), 0);
            int eLStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine, 0);
            if (eLStart == (std::max)(selStart, selEnd) && endLine > startLine) {
                endLine--;
            }
        }
    }

    if (startLine > endLine || startLine >= totalLines) return;
    if (startLine < 0) startLine = 0;
    if (endLine >= totalLines) endLine = totalLines - 1;

    // Parse arguments
    std::string inputDelims = "";
    std::string outputDelim = "  "; // default 2 spaces for standard column -t
    std::set<int> rightAlignCols;

    auto unescapeStr = [](const std::string& str) -> std::string {
        std::string s = str;
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            s = s.substr(1, s.size() - 2);
        }
        std::string res;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char next = s[i + 1];
                if (next == 't') { res += '\t'; i++; }
                else if (next == 'n') { res += '\n'; i++; }
                else if (next == 'r') { res += '\r'; i++; }
                else if (next == 's') { res += ' '; i++; }
                else if (next == '\\') { res += '\\'; i++; }
                else if (next == '"') { res += '"'; i++; }
                else if (next == '\'') { res += '\''; i++; }
                else { res += s[i]; }
            } else {
                res += s[i];
            }
        }
        return res;
    };

    auto parseColList = [](const std::string& str, std::set<int>& cols) {
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = Utils::trim(item);
            if (!item.empty()) {
                try {
                    int c = std::stoi(item);
                    if (c > 0) cols.insert(c);
                } catch (...) {}
            }
        }
    };

    // Tokenize args preserving quotes
    std::vector<std::string> tokens;
    std::string curToken;
    bool inQuotes = false;
    char quoteChar = '\0';
    for (size_t i = 0; i < args.size(); ++i) {
        char ch = args[i];
        if ((ch == '"' || ch == '\'') && (!inQuotes || ch == quoteChar)) {
            inQuotes = !inQuotes;
            if (inQuotes) quoteChar = ch;
            else quoteChar = '\0';
        } else if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
            if (!curToken.empty()) {
                tokens.push_back(curToken);
                curToken.clear();
            }
        } else {
            curToken += ch;
        }
    }
    if (!curToken.empty()) {
        tokens.push_back(curToken);
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        if (tok == "-t") {
            // Table mode
        } else if (tok == "-s" && i + 1 < tokens.size()) {
            i++;
            inputDelims = unescapeStr(tokens[i]);
        } else if (tok.rfind("-s", 0) == 0 && tok.size() > 2) {
            inputDelims = unescapeStr(tok.substr(2));
        } else if (tok == "-o" && i + 1 < tokens.size()) {
            i++;
            outputDelim = unescapeStr(tokens[i]);
        } else if (tok.rfind("-o", 0) == 0 && tok.size() > 2) {
            outputDelim = unescapeStr(tok.substr(2));
        } else if (tok == "-R" && i + 1 < tokens.size()) {
            i++;
            parseColList(tokens[i], rightAlignCols);
        } else if (tok.rfind("-R", 0) == 0 && tok.size() > 2) {
            parseColList(tok.substr(2), rightAlignCols);
        }
    }

    // Read and split lines
    std::vector<std::vector<std::string>> rows;
    std::vector<bool> isOriginalEmpty;
    size_t maxCols = 0;

    for (int line = startLine; line <= endLine; ++line) {
        auto rng = Utils::lineRange(hwndEdit, line, false);
        std::string lineStr = (rng.second > rng.first) ? Utils::getTextRange(hwndEdit, rng.first, rng.second) : "";

        // Remove trailing \r and \n
        while (!lineStr.empty() && (lineStr.back() == '\r' || lineStr.back() == '\n')) {
            lineStr.pop_back();
        }

        bool emptyLine = lineStr.empty() || lineStr.find_first_not_of(" \t") == std::string::npos;
        isOriginalEmpty.push_back(emptyLine);

        std::vector<std::string> cells;
        if (emptyLine && inputDelims.empty()) {
            // Keep empty
        } else if (inputDelims.empty()) {
            // Whitespace delimited
            size_t idx = 0;
            while (idx < lineStr.size()) {
                while (idx < lineStr.size() && (lineStr[idx] == ' ' || lineStr[idx] == '\t')) idx++;
                if (idx >= lineStr.size()) break;
                size_t cellStart = idx;
                while (idx < lineStr.size() && lineStr[idx] != ' ' && lineStr[idx] != '\t') idx++;
                cells.push_back(lineStr.substr(cellStart, idx - cellStart));
            }
        } else {
            // Custom delimiters
            size_t start = 0;
            while (start <= lineStr.size()) {
                size_t delimPos = lineStr.find_first_of(inputDelims, start);
                if (delimPos == std::string::npos) {
                    std::string cell = lineStr.substr(start);
                    cells.push_back(Utils::trim(cell));
                    break;
                } else {
                    std::string cell = lineStr.substr(start, delimPos - start);
                    cells.push_back(Utils::trim(cell));
                    start = delimPos + 1;
                }
            }
        }

        maxCols = (std::max)(maxCols, cells.size());
        rows.push_back(cells);
    }

    if (maxCols == 0) return;

    auto getDisplayWidth = [](const std::string& str) -> size_t {
        std::wstring wstr = Utils::toWide(str);
        return wstr.empty() ? str.size() : wstr.size();
    };

    std::vector<size_t> colWidths(maxCols, 0);
    for (const auto& row : rows) {
        for (size_t c = 0; c < row.size(); ++c) {
            colWidths[c] = (std::max)(colWidths[c], getDisplayWidth(row[c]));
        }
    }

    int eolMode = (int)::SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
    std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");

    std::vector<std::string> formattedLines;
    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        if (isOriginalEmpty[r] && row.empty()) {
            formattedLines.push_back("");
        } else {
            std::string lineOut;
            for (size_t c = 0; c < row.size(); ++c) {
                if (c > 0) {
                    lineOut += outputDelim;
                }
                const std::string& cell = row[c];
                size_t w = getDisplayWidth(cell);
                size_t targetW = colWidths[c];
                bool rightAlign = (rightAlignCols.count((int)(c + 1)) > 0);

                if (c == row.size() - 1) {
                    if (rightAlign && w < targetW) {
                        lineOut.append(targetW - w, ' ');
                    }
                    lineOut += cell;
                } else {
                    if (rightAlign) {
                        if (w < targetW) lineOut.append(targetW - w, ' ');
                        lineOut += cell;
                    } else {
                        lineOut += cell;
                        if (w < targetW) lineOut.append(targetW - w, ' ');
                    }
                }
            }
            formattedLines.push_back(lineOut);
        }
    }

    int posStart = Utils::lineStart(hwndEdit, startLine);
    auto endRange = Utils::lineRange(hwndEdit, endLine, true);
    int posEnd = endRange.second;

    std::string formattedBlock;
    for (size_t i = 0; i < formattedLines.size(); i++) {
        formattedBlock += formattedLines[i];
        if (i + 1 < formattedLines.size() || (endLine < totalLines - 1)) {
            formattedBlock += eolStr;
        }
    }

    Utils::beginUndo(hwndEdit);
    Utils::select(hwndEdit, posStart, posEnd);
    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)formattedBlock.c_str());
    Utils::endUndo(hwndEdit);

    if (g_normalMode) {
        g_normalMode->enter();
    }

    int affectedLines = endLine - startLine + 1;
    std::wstring msg = L"Formatted " + std::to_wstring(affectedLines) + L" line" + (affectedLines > 1 ? L"s" : L"") + L" (" + std::to_wstring(maxCols) + L" columns)";
    Utils::setStatus(msg.c_str());
}

void CommandMode::executeExternal(HWND hwndEdit, const ExRange& range, const std::string& cmdStr) {
    if (cmdStr.empty()) {
        Utils::setStatus(TEXT("No external command specified"));
        return;
    }

    std::string trimmedCmd = Utils::trim(cmdStr);
    std::string firstWord;
    size_t sp = trimmedCmd.find_first_of(" \t");
    if (sp != std::string::npos) {
        firstWord = trimmedCmd.substr(0, sp);
    } else {
        firstWord = trimmedCmd;
    }

    if (firstWord == "column" || firstWord == "column.exe" || firstWord == "col") {
        std::string colArgs = (sp != std::string::npos) ? Utils::trim(trimmedCmd.substr(sp)) : "";
        executeColumn(hwndEdit, range, colArgs);
        return;
    }

    std::string expandedCmd = expandVimPathVariables(cmdStr);

    wchar_t currentFile[MAX_PATH] = {0};
    ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)currentFile);
    wchar_t currentDir[MAX_PATH] = {0};
    if (currentFile[0] != L'\0') {
        wcscpy_s(currentDir, currentFile);
        PathRemoveFileSpecW(currentDir);
    }

    LPCWSTR pWorkDir = (currentDir[0] != L'\0') ? currentDir : NULL;

    bool isFilter = range.hasRange;
    std::string inputData;

    if (isFilter) {
        int posStart = Utils::lineStart(hwndEdit, range.startLine);
        auto endRange = Utils::lineRange(hwndEdit, range.endLine, true);
        int posEnd = endRange.second;
        inputData = Utils::getTextRange(hwndEdit, posStart, posEnd);
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdInRead = NULL, hStdInWrite = NULL;
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;

    if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0) ||
        !CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
        !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        Utils::setStatus(TEXT("Failed to create process pipes"));
        if (hStdInRead) CloseHandle(hStdInRead);
        if (hStdInWrite) CloseHandle(hStdInWrite);
        if (hStdOutRead) CloseHandle(hStdOutRead);
        if (hStdOutWrite) CloseHandle(hStdOutWrite);
        if (hStdErrRead) CloseHandle(hStdErrRead);
        if (hStdErrWrite) CloseHandle(hStdErrWrite);
        return;
    }

    SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hStdInRead;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLineW = L"cmd.exe /c " + std::wstring(expandedCmd.begin(), expandedCmd.end());
    std::vector<wchar_t> cmdLineBuf(cmdLineW.begin(), cmdLineW.end());
    cmdLineBuf.push_back(L'\0');

    BOOL created = CreateProcessW(
        NULL,
        cmdLineBuf.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        pWorkDir,
        &si,
        &pi
    );

    CloseHandle(hStdInRead);
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    if (!created) {
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        std::wstring err = L"Failed to execute: " + std::wstring(expandedCmd.begin(), expandedCmd.end());
        Utils::setStatus(err.c_str());
        return;
    }

    if (isFilter && !inputData.empty()) {
        DWORD bytesWritten = 0;
        WriteFile(hStdInWrite, inputData.c_str(), (DWORD)inputData.length(), &bytesWritten, NULL);
    }
    CloseHandle(hStdInWrite);

    std::string stdOutStr;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        stdOutStr.append(buffer, bytesRead);
    }
    CloseHandle(hStdOutRead);

    std::string stdErrStr;
    while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        stdErrStr.append(buffer, bytesRead);
    }
    CloseHandle(hStdErrRead);

    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (isFilter) {
        if (exitCode == 0 || !stdOutStr.empty()) {
            int posStart = Utils::lineStart(hwndEdit, range.startLine);
            auto endRange = Utils::lineRange(hwndEdit, range.endLine, true);
            int posEnd = endRange.second;

            int eolMode = (int)::SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
            std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");

            std::string normalized;
            for (size_t i = 0; i < stdOutStr.size(); i++) {
                if (stdOutStr[i] == '\r') {
                    if (i + 1 < stdOutStr.size() && stdOutStr[i + 1] == '\n') {
                        i++;
                    }
                    normalized += eolStr;
                } else if (stdOutStr[i] == '\n') {
                    normalized += eolStr;
                } else {
                    normalized += stdOutStr[i];
                }
            }

            Utils::beginUndo(hwndEdit);
            Utils::select(hwndEdit, posStart, posEnd);
            ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)normalized.c_str());
            Utils::endUndo(hwndEdit);

            if (state.mode == VISUAL && g_normalMode) {
                g_normalMode->enter();
            }

            int outLines = 0;
            for (char ch : normalized) if (ch == '\n') outLines++;
            if (outLines <= 0) outLines = 1;

            std::wstring statusMsg = std::to_wstring(outLines) + L" lines filtered (Exit " + std::to_wstring(exitCode) + L")";
            Utils::setStatus(statusMsg.c_str());
        } else {
            std::string errMsg = "Shell returned " + std::to_string(exitCode);
            if (!stdErrStr.empty()) errMsg += ": " + Utils::trim(stdErrStr);
            Utils::setStatus(std::wstring(errMsg.begin(), errMsg.end()).c_str());
        }
    } else {
        std::string allOutput = stdOutStr;
        if (!stdErrStr.empty()) {
            if (!allOutput.empty()) allOutput += "\n[stderr]\n";
            allOutput += stdErrStr;
        }

        allOutput = Utils::trim(allOutput);

        if (allOutput.empty()) {
            std::wstring statusMsg = L"[Exit " + std::to_wstring(exitCode) + L"] Command executed: " + std::wstring(expandedCmd.begin(), expandedCmd.end());
            Utils::setStatus(statusMsg.c_str());
        } else {
            int lineCount = 1;
            for (char ch : allOutput) if (ch == '\n') lineCount++;

            if (lineCount <= 2 && allOutput.length() < 120) {
                std::wstring statusMsg = L"[Exit " + std::to_wstring(exitCode) + L"] " + std::wstring(allOutput.begin(), allOutput.end());
                Utils::setStatus(statusMsg.c_str());
            } else {
                showOutputBuffer(expandedCmd, allOutput, exitCode);
                std::wstring statusMsg = L"[Exit " + std::to_wstring(exitCode) + L"] " + std::to_wstring(lineCount) + L" lines of output";
                Utils::setStatus(statusMsg.c_str());
            }
        }
    }
}

void CommandMode::executeMove(HWND hwndEdit, const ExRange& range, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    if (totalLines <= 0) return;

    int currentLine = Utils::caretLine(hwndEdit);
    int startLine = range.hasRange ? range.startLine : currentLine;
    int endLine = range.hasRange ? range.endLine : currentLine;

    if (startLine < 0) startLine = 0;
    if (endLine < 0) endLine = 0;
    if (startLine >= totalLines) startLine = totalLines - 1;
    if (endLine >= totalLines) endLine = totalLines - 1;
    if (startLine > endLine) std::swap(startLine, endLine);

    std::string targetStr = Utils::trim(args);
    if (targetStr.empty()) {
        Utils::setStatus(TEXT("E471: Argument required"));
        return;
    }

    int targetLine = 0; // 0-based target line index

    if (targetStr == "0" || targetStr == "1") {
        targetLine = 0;
    } else if (targetStr == "$") {
        targetLine = totalLines - 1;
    } else if (targetStr == ".") {
        targetLine = currentLine;
    } else if (targetStr[0] == '+' || targetStr[0] == '-') {
        int off = 1;
        if (targetStr.size() > 1) {
            try { off = std::stoi(targetStr.substr(1)); } catch (...) { off = 1; }
        }
        if (targetStr[0] == '+') targetLine = currentLine + off;
        else targetLine = currentLine - off;
    } else if (targetStr[0] == '\'') {
        if (targetStr.size() > 1) {
            int mLine = Marks::getMarkLine(targetStr[1]);
            if (mLine >= 0) targetLine = mLine;
            else targetLine = currentLine;
        }
    } else {
        try {
            int num = std::stoi(targetStr);
            if (num <= 1) {
                targetLine = 0;
            } else {
                targetLine = num - 1;
            }
        } catch (...) {
            Utils::setStatus(TEXT("E14: Invalid address"));
            return;
        }
    }

    if (targetLine < 0) targetLine = 0;
    if (targetLine >= totalLines) targetLine = totalLines - 1;

    if (targetLine >= startLine && targetLine <= endLine) {
        Utils::setStatus(TEXT("Lines already at target"));
        return;
    }

    int posStart = Utils::lineStart(hwndEdit, startLine);
    auto endRange = Utils::lineRange(hwndEdit, endLine, true);
    int posEnd = endRange.second;
    std::string text = Utils::getTextRange(hwndEdit, posStart, posEnd);

    int eolMode = (int)::SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
    std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");
    if (text.empty() || (text.back() != '\n' && text.back() != '\r')) {
        text += eolStr;
    }

    int lineCount = endLine - startLine + 1;

    Utils::beginUndo(hwndEdit);

    // Delete the source range
    Utils::select(hwndEdit, posStart, posEnd);
    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)"");

    int newTotal = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int insertPos = 0;
    int destLine = 0;

    if (startLine > targetLine) {
        // Moving UPWARDS (e.g. from line 5 to line 2):
        // Lines above startLine did not move, so targetLine in new document is targetLine
        destLine = targetLine;
        insertPos = Utils::lineStart(hwndEdit, destLine);
    } else {
        // Moving DOWNWARDS (e.g. from line 1 to line 3):
        // Lines between startLine and targetLine shifted UP by lineCount
        int insertIdx = targetLine - lineCount + 1;
        if (insertIdx >= newTotal) {
            insertPos = (int)::SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
            if (insertPos > 0) {
                char lastCh = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, insertPos - 1, 0);
                if (lastCh != '\n' && lastCh != '\r') {
                    text = eolStr + text;
                }
            }
            destLine = (std::max)(0, newTotal);
        } else {
            destLine = insertIdx;
            insertPos = Utils::lineStart(hwndEdit, destLine);
        }
    }

    ::SendMessage(hwndEdit, SCI_SETSEL, insertPos, insertPos);
    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)text.c_str());

    // Move caret to start of moved block
    int newCaret = Utils::lineStart(hwndEdit, destLine);
    if (newCaret >= 0) {
        ::SendMessage(hwndEdit, SCI_GOTOPOS, newCaret, 0);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
    }

    Utils::endUndo(hwndEdit);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    Utils::setStatus((std::to_wstring(lineCount) + L" line" + (lineCount > 1 ? L"s" : L"") + L" moved").c_str());
}

void CommandMode::executeCopy(HWND hwndEdit, const ExRange& range, const std::string& args) {
    if (!hwndEdit) return;

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    if (totalLines <= 0) return;

    int currentLine = Utils::caretLine(hwndEdit);
    int startLine = range.hasRange ? range.startLine : currentLine;
    int endLine = range.hasRange ? range.endLine : currentLine;

    if (startLine < 0) startLine = 0;
    if (endLine < 0) endLine = 0;
    if (startLine >= totalLines) startLine = totalLines - 1;
    if (endLine >= totalLines) endLine = totalLines - 1;
    if (startLine > endLine) std::swap(startLine, endLine);

    std::string targetStr = Utils::trim(args);
    if (targetStr.empty()) {
        targetStr = ".";
    }

    int targetLine = 0;

    if (targetStr == "0" || targetStr == "1") {
        targetLine = 0;
    } else if (targetStr == "$") {
        targetLine = totalLines - 1;
    } else if (targetStr == ".") {
        targetLine = currentLine;
    } else if (targetStr[0] == '+' || targetStr[0] == '-') {
        int off = 1;
        if (targetStr.size() > 1) {
            try { off = std::stoi(targetStr.substr(1)); } catch (...) { off = 1; }
        }
        if (targetStr[0] == '+') targetLine = currentLine + off;
        else targetLine = currentLine - off;
    } else if (targetStr[0] == '\'') {
        if (targetStr.size() > 1) {
            int mLine = Marks::getMarkLine(targetStr[1]);
            if (mLine >= 0) targetLine = mLine;
            else targetLine = currentLine;
        }
    } else {
        try {
            int num = std::stoi(targetStr);
            if (num <= 1) {
                targetLine = 0;
            } else {
                targetLine = num - 1;
            }
        } catch (...) {
            Utils::setStatus(TEXT("E14: Invalid address"));
            return;
        }
    }

    if (targetLine < 0) targetLine = 0;
    if (targetLine >= totalLines) targetLine = totalLines - 1;

    int posStart = Utils::lineStart(hwndEdit, startLine);
    auto endRange = Utils::lineRange(hwndEdit, endLine, true);
    int posEnd = endRange.second;
    std::string text = Utils::getTextRange(hwndEdit, posStart, posEnd);

    int eolMode = (int)::SendMessage(hwndEdit, SCI_GETEOLMODE, 0, 0);
    std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");
    if (text.empty() || (text.back() != '\n' && text.back() != '\r')) {
        text += eolStr;
    }

    int lineCount = endLine - startLine + 1;
    int insertPos = 0;
    int destLine = 0;

    if (targetStr == "$" || targetLine >= totalLines - 1) {
        insertPos = (int)::SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
        if (insertPos > 0) {
            char lastCh = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, insertPos - 1, 0);
            if (lastCh != '\n' && lastCh != '\r') {
                text = eolStr + text;
            }
        }
        destLine = totalLines;
    } else {
        destLine = targetLine;
        insertPos = Utils::lineStart(hwndEdit, destLine);
    }

    Utils::beginUndo(hwndEdit);
    ::SendMessage(hwndEdit, SCI_SETSEL, insertPos, insertPos);
    ::SendMessage(hwndEdit, SCI_REPLACESEL, 0, (LPARAM)text.c_str());

    int newCaret = Utils::lineStart(hwndEdit, destLine);
    if (newCaret >= 0) {
        ::SendMessage(hwndEdit, SCI_GOTOPOS, newCaret, 0);
        ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
    }
    Utils::endUndo(hwndEdit);

    if (state.mode == VISUAL && g_normalMode) {
        g_normalMode->enter();
    }

    Utils::setStatus((std::to_wstring(lineCount) + L" line" + (lineCount > 1 ? L"s" : L"") + L" copied").c_str());
}

void CommandMode::executePrint(HWND hwndEdit, const ExRange& range) {
    if (!hwndEdit) return;
    int startLine = range.hasRange ? range.startLine : Utils::caretLine(hwndEdit);
    int endLine = range.hasRange ? range.endLine : startLine;

    int posStart = Utils::lineStart(hwndEdit, startLine);
    int posEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);
    std::string text = Utils::getTextRange(hwndEdit, posStart, posEnd);

    if (startLine == endLine) {
        std::wstring lineMsg = std::to_wstring(startLine + 1) + L": " + std::wstring(text.begin(), text.end());
        Utils::setStatus(lineMsg.c_str());
    } else {
        std::wstring lineMsg = L"Lines " + std::to_wstring(startLine + 1) + L"-" + std::to_wstring(endLine + 1) + L": " + std::to_wstring(endLine - startLine + 1) + L" lines";
        Utils::setStatus(lineMsg.c_str());
    }
}

void CommandMode::showOutputBuffer(const std::string& title, const std::string& output, int exitCode) {
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    HWND h = Utils::getCurrentScintillaHandle();
    if (!h) return;

    std::string text = "# Command: " + title + "\n# Exit Code: " + std::to_string(exitCode) + "\n\n" + output;

    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);
    ::SendMessage(h, SCI_SETTEXT, 0, (LPARAM)text.c_str());
    ::SendMessage(h, SCI_SETSAVEPOINT, 0, 0);
    ::SendMessage(h, SCI_SETREADONLY, TRUE, 0);
    ::SendMessage(h, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Consolas");
    ::SendMessage(h, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
    ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, 0, 0);
}