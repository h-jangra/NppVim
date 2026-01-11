#include "../include/CommandMode.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../include/Keymap.h"
#include "../include/NppVim.h"
#include "../include/Marks.h"
#include "../include/Utils.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/PluginInterface.h"
#include "../plugin/menuCmdID.h"

extern NormalMode *g_normalMode;
extern NppData nppData;

static void appendNonKeymapHelp(std::string& help);

void CommandMode::enter(char prompt) {
  state.commandMode = true;
  state.commandBuffer.clear();
  state.commandBuffer.push_back(prompt);

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
    const int totalWidth = 60;
    int commandLength = static_cast<int>(display.length());

    std::wstring matchInfo;
    if (state.lastSearchMatchCount > 0) {
      matchInfo = L"[" + std::to_wstring(state.lastSearchMatchCount) + L" matches]";
    } else {
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
    state.commandBuffer.pop_back();
    updateStatus();

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

    previewSubstitutionFromBuffer(hwndEdit);
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

  const std::string &buf = state.commandBuffer;
  char firstChar = buf[0];

  try {
    if (firstChar == '/') {
      if (buf.size() > 1) {
        handleSearchCommand(hwndEdit, buf.substr(1));
      } else {
        Utils::setStatus(TEXT("No search pattern"));
      }
    } else if (firstChar == '?') {
      if (buf.size() > 1) {
        handleSearchCommand(hwndEdit, buf.substr(1), true);
      } else {
        Utils::setStatus(TEXT("No regex pattern"));
      }
    } else if (firstChar == ':') {
      if (buf.size() > 1) {
        handleColonCommand(hwndEdit, buf.substr(1));
      } else {
        exit();
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

void CommandMode::handleSearchCommand(HWND hwndEdit, const std::string &searchTerm, bool useRegex) {
  performSearch(hwndEdit, searchTerm, useRegex);
}

void CommandMode::handleColonCommand(HWND hwndEdit, const std::string &cmd) {
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

  if ((cmd.size() >= 2 && cmd[0] == 's' && !std::isalnum(cmd[1])) || 
      (cmd.size() >= 3 && cmd[0] == '%' && cmd[1] == 's' && !std::isalnum(cmd[2])) )
  {
    handleSubstitutionCommand(hwndEdit, cmd);
    return;
  }

  if (cmd == "wrap" || cmd == "wrapmode" || cmd == "wrap on") {
    ::SendMessage(hwndEdit, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
    ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_WRAP);
    Utils::setStatus(TEXT("Word wrap enabled"));
    return;
  }
  if (cmd == "nowrap" || cmd == "wrap off") {
      ::SendMessage(hwndEdit, SCI_SETWRAPMODE, SC_WRAP_NONE, 0);
      ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_WRAP);
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
  g_commandKeymap->reset();
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
    std::string help =
        "NppVim — Command Mode Help\n"
        "=========================\n\n";

    const size_t CMD_COL = 18;

    for (const auto& b : g_commandKeymap->getBindings()) {
        std::string cmd = ":" + b.keys;

        help += cmd;
        if (cmd.size() < CMD_COL)
            help += std::string(CMD_COL - cmd.size(), ' ');

        help += " - ";
        help += b.desc;
        help += "\n";
    }

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
        ::SendMessage(nppData._nppHandle, IDM_FILE_CLOSE, 0, 0);
    })
    .set("qa", "Close all files", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_FILE_CLOSEALL, 0, 0);
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
        ::SendMessage(nppData._nppHandle, IDM_FILE_CLOSE, 0, 0);
    })
    .set("sp", "Split window", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_VIEW_CLONE_TO_ANOTHER_VIEW, 0, 0);
        Utils::setStatus(TEXT("Split window"));
    })
    .set("vs", "Vertical split", [](HWND, int) {
        ::SendMessage(nppData._nppHandle, IDM_VIEW_CLONE_TO_ANOTHER_VIEW, 0, 0);
        Utils::setStatus(TEXT("Vertical split"));
    })
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
        ::SendMessage(hwnd, SCI_SETMARGINWIDTHN, 0, 50);
        Utils::setStatus(TEXT("Line numbers enabled"));
    })
    .set("set number", "Enable line numbers", [](HWND hwnd, int) {
        ::SendMessage(hwnd, SCI_SETMARGINWIDTHN, 0, 50);
        Utils::setStatus(TEXT("Line numbers enabled"));
    })
    .set("set nonu", "Disable line numbers", [](HWND hwnd, int) {
        ::SendMessage(hwnd, SCI_SETMARGINWIDTHN, 0, 0);
        Utils::setStatus(TEXT("Line numbers disabled"));
    })
    .set("set nonumber", "Disable line numbers", [](HWND hwnd, int) {
        ::SendMessage(hwnd, SCI_SETMARGINWIDTHN, 0, 0);
        Utils::setStatus(TEXT("Line numbers disabled"));
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
    .set("h",    "Open command help", helpHandler)
    .set("help", "Open command help", helpHandler)
    .set("tutor", "Open tutor", tutorHandler)
    .set("tut",   "Open tutor", tutorHandler)
    .set("t",     "Open tutor", tutorHandler);
}

static void appendNonKeymapHelp(std::string& help) {
    help += "\nOther Commands\n";
    help += "--------------\n";
    help += ":<number>          - Jump to line number\n";
    help += ":q!                - Force close file\n";
    help += ":quit!             - Force close file\n";
    help += ":marks, :m         - Show marks\n";
    help += ":delm, :dm         - Delete marks\n";
    help += ":sort              - Sort lines (A→Z)\n";
    help += ":sort!             - Sort lines (Z→A)\n";
    help += ":sort n            - Numeric sort\n";
    help += ":sort n!           - Numeric sort (desc)\n";
    help += ":s/old/new/        - Substitute\n";
    help += ":set tw=NN         - Set text width\n";
    help += ":wrap              - Enable word wrap\n";
    help += ":nowrap            - Disable wrap\n";
    help += ":wrap char         - Character wrap\n";
    help += ":wrap whitespace   - Whitespace wrap\n";
    help += "/pattern           - Search forward\n";
    help += "?pattern           - Search backward\n";
}

void CommandMode::handleSubstitutionCommand(HWND hwndEdit, const std::string &cmd) {
  clearSubstitutionPreview(hwndEdit);

  std::string command = cmd;
  bool globalReplace = false;
  int startPos = 0;
  int endPos = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
  int startLine = 0;
  int endLine = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0) - 1;

  if (command[0] == '%')
  {
    globalReplace = true;
    command = command.substr(1);
  }

  size_t commaPos = command.find(',');
  size_t sPos = command.find('s');

  if (commaPos != std::string::npos && commaPos < sPos)
  {
    std::string range = command.substr(0, sPos);
    command = command.substr(sPos);

    size_t commaPos2 = range.find(',');
    if (commaPos2 != std::string::npos)
    {
      try
      {
        std::string startStr = range.substr(0, commaPos2);
        if (startStr == "." || startStr.empty())
        {
          startLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION,
                                        (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), 0);
        }
        else if (startStr == "$")
        {
          startLine = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0) - 1;
        }
        else
        {
          startLine = std::stoi(startStr) - 1;
        }

        std::string endStr = range.substr(commaPos2 + 1);
        if (endStr == "." || endStr.empty())
        {
          endLine = startLine;
        }
        else if (endStr == "$")
        {
          endLine = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0) - 1;
        }
        else
        {
          endLine = std::stoi(endStr) - 1;
        }

        if (startLine < 0) startLine = 0;
        if (endLine >= (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0))
          endLine = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0) - 1;
        if (startLine > endLine) std::swap(startLine, endLine);

        startPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        endPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0);
        if (endPos < 0) endPos = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

        globalReplace = true;
      }
      catch (...)
      {
        Utils::setStatus(TEXT("Invalid range"));
        return;
      }
    }
  }

  if (command.size() < 4 || command[0] != 's')
  {
    Utils::setStatus(TEXT("Invalid substitution command"));
    return;
  }

  char delimiter = command[1];
  size_t patternStart = 2;
  size_t patternEnd = command.find(delimiter, patternStart);

  if (patternEnd == std::string::npos)
  {
    Utils::setStatus(TEXT("Missing pattern delimiter"));
    return;
  }

  std::string pattern = command.substr(patternStart, patternEnd - patternStart);
  if (pattern.empty())
  {
    Utils::setStatus(TEXT("Empty pattern"));
    return;
  }

  size_t replacementStart = patternEnd + 1;
  size_t replacementEnd = command.find(delimiter, replacementStart);

  if (replacementEnd == std::string::npos)
  {
    replacementEnd = command.length();
  }

  std::string replacement = command.substr(replacementStart, replacementEnd - replacementStart);

  bool caseInsensitive = false;
  bool confirmEach = false;
  bool replaceAll = false;
  bool useRegex = true;

  if (replacementEnd < command.length())
  {
    std::string flags = command.substr(replacementEnd + 1);
    for (char flag : flags)
    {
      switch (flag)
      {
        case 'i': caseInsensitive = true; break;
        case 'I': caseInsensitive = true; break;
        case 'c': confirmEach = true; break;
        case 'g': replaceAll = true; break;
        case 'l': useRegex = false; break;
        default: break;
      }
    }
  }

  performSubstitution(hwndEdit, pattern, replacement, useRegex, caseInsensitive,
                     replaceAll, confirmEach, globalReplace, startPos, endPos);
}

