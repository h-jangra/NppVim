#include "../include/CommandMode.h"
#include <shlwapi.h>
#include <fstream>
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../include/Keymap.h"
#include "../include/NppVim.h"
#include "../include/Marks.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/PluginInterface.h"
#include "../plugin/menuCmdID.h"
#include <sstream>
#include <algorithm>
#include <vector>

#include <unordered_map>

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
#include "../include/MappingManager.h"
#include "../include/RcParser.h"

void CommandMode::handleColonCommand(HWND hwndEdit, const std::string &cmd) {
  if (cmd.empty()) return;

  std::string fullCmd = cmd;
  std::stringstream ss(fullCmd);
  std::string baseCmd;
  ss >> baseCmd;

  // Resolve user-defined alias first
  baseCmd = resolveUserCommand(baseCmd);

  if (baseCmd == "set") {
      std::string args;
      std::getline(ss, args);
      args = Utils::trim(args);
      if (args.empty()) {
          // Display all options
          std::string help = "Options:\n========\n";
          auto options = OptionRegistry::getInstance().getAllOptions();
          for (const auto& opt : options) {
              help += opt.name + " = ";
              if (opt.type == OptionType::Bool) help += (std::get<bool>(opt.value) ? "on" : "off");
              else if (opt.type == OptionType::Number) help += std::to_string(std::get<int>(opt.value));
              else help += std::get<std::string>(opt.value);
              help += "\n";
          }
          // Show in a new buffer or status
          Utils::setStatus(TEXT("Options listed in Help (use :h for now)")); // Temporary
      } else {
          if (!OptionRegistry::getInstance().setOptionFromString(args)) {
              Utils::setStatus(TEXT("E518: Unknown option"));
          }
      }
      return;
  }

  if (baseCmd == "map" || baseCmd == "nmap" || baseCmd == "imap" || baseCmd == "vmap" ||
      baseCmd == "noremap" || baseCmd == "nnoremap" || baseCmd == "inoremap" || baseCmd == "vnoremap") {
      std::string args;
      std::getline(ss, args);
      args = Utils::trim(args);
      if (args.empty()) {
          // List mappings
          Utils::setStatus(TEXT("Use :map with args for now"));
      } else {
          RcParser::getInstance().executeLine(fullCmd, hwndEdit);
      }
      return;
  }

  if (baseCmd == "source" || baseCmd == "so") {
      std::string path;
      ss >> path;
      if (!RcParser::getInstance().parseFile(path, hwndEdit)) {
          Utils::setStatus(TEXT("E484: Cannot open file"));
      } else {
          Utils::setStatus(TEXT("Configuration sourced"));
      }
      return;
  }

  if (baseCmd == "NppVimReload") {
      loadConfig();
      Utils::setStatus(TEXT("NppVim reloaded"));
      return;
  }

  if (baseCmd == "version" || baseCmd == "ver") {
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
              }
          }
      }
      return;
  }

  if (baseCmd == "edit" || baseCmd == "e") {
      std::string path;
      std::getline(ss, path);
      path = Utils::trim(path);
      if (path.empty()) {
          // Preserve current behavior: reload current file
          ::SendMessage(nppData._nppHandle, IDM_FILE_RELOAD, 0, 0);
          Utils::setStatus(TEXT("File reloaded"));
      } else if (path == "rc" || path == "nppvim.rc" || path == ".nppvimrc") {
          ConfigManager::getInstance().editRc();
      } else if (path == "ini" || path == "config.ini") {
          ConfigManager::getInstance().editIni();
      } else {
          // Support :edit <filename>
          int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
          if (wideLen > 0) {
              std::vector<wchar_t> pathWide(wideLen);
              MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, pathWide.data(), wideLen);

              wchar_t currentFile[MAX_PATH] = {0};
              ::SendMessageW(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)currentFile);

              wchar_t currentDir[MAX_PATH] = {0};
              wcscpy_s(currentDir, currentFile);
              PathRemoveFileSpecW(currentDir);

              wchar_t fullPath[MAX_PATH] = {0};
              if (PathIsRelativeW(pathWide.data())) {
                  PathCombineW(fullPath, currentDir, pathWide.data());
              } else {
                  wcscpy_s(fullPath, pathWide.data());
              }

              if (!PathFileExistsW(fullPath)) {
                  // Create the file
                  HANDLE hFile = CreateFileW(fullPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
                  if (hFile != INVALID_HANDLE_VALUE) {
                      CloseHandle(hFile);
                  }
              }
              ::SendMessageW(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)fullPath);
          }
      }
      return;
  }

  if (baseCmd == "editrc" || baseCmd == "erc" || baseCmd == "rc") {
      ConfigManager::getInstance().editRc();
      return;
  }

  if (baseCmd == "editini" || baseCmd == "eini" || baseCmd == "ini") {
      ConfigManager::getInstance().editIni();
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
      } else {
        Utils::setStatus(TEXT("Line number out of range"));
      }
    }
    return;
  }

   if (cmd.find("sort") == 0) {
        if (cmd == "sort") {
            ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_SORTLINES_LEXICOGRAPHIC_ASCENDING, 0);
            Utils::setStatus(TEXT("Lines sorted"));
        } else if (cmd == "sort!") {
            ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_SORTLINES_LEXICOGRAPHIC_DESCENDING, 0);
            Utils::setStatus(TEXT("Lines sorted (descending)"));
        } else if (cmd == "sort n") {
            ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_SORTLINES_INTEGER_ASCENDING, 0);
            Utils::setStatus(TEXT("Lines sorted (numeric)"));
        } else if (cmd == "sort n!") {
            ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_SORTLINES_INTEGER_DESCENDING, 0);
            Utils::setStatus(TEXT("Lines sorted (numeric descending)"));
        } else {
            Utils::setStatus(TEXT("Use: sort, sort!, sort n, sort n!"));
        }
        return;
    }

  SubstitutionParsed parsedCmd;
  if (parseSubstitutionCommand(cmd, hwndEdit, parsedCmd))
  {
    handleSubstitutionCommand(hwndEdit, cmd);
    return;
  }

  if (cmd == "wrap" || cmd == "wrapmode" || cmd == "wrap on") {
    ::SendMessage(hwndEdit, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
    Utils::setStatus(TEXT("Word wrap enabled"));
    return;
  }
  if (cmd == "nowrap" || cmd == "wrap off") {
      ::SendMessage(hwndEdit, SCI_SETWRAPMODE, SC_WRAP_NONE, 0);
      Utils::setStatus(TEXT("Word wrap disabled"));
      return;
  }
  if (cmd == "wrap char") {
      ::SendMessage(hwndEdit, SCI_SETWRAPMODE, SC_WRAP_CHAR, 0);
      Utils::setStatus(TEXT("Character wrap enabled"));
      return;
  }
  if (cmd == "wrap whitespace") {
      ::SendMessage(hwndEdit, SCI_SETWRAPMODE, SC_WRAP_WHITESPACE, 0);
      Utils::setStatus(TEXT("Whitespace wrap enabled"));
      return;
  }

  if (cmd.rfind("set tw=", 0) == 0) {
    try {
        int width = std::stoi(cmd.substr(7));
        ::SendMessage(hwndEdit, SCI_SETEDGECOLUMN, width, 0);
        ::SendMessage(hwndEdit, SCI_SETEDGEMODE, EDGE_LINE, 0);
        Utils::setStatus(TEXT("Text width set"));
    } catch (...) {
        Utils::setStatus(TEXT("Invalid text width"));
    }
    return;
  }

  for (char c : cmd) {
    if (!g_commandKeymap->handleKey(hwndEdit, c)) {
        break;
    }
  }
  return;

}