void CommandMode::performSubstitution(HWND hwndEdit, const std::string &pattern,
                                     const std::string &replacement, bool useRegex,
                                     bool caseInsensitive, bool replaceAll,
                                     bool confirmEach, bool globalReplace,
                                     int startPos, int endPos)
{
   Utils::beginUndo(hwndEdit);

    if (pattern.empty())
    {
      Utils::setStatus(TEXT("Empty pattern"));
      Utils::endUndo(hwndEdit);
      return;
    }

    int flags = 0;
    if (useRegex) flags |= SCFIND_REGEXP;
    if (!caseInsensitive) flags |= SCFIND_MATCHCASE;

    ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

    int originalPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int originalAnchor = (int)::SendMessage(hwndEdit, SCI_GETANCHOR, 0, 0);
    int originalLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, originalPos, 0);

    int searchStart;
    int searchEnd;

    if (globalReplace) {
        searchStart = startPos;
        searchEnd = endPos;
    } else {
        int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, originalLine, 0);
        int lineEnd   = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, originalLine, 0);
        searchStart = lineStart;
        searchEnd = lineEnd;
    }

    int replacements = 0;
    int skipped = 0;

    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, searchStart, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);

    if (confirmEach)
    {
      ::SendMessage(hwndEdit, SCI_SETTARGETSTART, searchStart, 0);
      ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);

      while (true)
      {
          int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                        (WPARAM)pattern.length(),
                                        (LPARAM)pattern.c_str());

          if (found == -1)
              break;

          int matchStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
          int matchEnd   = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

          ::SendMessage(hwndEdit, SCI_SETSEL, matchStart, matchEnd);
          ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

          std::wstring prompt = L"Replace with \"" +
                                std::wstring(replacement.begin(), replacement.end()) +
                                L"\"? (y/n/a/q)";
          Utils::setStatus(prompt.c_str());

          char response = (char)Utils::getCharBlocking();

          if (response == 'y' || response == 'Y')
          {
              ::SendMessage(hwndEdit,
                  useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                  replacement.length(),
                  (LPARAM)replacement.c_str());

              replacements++;

              int diff = (int)replacement.length() - (matchEnd - matchStart);
              searchEnd += diff;
              matchEnd += diff;

              ::SendMessage(hwndEdit, SCI_SETTARGETSTART, matchEnd, 0);
              ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);
          }
          else if (response == 'n' || response == 'N')
          {
              skipped++;
              ::SendMessage(hwndEdit, SCI_SETTARGETSTART, matchEnd, 0);
              ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);
          }
          else if (response == 'a' || response == 'A')
          {
              ::SendMessage(hwndEdit,
                  useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                  replacement.length(),
                  (LPARAM)replacement.c_str());

              replacements++;

              int diff = (int)replacement.length() - (matchEnd - matchStart);
              searchEnd += diff;
              matchEnd += diff;

              ::SendMessage(hwndEdit, SCI_SETTARGETSTART, matchEnd, 0);
              ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);

              found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                        (WPARAM)pattern.length(), (LPARAM)pattern.c_str());

              while (found != -1)
              {
                matchStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
                matchEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

                ::SendMessage(hwndEdit, useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                            replacement.length(), (LPARAM)replacement.c_str());
                replacements++;

                int newLength = (int)replacement.length();
                int oldLength = matchEnd - matchStart;
                int lengthDiff = newLength - oldLength;

                searchEnd += lengthDiff;
                matchEnd += lengthDiff;

                ::SendMessage(hwndEdit, SCI_SETTARGETSTART, matchEnd, 0);
                ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);

                found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                          (WPARAM)pattern.length(), (LPARAM)pattern.c_str());
              }
              break;
          }
          else if (response == 'q' || response == 'Q')
          {
              break;
          }
      }
    }
    else
    {
      if (replaceAll)
      {
        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                      (WPARAM)pattern.length(), (LPARAM)pattern.c_str());

        while (found != -1)
        {
          int matchStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
          int matchEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);

          ::SendMessage(hwndEdit, useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                      replacement.length(), (LPARAM)replacement.c_str());
          replacements++;

          int newLength = (int)replacement.length();
          int oldLength = matchEnd - matchStart;
          int lengthDiff = newLength - oldLength;

          searchEnd += lengthDiff;
          matchEnd += lengthDiff;

          ::SendMessage(hwndEdit, SCI_SETTARGETSTART, matchEnd, 0);
          ::SendMessage(hwndEdit, SCI_SETTARGETEND, searchEnd, 0);

          found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                    (WPARAM)pattern.length(), (LPARAM)pattern.c_str());
        }
      }
      else
      {
        int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                      (WPARAM)pattern.length(), (LPARAM)pattern.c_str());

        if (found != -1)
        {
          ::SendMessage(hwndEdit, useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                      replacement.length(), (LPARAM)replacement.c_str());
          replacements++;

          int matchEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
          ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, matchEnd, 0);
          ::SendMessage(hwndEdit, SCI_SETSEL, matchEnd, matchEnd);
        }
        else
        {
          ::SendMessage(hwndEdit, SCI_SETTARGETSTART, 0, 0);
          ::SendMessage(hwndEdit, SCI_SETTARGETEND, originalPos, 0);

          found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                    (WPARAM)pattern.length(), (LPARAM)pattern.c_str());

          if (found != -1)
          {
            ::SendMessage(hwndEdit, useRegex ? SCI_REPLACETARGETRE : SCI_REPLACETARGET,
                        replacement.length(), (LPARAM)replacement.c_str());
            replacements++;

            int matchEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, matchEnd, 0);
            ::SendMessage(hwndEdit, SCI_SETSEL, matchEnd, matchEnd);
            Utils::setStatus(TEXT("Search wrapped to top"));
          }
        }
      }
    }

  if (replacements == 0 && !globalReplace)
  {
    ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, originalPos, 0);
    ::SendMessage(hwndEdit, SCI_SETANCHOR, originalAnchor, 0);
  }

  Utils::endUndo(hwndEdit);

  if (replacements > 0)
  {
    std::wstring msg = std::to_wstring(replacements) + L" replacement" +
                      (replacements > 1 ? L"s" : L"") + L" made";
    if (skipped > 0)
    {
      msg += L", " + std::to_wstring(skipped) + L" skipped";
    }
    Utils::setStatus(msg.c_str());
  }
  else
  {
    Utils::setStatus(TEXT("Pattern not found"));
  }
}

void CommandMode::performSearch(HWND hwndEdit, const std::string &searchTerm, bool useRegex)
{
  if (searchTerm.empty())
  {
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
    Utils::showCurrentMatchPosition(hwndEdit, searchTerm, useRegex);
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

  int flags = (state.useRegex ? SCFIND_REGEXP : 0);
  ::SendMessage(hwndEdit, SCI_SETSEARCHFLAGS, flags, 0);

  ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
  ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

  int found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                 (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

  if (found == -1)
  {
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
    Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
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

  while (found != -1)
  {
    lastFound = found;
    lastStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
    lastEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lastEnd, 0);
    found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                               (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());
  }

  if (lastFound != -1)
  {
    ::SendMessage(hwndEdit, SCI_SETSEL, lastStart, lastEnd);
    ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
    Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
  }
  else
  {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(hwndEdit, SCI_SETTARGETEND, docLen, 0);

    found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                               (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());

    lastFound = -1;
    while (found != -1)
    {
      lastFound = found;
      lastStart = (int)::SendMessage(hwndEdit, SCI_GETTARGETSTART, 0, 0);
      lastEnd = (int)::SendMessage(hwndEdit, SCI_GETTARGETEND, 0, 0);
      ::SendMessage(hwndEdit, SCI_SETTARGETSTART, lastEnd, 0);
      found = (int)::SendMessage(hwndEdit, SCI_SEARCHINTARGET,
                                 (WPARAM)state.lastSearchTerm.length(), (LPARAM)state.lastSearchTerm.c_str());
    }

    if (lastFound != -1)
    {
      ::SendMessage(hwndEdit, SCI_SETSEL, lastStart, lastEnd);
      ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);
      Utils::setStatus(TEXT("Search wrapped to bottom"));
      Utils::showCurrentMatchPosition(hwndEdit, state.lastSearchTerm, state.useRegex);
    }
    else
    {
      Utils::setStatus(TEXT("Pattern not found"));
    }
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
    ::SendMessage(h, SCI_INDICSETSTYLE, IND_SUB_MATCH, INDIC_ROUNDBOX);
    ::SendMessage(h, SCI_INDICSETFORE, IND_SUB_MATCH, RGB(255, 180, 0));

    ::SendMessage(h, SCI_INDICSETSTYLE, IND_SUB_REPL, INDIC_STRAIGHTBOX);
    ::SendMessage(h, SCI_INDICSETFORE, IND_SUB_REPL, RGB(0, 200, 120));
}

void CommandMode::clearSubstitutionPreview(HWND h) {
    int firstLine = (int)::SendMessage(h, SCI_GETFIRSTVISIBLELINE, 0, 0);
    int linesOnScreen = (int)::SendMessage(h, SCI_LINESONSCREEN, 0, 0);

    int start = (int)::SendMessage(h, SCI_POSITIONFROMLINE, firstLine, 0);
    int end = (int)::SendMessage(
        h,
        SCI_POSITIONFROMLINE,
        firstLine + linesOnScreen + 1,
        0
    );

    ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_MATCH, 0);
    ::SendMessage(h, SCI_INDICATORCLEARRANGE, start, end - start);

    ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_REPL, 0);
    ::SendMessage(h, SCI_INDICATORCLEARRANGE, start, end - start);
}