auto tutorHandler = [](HWND, int) {
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);

    HWND h = Utils::getCurrentScintillaHandle();
    std::string tutor = Utils::buildTutorText();

    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);
    ::SendMessage(h, SCI_SETTEXT, 0, (LPARAM)tutor.c_str());
    ::SendMessage(h, SCI_SETSAVEPOINT, 0, 0);
    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);

    Utils::setStatus(TEXT("-- TUTOR --"));
};

auto helpHandler = [](HWND, int) {
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);

    HWND h = Utils::getCurrentScintillaHandle();
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

    Utils::setStatus(TEXT("-- HELP --"));
};

CommandMode::CommandMode(VimState &state) : state(state)
{
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
    .set("q", "Close current file", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_FILE_CLOSE, 0);
    })
    .set("qa", "Close all files", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_FILE_CLOSEALL, 0);
    })
    .set("wq", "Save and close file", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, NPPM_SAVECURRENTFILE, 0, 0);
        ::SendMessage(nppData._nppHandle, IDM_FILE_CLOSE, 0, 0);
    })
    .set("wqa", "Save all and close all files", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, NPPM_SAVEALLFILES, 0, 0);
        ::SendMessage(nppData._nppHandle, IDM_FILE_CLOSEALL, 0, 0);
    })
    .set("bn", "Next tab", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_NEXT, 0, 0);
    })
    .set("bp", "Previous tab", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_VIEW_TAB_PREV, 0, 0);
    })
    .set("bd", "Close current tab", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_FILE_CLOSE, 0);
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
    .set("m", "Move line to specific line number", [](HWND h, int c) {
        int currentLine = ::SendMessage(h, SCI_LINEFROMPOSITION, Utils::caretPos(h), 0);
        int targetLine = c > 0 ? c - 1 : 0;
        if (targetLine >= 0 && targetLine != currentLine) {
            ::SendMessage(h, SCI_MOVESELECTEDLINESUP, currentLine < targetLine ? 0 : 1, 0);
            if (currentLine < targetLine) {
                for (int i = currentLine; i < targetLine; ++i) {
                    ::SendMessage(h, SCI_MOVESELECTEDLINESDOWN, 0, 0);
                }
            } else {
                for (int i = currentLine; i > targetLine; --i) {
                    ::SendMessage(h, SCI_MOVESELECTEDLINESUP, 0, 0);
                }
            }
            ::SendMessage(h, SCI_GOTOLINE, targetLine, 0);
        }
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
    .set("h",    "Open command help", helpHandler)
    .set("help", "Open command help", helpHandler)
    .set("tutor", "Open tutor", tutorHandler)
    .set("tut",   "Open tutor", tutorHandler)
    .set("t",     "Open tutor", tutorHandler);
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
    help += ":reload            - Reload nppvim.rc\n";
    help += ":config            - Open settings dialog\n";
    help += ":tutor             - Open interactive tutor\n";
    help += "\nConfiguration\n";
    help += "-------------\n";
    help += "Settings are saved in 'config.ini' and 'nppvim.rc' in the plugin directory.\n";
    help += "Use :set <option> to change settings. Available options:\n";
    help += "  number, relativenumber, hlsearch, ignorecase, smartcase,\n";
    help += "  expandtab, tabstop, shiftwidth, wrap, cursorline, list,\n";
    help += "  scrolloff, keylayout, langmap\n";
    help += "\nExamples:\n";
    help += "  :set number\n";
    help += "  :set tabstop=4\n";
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

void CommandMode::handleMarksCommand(HWND hwndEdit, const std::string &commandLine)
{
  if (commandLine == "marks" || commandLine == "m")
  {
    std::string marksList = Marks::listMarks(hwndEdit);

#ifdef UNICODE
    int len = MultiByteToWideChar(CP_UTF8, 0, marksList.c_str(), -1, NULL, 0);
    if (len > 0)
    {
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

  if (commandLine.find("delm") == 0 || commandLine.find("dm") == 0)
  {
    size_t startPos = (commandLine.length() > 4 && commandLine[4] == ' ') ? 5 : 4;

    if (commandLine.find("!") != std::string::npos || commandLine.find("a") != std::string::npos)
    {
      Marks::clearAllMarks(hwndEdit);
      Utils::setStatus(TEXT("-- All marks deleted --"));
      return;
    }

    if (startPos < commandLine.length())
    {
      std::string marks = commandLine.substr(startPos);
      int deletedCount = 0;

      for (char ch : marks)
      {
        if (ch != ' ' && Marks::isValidMark(ch))
        {
          Marks::deleteMark(hwndEdit, ch);
          deletedCount++;
        }
      }

      if (deletedCount > 0)
      {
        Utils::setStatus(TEXT("-- Marks deleted --"));
      }
      else
      {
        Utils::setStatus(TEXT("-- No valid marks specified --"));
      }
    }
    else
    {
      Utils::setStatus(TEXT("-- Specify marks to delete --"));
    }
    return;
  }

  Utils::setStatus(TEXT("-- Unknown marks command --"));
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

    std::string str = buf;
    if (!str.empty() && str[0] == ':') str = str.substr(1);
    if (str.empty()) return false;

    size_t sPos = std::string::npos;
    size_t cmdLen = 0;

    // Check for "substitute"
    size_t subKeywordPos = str.find("substitute");
    if (subKeywordPos != std::string::npos) {
        bool validBefore = (subKeywordPos == 0 || str[subKeywordPos - 1] == '%' || str[subKeywordPos - 1] == '\'' ||
                            str[subKeywordPos - 1] == '>' || str[subKeywordPos - 1] == ',' ||
                            std::isdigit((unsigned char)str[subKeywordPos - 1]) || str[subKeywordPos - 1] == '.' || str[subKeywordPos - 1] == '$');
        if (validBefore) {
            sPos = subKeywordPos;
            cmdLen = 10;
        }
    }

    if (sPos == std::string::npos) {
        // Find 's'
        for (size_t i = 0; i < str.size(); i++) {
            if (str[i] == 's') {
                if (i + 1 < str.size() && !std::isalnum((unsigned char)str[i + 1])) {
                    sPos = i;
                    cmdLen = 1;
                    break;
                } else if (i + 1 == str.size()) {
                    sPos = i;
                    cmdLen = 1;
                    break;
                }
            }
        }
    }

    if (sPos == std::string::npos) return false;

    res.isSubstitution = true;
    res.rangeStr = str.substr(0, sPos);

    int totalLines = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    int currentPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, currentPos, 0);

    if (res.rangeStr == "%") {
        res.startLine = 0;
        res.endLine = (totalLines > 0) ? totalLines - 1 : 0;
    } else if (res.rangeStr == "'<,'>" || (res.rangeStr.empty() && state.mode == VISUAL)) {
        int selStart = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
        int selEnd = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
        res.startLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, selStart, 0);
        res.endLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, selEnd, 0);
        if (res.startLine > res.endLine) std::swap(res.startLine, res.endLine);
    } else if (res.rangeStr.empty() || res.rangeStr == ".") {
        res.startLine = currentLine;
        res.endLine = currentLine;
    } else if (res.rangeStr.find(',') != std::string::npos) {
        size_t comma = res.rangeStr.find(',');
        std::string startPart = res.rangeStr.substr(0, comma);
        std::string endPart = res.rangeStr.substr(comma + 1);

        auto parseLineStr = [&](const std::string& p) -> int {
            if (p.empty() || p == ".") return currentLine;
            if (p == "$") return totalLines - 1;
            if (p == "'<") {
                int s = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
                return (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, s, 0);
            }
            if (p == "'>") {
                int e = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
                return (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, e, 0);
            }
            try {
                if (p[0] == '.' && p.size() > 1) return currentLine + std::stoi(p.substr(1));
                if (p[0] == '$' && p.size() > 1) return (totalLines - 1) + std::stoi(p.substr(1));
                return std::stoi(p) - 1;
            } catch (...) {
                return currentLine;
            }
        };

        res.startLine = parseLineStr(startPart);
        res.endLine = parseLineStr(endPart);
    } else {
        try {
            if (res.rangeStr == "$") res.startLine = res.endLine = totalLines - 1;
            else res.startLine = res.endLine = std::stoi(res.rangeStr) - 1;
        } catch (...) {
            res.startLine = res.endLine = currentLine;
        }
    }

    if (res.startLine < 0) res.startLine = 0;
    if (res.endLine >= totalLines) res.endLine = (totalLines > 0) ? totalLines - 1 : 0;
    if (res.startLine > res.endLine) std::swap(res.startLine, res.endLine);

    size_t parseIdx = sPos + cmdLen;
    if (parseIdx >= str.size()) return true;

    res.delimiter = str[parseIdx++];

    // Extract pattern
    bool escaped = false;
    while (parseIdx < str.size()) {
        char ch = str[parseIdx++];
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
    while (parseIdx < str.size()) {
        char ch = str[parseIdx++];
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
    while (parseIdx < str.size()) {
        res.flags += str[parseIdx++];
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
    } else {
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

void CommandMode::showRegisters() {
    HWND h = Utils::getCurrentScintillaHandle();
    if (!h) return;
    
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    
    std::string registersText;
    registersText += "NppVim Registers\n";
    registersText += "════════════════\n\n";
    
    auto getPreview = [](const std::string& content, int maxLength = 40) -> std::string {
        if (content.empty()) return "";
        
        std::string preview;
        int charCount = 0;
        bool truncated = false;
        
        for (size_t i = 0; i < content.size() && charCount < maxLength; i++) {
            char ch = content[i];
            
            if (ch == '\n' || ch == '\r') {
                if (charCount + 4 < maxLength) {
                    preview += "\\n";
                    charCount += 2;
                } else {
                    truncated = true;
                    break;
                }
                
                // Skip \r\n pairs
                if (ch == '\r' && i + 1 < content.size() && content[i + 1] == '\n') {
                    i++;
                }
            } else if (ch == '\t') {
                if (charCount + 2 < maxLength) {
                    preview += "\\t";
                    charCount += 2;
                } else {
                    truncated = true;
                    break;
                }
            } else if (ch >= 32 && ch <= 126) {
                preview += ch;
                charCount++;
            } else {
                if (charCount + 4 < maxLength) {
                    char hex[5];
                    sprintf_s(hex, "\\x%02X", (unsigned char)ch);
                    preview += hex;
                    charCount += 4;
                } else {
                    truncated = true;
                    break;
                }
            }
        }
        
        if (truncated || charCount >= maxLength) {
            preview += "...";
        }
        
        return preview;
    };
    
    auto countLines = [](const std::string& content) -> int {
        int lines = 0;
        for (char ch : content) {
            if (ch == '\n') lines++;
        }
        return lines + (content.empty() ? 0 : 1);
    };
    
    // Named registers table
    registersText += "Named registers:\n";
    registersText += "Type Name  Preview                              Lines\n";
    registersText += "──── ───── ──────────────────────────────────── ─────\n";
    
    for (char reg = 'a'; reg <= 'z'; reg++) {
        std::string content = Utils::getRegisterContent(reg);
        if (!content.empty()) {
            std::string preview = getPreview(content, 35);
            int lines = countLines(content);
            
            char line[80];
            sprintf_s(line, "char  \"%c   %-35s %4d\n", 
                     reg, preview.c_str(), lines);
            registersText += line;
        }
    }
    
    registersText += "\n";
    
    // Numbered registers table
    registersText += "Numbered registers:\n";
    registersText += "Type Name  Preview                              Lines\n";
    registersText += "──── ───── ──────────────────────────────────── ─────\n";
    
    for (char reg = '0'; reg <= '9'; reg++) {
        std::string content = Utils::getRegisterContent(reg);
        if (!content.empty()) {
            std::string preview = getPreview(content, 35);
            int lines = countLines(content);
            
            char line[80];
            sprintf_s(line, "char  \"%c   %-35s %4d\n", 
                     reg, preview.c_str(), lines);
            registersText += line;
        }
    }
    
    registersText += "\n";
    
    // Special registers
    registersText += "Special registers:\n";
    registersText += "Type Name  Description                         Content\n";
    registersText += "──── ───── ─────────────────────────────────── ───────\n";
    
    // System clipboard
    std::string clipboardPreview;
    if (IsClipboardFormatAvailable(CF_TEXT)) {
        if (OpenClipboard(NULL)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pszText = (char*)GlobalLock(hData);
                if (pszText) {
                    clipboardPreview = getPreview(pszText, 30);
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
        }
    }
    
    char line[80];
    sprintf_s(line, "sys   \"+   System clipboard               %s\n", 
             clipboardPreview.empty() ? "(empty)" : clipboardPreview.c_str());
    registersText += line;
    
    sprintf_s(line, "sys   \"*   System clipboard (selection)   %s\n", 
             clipboardPreview.empty() ? "(empty)" : clipboardPreview.c_str());
    registersText += line;
    
    // Last search
    if (!state.lastSearchTerm.empty()) {
        sprintf_s(line, "spec  \"/   Last search pattern           \"%s\"\n", 
                 getPreview(state.lastSearchTerm, 30).c_str());
        registersText += line;
    } else {
        registersText += "spec  \"/   Last search pattern           (none)\n";
    }
    
    // Black hole
    std::string blackhole = Utils::getRegisterContent('_');
    if (!blackhole.empty()) {
        sprintf_s(line, "spec  \"_   Black hole register           %s\n", 
                 getPreview(blackhole, 30).c_str());
        registersText += line;
    } else {
        registersText += "spec  \"_   Black hole register           (empty)\n";
    }
    
    // Set the text in the new buffer
    ::SendMessage(h, SCI_SETREADONLY, FALSE, 0);
    ::SendMessage(h, SCI_SETTEXT, 0, (LPARAM)registersText.c_str());
    ::SendMessage(h, SCI_SETSAVEPOINT, 0, 0);
    ::SendMessage(h, SCI_SETREADONLY, TRUE, 0);
    
    // Use monospace font for table alignment
    ::SendMessage(h, SCI_STYLESETFONT, STYLE_DEFAULT, (LPARAM)"Consolas");
    ::SendMessage(h, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
    
    ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, 0, 0);
    
    Utils::setStatus(TEXT("-- REGISTERS --"));
}