void CommandMode::previewSubstitution(
    HWND h,
    const std::string& pat,
    const std::string& rep,
    bool regex,
    bool global
) {
    clearSubstitutionPreview(h);

    int firstLine = (int)::SendMessage(h, SCI_GETFIRSTVISIBLELINE, 0, 0);
    int linesOnScreen = (int)::SendMessage(h, SCI_LINESONSCREEN, 0, 0);

    int startPos = (int)::SendMessage(h, SCI_POSITIONFROMLINE, firstLine, 0);
    int endPos = (int)::SendMessage(
        h,
        SCI_POSITIONFROMLINE,
        firstLine + linesOnScreen + 1,
        0
    );

    int flags = regex ? SCFIND_REGEXP : 0;
    ::SendMessage(h, SCI_SETSEARCHFLAGS, flags, 0);
    ::SendMessage(h, SCI_SETTARGETSTART, startPos, 0);
    ::SendMessage(h, SCI_SETTARGETEND, endPos, 0);

    bool firstOnly = !global;

    int found = (int)::SendMessage(
        h,
        SCI_SEARCHINTARGET,
        pat.size(),
        (LPARAM)pat.c_str()
    );

    while (found != -1) {
        int s = (int)::SendMessage(h, SCI_GETTARGETSTART, 0, 0);
        int e = (int)::SendMessage(h, SCI_GETTARGETEND, 0, 0);

        ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_MATCH, 0);
        ::SendMessage(h, SCI_INDICATORFILLRANGE, s, e - s);

        ::SendMessage(h, SCI_SETINDICATORCURRENT, IND_SUB_REPL, 0);
        ::SendMessage(h, SCI_INDICATORFILLRANGE, s, e - s);

        if (firstOnly) break;

        ::SendMessage(h, SCI_SETTARGETSTART, e, 0);
        found = (int)::SendMessage(
            h,
            SCI_SEARCHINTARGET,
            pat.size(),
            (LPARAM)pat.c_str()
        );
    }
}

bool CommandMode::parseSubstitution(
    const std::string& buf,
    std::string& pat,
    std::string& rep,
    bool& regex,
    bool& global,
    bool& confirm
) {
    if (buf.size() < 4) return false;

    size_t i = 0;
    if (buf[i] == ':') i++;

    if (i >= buf.size() || buf[i] != 's')
        return false;

    if (i + 1 >= buf.size())
        return false;

    char d = buf[i + 1];

    size_t p1 = i + 2;
    size_t p2 = buf.find(d, p1);
    if (p2 == std::string::npos) return false;

    size_t p3 = buf.find(d, p2 + 1);
    if (p3 == std::string::npos) return false;

    pat = buf.substr(p1, p2 - p1);
    rep = buf.substr(p2 + 1, p3 - (p2 + 1));

    if (pat.empty()) return false;

    regex = true;
    global = false;
    confirm = false;

    for (size_t k = p3 + 1; k < buf.size(); k++) {
        if (buf[k] == 'g') global = true;
        else if (buf[k] == 'c') confirm = true;
        else if (buf[k] == 'l') regex = false;
    }

    return true;
}

void CommandMode::previewSubstitutionFromBuffer(HWND h) {
    std::string pat, rep;
    bool regex, global, confirm;

    if (state.commandBuffer == lastPreviewBuffer)
        return;

    lastPreviewBuffer = state.commandBuffer;

    if (!parseSubstitution(state.commandBuffer, pat, rep, regex, global, confirm)) {
        clearSubstitutionPreview(h);
        return;
    }

    if (confirm || pat.size() < 2) {
        clearSubstitutionPreview(h);
        return;
    }

    previewSubstitution(h, pat, rep, regex, global);
}
