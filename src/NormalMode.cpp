// NormalMode.cpp
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include "../include/NormalMode.h"
#include "../include/CommandMode.h"
#include "../include/VisualMode.h"
#include "../include/Keymap.h"
#include "../include/NppVim.h"
#include "../include/Marks.h"
#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../plugin/menuCmdID.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../plugin/PluginInterface.h"
#include "../plugin/Scintilla.h"

extern NormalMode* g_normalMode;
extern VisualMode* g_visualMode;
extern CommandMode* g_commandMode;
extern NppData nppData;

int g_macroDepth = 0;

extern VimConfig g_config;

NormalMode::NormalMode(VimState& state) : state(state) {
    g_normalKeymap = std::make_unique<Keymap>(state);
    setupKeyMaps();
}

void NormalMode::setupKeyMaps() {
    auto& k = *g_normalKeymap;

    k.motion("h", 'h', "Move cursor left", [](HWND h, int c) { Motion::charLeft(h, c); })
     .motion("j", 'j', "Move cursor down", [](HWND h, int c) { Motion::lineDown(h, c); })
     .motion("k", 'k', "Move cursor up", [](HWND h, int c) { Motion::lineUp(h, c); })
     .motion("l", 'l', "Move cursor right", [](HWND h, int c) { Motion::charRight(h, c); })
     .motion("w", 'w', "Next WORD", [](HWND h, int c) { Motion::wordRight(h, c); })
     .motion("W", 'W', "Next word", [](HWND h, int c) { Motion::wordRightBig(h, c); })
     .motion("b", 'b', "Previous word", [](HWND h, int c) { Motion::wordLeft(h, c); })
     .motion("B", 'B', "Previous WORD", [](HWND h, int c) { Motion::wordLeftBig(h, c); })
     .motion("e", 'e', "Word end", [](HWND h, int c) { Motion::wordEnd(h, c); })
     .motion("E", 'E', "WORD end", [](HWND h, int c) { Motion::wordEndBig(h, c); })
     .motion("0", '0', "Line start", [](HWND h, int c) { Motion::lineStart(h, 1); })
     .motion("$", '$', "Line end", [](HWND h, int c) { Motion::lineEnd(h, c); })
     .motion("^", '^', "Line start", [](HWND h, int c) { Motion::lineStart(h, c); })
     .motion("{", '{', "Paragraph up", [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::paragraphUp(h, c);
    })
     .motion("}", '}', "Paragraph down", [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::paragraphDown(h, c);
     })
     .motion(")", ')', "Next sentence", [](HWND h, int c) {
         ::SendMessage(h, SCI_LINEEND, c, 0);
         ::SendMessage(h, SCI_LINEDOWN, c, 0);
         ::SendMessage(h, SCI_VCHOME, c, 0);
    })
    .motion("(", '(', "Previous sentence", [](HWND h, int c) {
        ::SendMessage(h, SCI_VCHOME, c, 0);
        ::SendMessage(h, SCI_LINEUP, c, 0);
        ::SendMessage(h, SCI_VCHOME, c, 0);
    })
     .motion("%", '%', "Matching bracket", [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        int match = ::SendMessage(h, SCI_BRACEMATCH, pos, 0);
        if (match != -1) ::SendMessage(h, SCI_GOTOPOS, match, 0);
     })
     .motion("H", 'H', "Page up", [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::pageUp(h);
    })
     .motion("L", 'L', "Page down", [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::pageDown(h);
     })
    .set("M", "Screen middle", [this](HWND h, int c) {
        int firstVisible = ::SendMessage(h, SCI_GETFIRSTVISIBLELINE, 0, 0);
        int linesOnScreen = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0);
        int middleLine = firstVisible + linesOnScreen / 2;
        ::SendMessage(h, SCI_GOTOLINE, middleLine, 0);
        ::SendMessage(h, SCI_VCHOME, 0, 0);
    })
     .motion("G", 'G', "File end", [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
         if (c == 1) Motion::documentEnd(h);
         else Motion::gotoLine(h, c);
     })
     .set("gg", "File start",[this](HWND h, int c) {
        if (state.opPending) {
            int count = (state.repeatCount > 0) ? state.repeatCount : 1;

            Utils::beginUndo(h);
            applyOperatorToMotion(h, state.opPending, '{', INT_MAX);
            Utils::endUndo(h);

            state.opPending = 0;
            state.repeatCount = 0;
            return;
        }

         long pos = Utils::caretPos(h);
         int line = Utils::caretLine(h);
         state.recordJump(pos, line);
         if (c > 1) Motion::gotoLine(h, c);
         else Motion::documentStart(h);
         pos = Utils::caretPos(h);
         Utils::select(h, pos, pos);
         state.recordLastOp(OP_MOTION, c, 'g');
     })
     .set("ge", "Previous word end",  [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            int pos = Utils::caretPos(h);

            if (pos > 0)
                pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);

            while (pos > 0) {
                char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
                if (!isspace(ch)) break;
                pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
            }

            while (pos > 0) {
                char ch = ::SendMessage(h, SCI_GETCHARAT, pos - 1, 0);
                if (isspace(ch)) break;
                pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
            }

            while (true) {
                char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
                if (isspace(ch)) break;

                int next = ::SendMessage(h, SCI_POSITIONAFTER, pos, 0);
                if (next == pos) break;
                pos = next;
            }

            pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);

            ::SendMessage(h, SCI_GOTOPOS, pos, 0);
        }
    })
    .motion("gE", 'E', "Previous WORD end", [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            int pos = Utils::caretPos(h);

            if (pos > 0)
                pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);

            while (pos > 0) {
                char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
                if (ch != ' ' && ch != '\t' && ch != '\n') break;
                pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
            }

            while (pos > 0) {
                char ch = ::SendMessage(h, SCI_GETCHARAT, pos - 1, 0);
                if (ch == ' ' || ch == '\t' || ch == '\n') break;
                pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
            }

            while (true) {
                char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
                if (ch == ' ' || ch == '\t' || ch == '\n') break;

                int next = ::SendMessage(h, SCI_POSITIONAFTER, pos, 0);
                if (next == pos) break;
                pos = next;
            }

            pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);

            ::SendMessage(h, SCI_GOTOPOS, pos, 0);
        }
    })
    .set("gf", "Goto file", [](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int lineStart = Utils::lineStart(h, line);
        int lineEnd = Utils::lineEnd(h, line);

        int left = pos;
        while (left > lineStart) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, left - 1, 0);
            if (ch == ' ' || ch == '\t' || ch == '"' || ch == '\'' || ch == '<' || ch == '(' || ch == '[') break;
            left--;
        }

        int right = pos;
        while (right < lineEnd) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, right, 0);
            if (ch == ' ' || ch == '\t' || ch == '"' || ch == '\'' || ch == '>' || ch == ')' || ch == ']' || ch == ',' || ch == ';') break;
            right++;
        }

        if (right > left) {
            std::string text = Utils::getTextRange(h, left, right);

            int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
            if (wideLen > 0) {
                std::vector<wchar_t> pathWide(wideLen);
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pathWide.data(), wideLen);

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

                if (PathFileExistsW(fullPath)) {
                    ::SendMessageW(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)fullPath);
                } else {
                    ::SendMessageW(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)pathWide.data());
                }
            }
        }
    })
    .set("gx", "Goto link", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int lineStart = Utils::lineStart(h, line);
        int lineEnd = Utils::lineEnd(h, line);

        int left = pos;
        while (left > lineStart) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, left - 1, 0);
            if (std::isspace(ch) || ch == '"' || ch == '\'' || ch == '<' || ch == '(' || ch == '[') break;
            left--;
        }

        int right = pos;
        while (right < lineEnd) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, right, 0);
            if (std::isspace(ch) || ch == '"' || ch == '\'' || ch == '>' || ch == ')' || ch == ']') break;
            right++;
        }

        if (right > left) {
            std::string text = Utils::getTextRange(h, left, right);

            int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
            if (wideLen > 0) {
                std::vector<wchar_t> urlWide(wideLen);
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, urlWide.data(), wideLen);
                ShellExecuteW(NULL, L"open", urlWide.data(), NULL, NULL, SW_SHOWNORMAL);
            }
        }
    })
    .motion("_", '_', "First non-blank char", [](HWND h, int c) {
        Motion::lineStart(h, c);
    });

    k.set("g0", "Screen line start", [](HWND h, int c) {
        ::SendMessage(h, SCI_VCHOME, 0, 0);
    })
    .motion("g^", '^', "First non-blank (screen line)", [](HWND h, int c) {
        ::SendMessage(h, SCI_VCHOME, 0, 0);
    })
    .motion("g$", '$', "Screen line end", [](HWND h, int c) {
        ::SendMessage(h, SCI_LINEEND, 0, 0);
    })
    .motion("g_", '_', "Last non-blank char", [](HWND h, int c) {
        int line = Utils::caretLine(h);
        int start = Utils::lineStart(h, line);
        int end = Utils::lineEnd(h, line);

        int pos = end;
        if (pos > 0)
            pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);

        while (pos > 0) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
            if (ch != ' ' && ch != '\t') break;
            pos = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
        }

        ::SendMessage(h, SCI_GOTOPOS, pos, 0);
    })
    .motion("gj", 'j', "Down (visual line)", [](HWND h, int c) {
        for (int i = 0; i < c; i++)
            ::SendMessage(h, SCI_LINEDOWN, 0, 0);
    })
    .motion("gk", 'k', "Up (visual line)", [](HWND h, int c) {
        for (int i = 0; i < c; i++)
            ::SendMessage(h, SCI_LINEUP, 0, 0);
    })
    .motion("gm", 'm', "Middle of screen line", [](HWND h, int c) {
        int first = ::SendMessage(h, SCI_GETFIRSTVISIBLELINE, 0, 0);
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0);
        int mid = first + lines / 2;
        ::SendMessage(h, SCI_GOTOLINE, mid, 0);
    })
    .set("g;", "Last edit position", [this](HWND h, int c) {
        if (state.lastInsertPos != -1)
            ::SendMessage(h, SCI_GOTOPOS, state.lastInsertPos, 0);
    })
    .set("g,", "Previous edit (stub)", [this](HWND h, int c) {
    })
    .set("gJ", "Join lines (no space)", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::joinLines(h, Utils::caretLine(h), c, false);
        Utils::endUndo(h);
    })
    .set("g~", "Toggle case (operator)", [this](HWND h, int c) {
        state.opPending = '~';
        Utils::setStatus(TEXT("-- TOGGLE CASE --"));
    })
    .set("gu", "Lowercase (operator)", [this](HWND h, int c) {
        state.opPending = 'u';
        Utils::setStatus(TEXT("-- LOWERCASE --"));
    })
    .set("gU", "Uppercase (operator)", [this](HWND h, int c) {
        state.opPending = 'U';
        Utils::setStatus(TEXT("-- UPPERCASE --"));
    })
    .set("gq", "Format text (stub)", [this](HWND h, int c) {
    })
    .set("gw", "Format keep cursor (stub)", [this](HWND h, int c) {
    })
    .set("g*", "Search word (no boundary)", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        auto bounds = Utils::findWordBounds(h, pos);
        if (bounds.first != bounds.second && g_commandMode) {
            std::string text = Utils::getTextRange(h, bounds.first, bounds.second);
            g_commandMode->performSearch(h, text.c_str(), true);
            g_commandMode->searchNext(h);
        }
    })
    .set("g#", "Search backward (no boundary)", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        auto bounds = Utils::findWordBounds(h, pos);
        if (bounds.first != bounds.second && g_commandMode) {
            std::string text = Utils::getTextRange(h, bounds.first, bounds.second);
            g_commandMode->performSearch(h, text.c_str(), true);
            g_commandMode->searchPrevious(h);
        }
    })
    .set("gd", "Goto local definition", [this](HWND h, int c) {
        NormalMode::gotoDefinition(h, state, false);
    })
    .set("gD", "Goto global definition", [this](HWND h, int c) {
        NormalMode::gotoDefinition(h, state, false);
    })
    .set("ga", "Show char info", [](HWND h, int c) {
        int pos = Utils::caretPos(h);
        char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
        std::wstring msg = L"ASCII: " + std::to_wstring((int)ch);
        Utils::setStatus(msg.c_str());
    })
    .set("gp", "Paste after (stay after)", [this](HWND h, int c) {
        g_normalKeymap->handleKey(h, 'p');
        Motion::charRight(h, 1);
    })
    .set("gP", "Paste before (stay after)", [this](HWND h, int c) {
        g_normalKeymap->handleKey(h, 'P');
        Motion::charRight(h, 1);
    })
    .set("g?", "Rot13 operator", [this](HWND h, int c) {
        state.opPending = '?';
    })
    .set("gR", "Virtual replace", [this](HWND h, int c) {
        state.mode = INSERT;
        Utils::sci(h, SCI_SETOVERTYPE, true, 0);
    })
    .set("g??", "Rot13 line", [this](HWND h, int c) {
        int line = Utils::caretLine(h);
        auto [s, e] = Utils::lineRange(h, line, true);
        Utils::rot13(h, s, e);
    });

    k.set("d", "Delete line", [this](HWND h, int c) {
         state.resetPending();
         state.lastYankLinewise = true;
         Utils::beginUndo(h);

         char reg = Utils::getCurrentRegister();

         for (int i = 0; i < c; ++i) deleteLineOnce(h);
         Utils::endUndo(h);
         state.recordLastOp(OP_DELETE_LINE, c);
     })
     .set("y", "Yank line", [this](HWND h, int c) {
         state.resetPending();
         state.lastYankLinewise = true;
         Utils::beginUndo(h);

        char reg = Utils::getCurrentRegister();
        if (state.opPending == 'y' && state.repeatCount == 0) {
            // Handle "ayy" - yank to register a
        }

         for (int i = 0; i < c; ++i) yankLineOnce(h);
         Utils::endUndo(h);
         state.recordLastOp(OP_YANK_LINE, c);
     })
     .set("c", "Change line", [this](HWND h, int c) {
         state.resetPending();
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) deleteLineOnce(h);
         Utils::endUndo(h);
         enterInsertMode();
         state.recordLastOp(OP_MOTION, c, 'c');
     });

    k.set("iw", "Inner word", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', 'w');
        state.resetPending();
    })
    .set("aw", "Around word", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', 'w');
        state.resetPending();
    })
    .set("iW", "Inner WORD", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', 'W');
        state.resetPending();
    })
    .set("aW", "Around WORD", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', 'W');
        state.resetPending();
    })
    .set("ip", "Inner paragraph", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', 'p');
        state.resetPending();
    })
    .set("ap", "Around paragraph", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', 'p');
        state.resetPending();
    })
    .set("is", "Inner sentence", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', 's');
        state.resetPending();
    })
    .set("as", "Around sentence", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', 's');
        state.resetPending();
    })
    .set("i(", "Inner parentheses", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', '(');
        state.resetPending();
    })
    .set("a(", "Around parentheses", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', '(');
        state.resetPending();
    })
    .set("i[", "Inner brackets", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', '[');
        state.resetPending();
    })
    .set("a[", "Around brackets", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', '[');
        state.resetPending();
    })
    .set("i{", "Inner braces", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', '{');
        state.resetPending();
    })
    .set("a{", "Around braces", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', '{');
        state.resetPending();
    })
    .set("i\"", "Inner double quotes", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', '"');
        state.resetPending();
    })
    .set("a\"", "Around double quotes", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', '"');
        state.resetPending();
    })
    .set("i'", "Inner single quotes", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', '\'');
        state.resetPending();
    })
    .set("a'", "Around single quotes", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', '\'');
        state.resetPending();
    })
    .set("i`", "Inner backticks", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'i', '`');
        state.resetPending();
    })
    .set("a`", "Around backticks", [this](HWND h, int c) {
        if (!state.opPending) return;
        TextObject t; t.apply(h, state, state.opPending, 'a', '`');
        state.resetPending();
    });


    k.set("i", "Insert mode", [this](HWND h, int c) {
        enterInsertMode();
    })
    .set("a", "Append mode", [this](HWND h, int c) {
        Motion::charRight(h, 1);
        enterInsertMode();
    })
    .set("A", "Append line end", [this](HWND h, int c) {
        ::SendMessage(h, SCI_LINEEND, 0, 0);
        enterInsertMode();
    })
    .set("I", "Insert line start", [this](HWND h, int c) {
        ::SendMessage(h, SCI_VCHOME, 0, 0);
        enterInsertMode();
    })
    .set("o", "New line below", [this](HWND h, int c) {
        ::SendMessage(h, SCI_LINEEND, 0, 0);
        ::SendMessage(h, SCI_NEWLINE, 0, 0);
        enterInsertMode();
    })
    .set("O", "New line above", [this](HWND h, int c) {
        ::SendMessage(h, SCI_HOME, 0, 0);
        ::SendMessage(h, SCI_NEWLINE, 0, 0);
        Motion::lineUp(h, 1);
        enterInsertMode();
    });

     k.set("D", "Delete to end", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; i++) {
             int pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             int end = Utils::lineEnd(h, line);
             if (pos < end) {
                Utils::clear(h, pos, end);
             }
         }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'D');
     })
     .set("C", "Change to end", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; i++) {
             int pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             int end = Utils::lineEnd(h, line);
             if (pos < end) {
                 // Get the text being deleted
                 std::string text = Utils::getTextRange(h, pos, end);
                 
                 // Store in register if not blackhole
                 if (!state.deleteToBlackhole) {
                     char reg = Utils::getCurrentRegister();
                     if (reg != '_') {  // Skip blackhole register
                         Utils::storeRegister(reg, text.c_str());
                     }
                 }
                 
                 ::SendMessage(h, SCI_DELETERANGE, pos, end - pos);
             }
         }
         Utils::endUndo(h);
         enterInsertMode();
     });

    k.set("x", "Delete char", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) {
             int pos = Utils::caretPos(h);
             int docLen = (int)Utils::sci(h, SCI_GETLENGTH);
             if (pos >= docLen) break;
             int next = Utils::sci(h, SCI_POSITIONAFTER, pos, 0);
             
             // Get the character before deleting
             std::string text = Utils::getTextRange(h, pos, next);
             
             // Store in register if not blackhole
             if (!state.deleteToBlackhole && g_config.xStoreClipboard) {
                 char reg = Utils::getCurrentRegister();
                 if (reg != '_') {  // Skip blackhole register
                     Utils::storeRegister(reg, text.c_str());
                 }
             }
             
            Utils::clear(h, pos, next);
         }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'x');
     })
     .set("X", "Delete backward", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) {
             int pos = Utils::caretPos(h);
             if (pos <= 0) break;
             int prev = Utils::sci(h, SCI_POSITIONBEFORE, pos, 0);
             if (prev < 0) prev = 0;
             
             // Get the character before deleting
             std::string text = Utils::getTextRange(h, prev, pos);
             
             // Store in register if not blackhole
             if (!state.deleteToBlackhole) {
                 char reg = Utils::getCurrentRegister();
                 if (reg != '_') {  // Skip blackhole register
                     Utils::storeRegister(reg, text.c_str());
                 }
             }
             
             Utils::sci(h, SCI_DELETERANGE, prev, pos - prev);
         }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'X');
     })
     .set("r", "Replace char", [this](HWND h, int c) {
         state.replacePending = true;
         Utils::setStatus(TEXT("-- REPLACE CHAR --"));
     })
     .set("R", "Replace mode", [this](HWND h, int c) {
         state.mode = INSERT;
         Utils::setStatus(TEXT("-- REPLACE --"));
         Utils::sci(h, SCI_SETOVERTYPE, true, 0);
         Utils::sci(h, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
         state.recordLastOp(OP_MOTION, c, 'R');
     })
     .motion("~", '~', "Toggle case", [this](HWND h, int c) { motion.toggleCase(h, c); });

    k.set("J", "Join lines", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::joinLines(h, Utils::caretLine(h), c, true);
        Utils::endUndo(h);
        state.recordLastOp(OP_MOTION, c, 'J');
     });

    k.set("p", "Paste after", [this](HWND h, int c) {
        if (state.mode == VISUAL) {
            char reg = Utils::getCurrentRegister();
            std::string content = Utils::getRegisterContent(reg);
            if (content.empty() && (reg == '"' || reg == '+' || reg == '*')) {
                if (OpenClipboard(h)) {
                    HANDLE hData = GetClipboardData(CF_TEXT);
                    if (hData) {
                        char* pszText = (char*)GlobalLock(hData);
                        if (pszText) { content = pszText; GlobalUnlock(hData); }
                    }
                    CloseClipboard();
                }
            }
            if (!content.empty()) {
                Utils::beginUndo(h);
                // Save the selection into the unnamed register before overwriting
                int selStart = (int)::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
                int selEnd   = (int)::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
                std::string selText = Utils::getTextRange(h, selStart, selEnd);
                Utils::storeRegister('"', selText.c_str());

                ::SendMessage(h, SCI_REPLACESEL, 0, (LPARAM)content.c_str());
                Utils::endUndo(h);
            }
            if (g_normalMode) g_normalMode->enter();
            return;
        }
        
        char reg = Utils::getCurrentRegister();
        std::string content;

        if (reg == '+' || reg == '*') {
            // Get from system clipboard
            if (OpenClipboard(h)) {
                HANDLE hData = GetClipboardData(CF_TEXT);
                if (hData) {
                    char* pszText = (char*)GlobalLock(hData);
                    if (pszText) {
                        content = pszText;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
        } else {
            content = Utils::getRegisterContent(reg);
            if (content.empty() && reg == '"') {
                if (OpenClipboard(h)) {
                    HANDLE hData = GetClipboardData(CF_TEXT);
                    if (hData) {
                        char* pszText = (char*)GlobalLock(hData);
                        if (pszText) { content = pszText; GlobalUnlock(hData); }
                    }
                    CloseClipboard();
                }
            }
        }

        if (!content.empty()) {
            Utils::beginUndo(h);
            bool linewise = state.lastYankLinewise;
            if (linewise) {
                Utils::pasteAfter(h, c, true);
            } else {
                for (int i = 0; i < c; i++) {
                    ::SendMessage(h, SCI_REPLACESEL, 0, (LPARAM)content.c_str());
                }
            }
            Utils::endUndo(h);
        }
        state.recordLastOp(OP_PASTE, c);
     })
     .set("P", "Paste before", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::pasteBefore(h, c, state.lastYankLinewise);
        Utils::endUndo(h);
        state.recordLastOp(OP_PASTE, c);
     });

    k.set("\"", "Select register", [this](HWND h, int c) {
        state.awaitingRegister = true;
        Utils::setStatus(TEXT("-- Select register --"));
    })
    // .set("_", [this](HWND h, int c) {
    //     Utils::setCurrentRegister('_');
    //     state.deleteToBlackhole = true;
    // })
    .set("_d", "Delete without saving to clipboard", [this](HWND h, int c) {
        Utils::setCurrentRegister('_');
        state.deleteToBlackhole = true;
        g_normalKeymap->handleKey(h, 'd');
        Utils::setCurrentRegister('"');
        state.deleteToBlackhole = false;
    })
    .set("_y", "Yank without saving to clipboard", [this](HWND h, int c) {
        Utils::setCurrentRegister('_');
        g_normalKeymap->handleKey(h, 'y');
        Utils::setCurrentRegister('"');
        state.deleteToBlackhole = false;
    });

    k.set("/", "Forward search", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter('/'); })
     .set("?", "Regex search", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter('?'); })
     .set("n", "Next match", [this](HWND h, int c) {
         if (g_commandMode) {
             long pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             state.recordJump(pos, line);
             for (int i = 0; i < c; i++) g_commandMode->searchNext(h);
             state.recordLastOp(OP_MOTION, c, 'n');
         }
     })
     .set("N", "Previous match", [this](HWND h, int c) {
         if (g_commandMode) {
             long pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             state.recordJump(pos, line);
             for (int i = 0; i < c; i++) g_commandMode->searchPrevious(h);
             state.recordLastOp(OP_MOTION, c, 'N');
         }
     })
     .set("*", "Search word forward", [this](HWND h, int c) {
         int pos = Utils::caretPos(h);
         auto bounds = Utils::findWordBounds(h, pos);
         if (bounds.first != bounds.second && g_commandMode) {
             int len = bounds.second - bounds.first;
             std::string text = Utils::getTextRange(h, bounds.first, bounds.second);
             g_commandMode->performSearch(h, text.c_str(), false);
             g_commandMode->searchNext(h);
             state.recordLastOp(OP_MOTION, c, '*');
         }
     })
     .set("#", "Search word backward", [this](HWND h, int c) {
         int pos = Utils::caretPos(h);
         auto bounds = Utils::findWordBounds(h, pos);
         if (bounds.first != bounds.second && g_commandMode) {
             int len = bounds.second - bounds.first;
             std::string text = Utils::getTextRange(h, bounds.first, bounds.second);
             g_commandMode->performSearch(h, text.c_str(), false);
             g_commandMode->searchPrevious(h);
             state.recordLastOp(OP_MOTION, c, '#');
         }
     });

    k.set("f", "Find character", [this](HWND h, int c) {
         state.opPending = 'f';
         state.textObjectPending = 'f';
         Utils::setStatus(TEXT("-- find char (forward) --"));
     })
     .set("F", "Find backward", [this](HWND h, int c) {
         state.opPending = 'F';
         state.textObjectPending = 'f';
         Utils::setStatus(TEXT("-- find char (backward) --"));
     })
     .set("t", "Till character", [this](HWND h, int c) {
         state.opPending = 't';
         state.textObjectPending = 't';
         Utils::setStatus(TEXT("-- till char (forward) --"));
     })
     .set("T", "Till backward", [this](HWND h, int c) {
         state.opPending = 'T';
         state.textObjectPending = 't';
         Utils::setStatus(TEXT("-- till char (backward) --"));
     })
     .set(";", "Repeat find", [this](HWND h, int c) {
        if (state.lastSearchChar == 0) return;
        bool fwd = state.lastSearchForward;
        bool till = state.lastSearchTill;
        char ch = state.lastSearchChar;
        int before = Utils::caretPos(h);
        if (till) {
            int nudge = fwd ? before + 1 : before - 1;
            ::SendMessage(h, SCI_SETCURRENTPOS, nudge, 0);
            ::SendMessage(h, SCI_SETSEL, nudge, nudge);
            if (fwd) Motion::tillChar(h, c, ch);
            else Motion::tillCharBack(h, c, ch);
        } else {
            if (fwd) Motion::nextChar(h, c, ch);
            else Motion::prevChar(h, c, ch);
        }
        state.recordLastOp(OP_MOTION, c, ';');
        ::SendMessage(h, SCI_SETEMPTYSELECTION, Utils::caretPos(h), 0);
     })
     .set(",", "Reverse find", [this](HWND h, int c) {
        if (state.lastSearchChar == 0) return;
        bool fwd = !state.lastSearchForward;
        bool till = state.lastSearchTill;
        char ch = state.lastSearchChar;
        int before = Utils::caretPos(h);
        if (till) {
            if (fwd) Motion::tillChar(h, c, ch);
            else Motion::tillCharBack(h, c, ch);
        } else {
            if (fwd) Motion::nextChar(h, c, ch);
            else Motion::prevChar(h, c, ch);
        }
        state.recordLastOp(OP_MOTION, c, ',');
        int pos = Utils::caretPos(h);
        Utils::select(h, pos, pos);
     });

    k.set("v", [](HWND h, int c) { if (g_visualMode) g_visualMode->enterChar(h); })
     .set("V", [](HWND h, int c) { if (g_visualMode) g_visualMode->enterLine(h); });

    k.set(":", "Enter Command mode", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter(':'); })
     .set("u", "Undo", [this](HWND h, int c) {
         ::SendMessage(h, SCI_UNDO, 0, 0);
         state.recordLastOp(OP_MOTION, 1, 'u');
     })
     .set(".", "Repeat", [this](HWND h, int c) {
         if (state.lastOp.type == OP_NONE) return;
         Utils::beginUndo(h);
         int rc = (state.repeatCount > 0) ? state.repeatCount : state.lastOp.count;
         switch (state.lastOp.type) {
         case OP_DELETE_LINE:
             for (int i = 0; i < rc; ++i) deleteLineOnce(h);
             break;
         case OP_YANK_LINE:
             for (int i = 0; i < rc; ++i) yankLineOnce(h);
             break;
         case OP_PASTE_LINE:
             for (int i = 0; i < rc; ++i) Utils::pasteAfter(h, 1, state.lastYankLinewise);
             break;
         case OP_MOTION:
             if (state.lastOp.textModifier && state.lastOp.textObject) {
                TextObject t;
                t.apply(
                    h,
                    state,
                    state.lastOp.motion,
                    state.lastOp.textModifier,
                    state.lastOp.textObject
                );
                break;
            }
             if (state.lastOp.motion == 'x' || state.lastOp.motion == 'X') {
                 auto& k = *g_normalKeymap;
                 k.handleKey(h, state.lastOp.motion);
             } else if (state.lastOp.motion == 'f' || state.lastOp.motion == 'F' ||
                        state.lastOp.motion == 't' || state.lastOp.motion == 'T') {
                 if (state.lastOp.searchChar != 0) {
                     if (state.lastOp.motion == 'f') Motion::nextChar(h, rc, state.lastOp.searchChar);
                     else if (state.lastOp.motion == 'F') Motion::prevChar(h, rc, state.lastOp.searchChar);
                     else if (state.lastOp.motion == 't') Motion::tillChar(h, rc, state.lastOp.searchChar);
                     else if (state.lastOp.motion == 'T') Motion::tillCharBack(h, rc, state.lastOp.searchChar);
                 }
             } else {
                 applyOperatorToMotion(h, 'd', state.lastOp.motion, rc);
             }
             break;
         case OP_REPLACE:
             state.replacePending = true;
             break;
        case OP_INDENT:
            if (state.lastVisualAnchor >= 0 && state.lastVisualCaret >= 0) {
                Utils::select(h, state.lastVisualAnchor, state.lastVisualCaret);
            }

            if (state.lastOp.motion == '>') {
                Utils::handleIndent(h, rc);
            } else if (state.lastOp.motion == '<') {
                Utils::handleUnindent(h, rc);
            }
            break;
         }
         Utils::endUndo(h);
         state.repeatCount = 0;
     });

    k.set("zz", "Center cursor", [this](HWND h, int c) {
         int pos = Utils::caretPos(h);
         int line = Utils::caretLine(h);
         ::SendMessage(h, SCI_SETFIRSTVISIBLELINE,
             line - (::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2), 0);
         state.recordLastOp(OP_MOTION, c, 'z');
     })
    .set("zt", "Cursor to top", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line, 0);
    })
    .set("zb", "Cursor to bottom", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int linesOnScreen = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line - linesOnScreen + 1, 0);
    })
    .set("zo", "Open fold", [](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION,
            Utils::caretPos(h), 0);
        for (int i = 0; i < c; i++) {
            ::SendMessage(h, SCI_FOLDLINE, line, SC_FOLDACTION_EXPAND);
        }
    })
    .set("zc", "Close fold", [](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION,
            Utils::caretPos(h), 0);
        for (int i = 0; i < c; i++) {
            ::SendMessage(h, SCI_FOLDLINE, line, SC_FOLDACTION_CONTRACT);
        }
    })
    .set("za", "Toggle fold", [](HWND h, int c) {
        ::SendMessage(h, SCI_TOGGLEFOLD, ::SendMessage(h, SCI_LINEFROMPOSITION, Utils::caretPos(h), 0), 0);
    })
    .set("q", "Start/stop recording macro", [this](HWND h, int c) {
        if (state.recordingMacro) {
            if (!state.macroBuffer.empty() && state.macroBuffer.back() == 'q') {
                state.macroBuffer.pop_back();
            }
            state.registers[state.macroRegister] = std::string(state.macroBuffer.begin(), state.macroBuffer.end());
            state.recordingMacro = false;
            state.recordingInsertMacro = false;
            state.macroRegister = '\0';
            Utils::setStatus(TEXT("-- Stopped recording --"));
        } else {
            state.awaitingMacroRegister = true;
            Utils::setStatus(TEXT("-- Recording macro --"));
        }
    })
    .set("@", "Execute macro", [this](HWND h, int c) {
        if (g_macroDepth >= MAX_MACRO_DEPTH) {
            Utils::setStatus(TEXT("Macro recursion limit reached"));
            return;
        }

        char reg = Utils::getCurrentRegister();

        if (state.registers.find(reg) == state.registers.end() || state.registers[reg].empty()) {
            Utils::setStatus(TEXT("Register is empty"));
            return;
        }
        std::string macroContent = state.registers[reg];

        // Iteration limit to prevent infinite loops
        const int MAX_MACRO_ITERATIONS = 1000;
        int iterations = 0;

        g_macroDepth++;

        for (int iteration = 0; iteration < c; ++iteration) {
            for (char key : macroContent) {
                if (++iterations > MAX_MACRO_ITERATIONS) {
                    Utils::setStatus(TEXT("Macro iteration limit reached"));
                    g_macroDepth--;
                    return;
                }

                // Process Windows messages to keep UI responsive
                // MSG msg;
                // while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                //     TranslateMessage(&msg);
                //     DispatchMessage(&msg);
                // }

                handleKey(h, key);
            }
        }

        g_macroDepth--;
        Utils::setCurrentRegister('"');
        Utils::setStatus(TEXT(""));
    });

    k.set("m", "Set mark", [this](HWND h, int c) {
         state.awaitingMarkSet = true;
         Utils::setStatus(TEXT("-- Set mark --"));
     })
    .set("``", "Jump back", [this](HWND h, int c) {
        if (state.jumpList.size() < 2) return;
        int last = state.jumpList.size() - 1;
        std::swap(state.jumpList[last], state.jumpList[last - 1]);
        auto jump = state.jumpList[last];
        if (jump.position != -1) {
            long pos = Utils::caretPos(h);
            int line = Utils::caretLine(h);
            state.recordJump(pos, line);
            ::SendMessage(h, SCI_GOTOPOS, jump.position, 0);
            ::SendMessage(h, SCI_SETSEL, jump.position, jump.position);
            ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
        }
    })
     .set("'", "Jump to mark", [this](HWND h, int c) {
         if (c > 1) {
             if (state.jumpList.size() >= 2) {
                 int last = state.jumpList.size() - 1;
                 std::swap(state.jumpList[last], state.jumpList[last - 1]);
                 auto jump = state.jumpList[last];
                 if (jump.lineNumber != -1) {
                     int target = ::SendMessage(h, SCI_GETLINEINDENTPOSITION, jump.lineNumber, 0);

                     ::SendMessage(h, SCI_GOTOPOS, target, 0);
                     ::SendMessage(h, SCI_SETSEL, target, target);
                     ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
                     Utils::setStatus(TEXT("-- Jumped to last line --"));
                 }
             }
             return;
         }
         state.awaitingMarkJump = true;
         state.isBacktickJump = false;
         state.pendingJumpCount = c;
         Utils::setStatus(TEXT("-- Jump to mark (line start) --"));
     })
     .set("''", "Jump to last line", [this](HWND h, int c) {
         if (state.jumpList.size() < 2) return;
         int last = state.jumpList.size() - 1;
         std::swap(state.jumpList[last], state.jumpList[last - 1]);
         auto jump = state.jumpList[last];
         if (jump.lineNumber != -1) {
             int target = ::SendMessage(h, SCI_GETLINEINDENTPOSITION, jump.lineNumber, 0);
             ::SendMessage(h, SCI_GOTOPOS, target, 0);
             ::SendMessage(h, SCI_SETSEL, target, target);
             ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
         }
     });

    k.set(">", "Indent", [this](HWND h, int c) {
        Utils::handleIndent(h, c);
        state.recordLastOp(OP_INDENT, c, '>');
     })
     .set("<", "Unindent", [this](HWND h, int c) {
         Utils::handleUnindent(h, c);
         state.recordLastOp(OP_INDENT, c, '<');
     })
     .set("=", "Autoindent", [](HWND h, int c) { Utils::handleAutoIndent(h, c); });

    k.set("gcc", "Toggle Comment", [this](HWND h, int c) {
         ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_BLOCK_COMMENT, 0);
         state.recordLastOp(OP_MOTION, c, 'c');
     })
     .set("gt", "Previous tab", [this](HWND h, int c) {
         for (int i = 0; i < c; i++)
             ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_NEXT);
         state.recordLastOp(OP_MOTION, c, 't');
     })
     .set("gT", "Next tab", [this](HWND h, int c) {
         for (int i = 0; i < c; i++)
             ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_PREV);
         state.recordLastOp(OP_MOTION, c, 'T');
     })
    .set("gv", [this](HWND h, int c) {
        if (state.lastVisualAnchor < 0 || state.lastVisualCaret < 0) return;

        state.restoringVisual = true;

        if (state.lastVisualWasBlock)
            g_visualMode->enterBlock(h);
        else if (state.lastVisualWasLine)
            g_visualMode->enterLine(h);
        else
            g_visualMode->enterChar(h);

        ::SendMessage(h, SCI_SETSEL, state.lastVisualAnchor, state.lastVisualCaret);
        state.restoringVisual = false;
    })
     .set("gi", "Jumps to last insert position", [this](HWND h, int c) {
        if (state.lastInsertPos == -1) return;
        ::SendMessage(h, SCI_GOTOPOS, state.lastInsertPos, 0);
        enterInsertMode();
    });

    k.set("[ ", "Insert line above", [](HWND h, int c) {
         ::SendMessage(h, SCI_HOME, 0, 0);
         ::SendMessage(h, SCI_NEWLINE, 0, 0);
         Motion::lineUp(h, 1);
     })
     .set("] ", "Insert line below", [](HWND h, int c) {
         ::SendMessage(h, SCI_LINEEND, 0, 0);
         ::SendMessage(h, SCI_NEWLINE, 0, 0);
     });

    k.set("gUU", "Uppercase Whole Line", [this](HWND h, int c) {
        Utils::beginUndo(h);
        int line = Utils::caretLine(h);
        int last = Utils::lineCount(h) - 1;
        for (int i = 0; i < c && line + i <= last; i++) {
            int start = Utils::lineStart(h, line + i);
            int end = Utils::lineEnd(h, line + i);
            Utils::toUpper(h, start, end);
        }
        Utils::endUndo(h);
    })
    .set("guu", "Lowercase Whole Line", [this](HWND h, int c) {
        Utils::beginUndo(h);
        int line = Utils::caretLine(h);
        int last = Utils::lineCount(h) - 1;
        for (int i = 0; i < c && line + i <= last; i++) {
            int start = Utils::lineStart(h, line + i);
            int end = Utils::lineEnd(h, line + i);
            Utils::toLower(h, start, end);
        }
        Utils::endUndo(h);
    });

    k.set("\x01", "Ctrl+A Increment number", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int start = Utils::lineStart(h, line);
        int end = Utils::lineEnd(h, line);

        int s = pos;
        int e = pos;

        if (pos < end && std::isdigit(::SendMessage(h, SCI_GETCHARAT, pos, 0))) {
            s = pos;
        } else {
            while (s < end && !std::isdigit(::SendMessage(h, SCI_GETCHARAT, s, 0))) s++;
            if (s == end) {
                s = pos;
                while (s > start && !std::isdigit(::SendMessage(h, SCI_GETCHARAT, s - 1, 0))) s--;
                if (s == start) return;
                s--;
            }
        }

        while (s > start && std::isdigit(::SendMessage(h, SCI_GETCHARAT, s - 1, 0))) s--;
        if (s > start && ::SendMessage(h, SCI_GETCHARAT, s - 1, 0) == '-') s--;

        e = s;
        while (e < end && std::isdigit(::SendMessage(h, SCI_GETCHARAT, e, 0))) e++;

        std::vector<char> buf(e - s + 1);
        Sci_TextRangeFull tr{ { s, e }, buf.data() };
        ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);

        int n = std::stoi(buf.data()) + c;
        std::string r = std::to_string(n);

        ::SendMessage(h, SCI_SETTARGETRANGE, s, e);
        ::SendMessage(h, SCI_REPLACETARGET, r.size(), (LPARAM)r.c_str());
    })
    .set("\x18", "Ctrl+X Decrement number", [this](HWND h, int c) {
        g_normalKeymap->handleKey(h, '\x01');
    });

    k.set("\x12", "Ctrl+R - Redo", [this](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            ::SendMessage(h, SCI_REDO, 0, 0);
        }
        state.recordLastOp(OP_MOTION, c, 'R');
    });

    k.set("\x05", "Ctrl+E - scroll down", [](HWND h, int c) {
        ::SendMessage(h, SCI_LINESCROLL, 0, c);
    })
    .set("\x19", "Ctrl+Y - scroll up", [](HWND h, int c) {
        ::SendMessage(h, SCI_LINESCROLL, 0, -c);
    })
    .set("\x04", "Ctrl+D - half page down", [](HWND h, int c) {
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) {
            Motion::lineDown(h, lines);
        }
    })
    .set("\x15", "Ctrl+U - half page up", [](HWND h, int c) {
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) {
            Motion::lineUp(h, lines);
        }
    })
    .set("\x06", "Ctrl+F - page forward", [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            Motion::pageDown(h);
        }
    })
    .set("\x02", "Ctrl+B - page backward", [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            Motion::pageUp(h);
        }
    });

    k.set("S", "Change inner/around line", [this](HWND h, int c) {
        Utils::beginUndo(h);
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int start = Utils::lineStart(h, line);
        int end = Utils::lineEnd(h, line);
        ::SendMessage(h, SCI_DELETERANGE, start, end - start);
        Utils::endUndo(h);
        enterInsertMode();
    });

    k.set("|", "Go to column <num>", [](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int lineStart = Utils::lineStart(h, line);
        int lineEnd = Utils::lineEnd(h, line);
        int targetCol = (c > 0 ? c - 1 : 0);
        int targetPos = lineStart + targetCol;
        if (targetPos > lineEnd) targetPos = lineEnd;
        ::SendMessage(h, SCI_GOTOPOS, targetPos, 0);
    });

    k.set("gI", "Insert at indent", [this](HWND h, int c) {
        int line = Utils::caretLine(h);
        int start = Utils::lineStart(h, line);
        int end = Utils::lineEnd(h, line);

        int pos = start;
        while (pos < end) {
            char ch = (char)::SendMessage(h, SCI_GETCHARAT, pos, 0);
            if (ch != ' ' && ch != '\t') break;
            pos++;
        }

        ::SendMessage(h, SCI_GOTOPOS, pos, 0);
        enterInsertMode();
    });
}

void NormalMode::enter() {
    
    if (!g_config.enableKeyboardLayoutSwitching) {
        ActivateKeyboardLayout(g_englishLayout, 0);
    }

    HWND hwnd = Utils::getCurrentScintillaHandle();

    if (state.mode == VISUAL && !state.restoringVisual) {
        if (state.isBlockVisual) {
            state.lastVisualAnchor = ::SendMessage(hwnd, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            state.lastVisualCaret  = ::SendMessage(hwnd, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
        } else {
            state.lastVisualAnchor = ::SendMessage(hwnd, SCI_GETSELECTIONSTART, 0, 0);
            state.lastVisualCaret  = ::SendMessage(hwnd, SCI_GETSELECTIONEND, 0, 0);
        }
        state.lastVisualWasLine  = state.isLineVisual;
        state.lastVisualWasBlock = state.isBlockVisual;
    }

    // Save current layout and switch to English for Normal mode
    HWND focusWnd = ::GetFocus();
    HKL currentLayout = ::GetKeyboardLayout(::GetWindowThreadProcessId(focusWnd, nullptr));
    HKL englishLayout = ::LoadKeyboardLayout(TEXT("00000409"), KLF_ACTIVATE);
    if (currentLayout != englishLayout) {
        state.savedInsertLayout = currentLayout;
        ::PostMessage(focusWnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)englishLayout);
        ::ActivateKeyboardLayout(englishLayout, 0);
    }

    state.mode = NORMAL;
    state.isLineVisual = false;
    state.visualAnchor = -1;
    state.visualAnchorLine = -1;
    state.reset();
    state.lastSearchMatchCount = -1;

    if (g_normalKeymap) g_normalKeymap->reset();

    Utils::setStatus(TEXT("-- NORMAL --"));
    ::SendMessage(hwnd, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);

    if (!state.restoringVisual) {
        int caret = Utils::caretPos(hwnd);
        Utils::select(hwnd, caret, caret);
    }

    state.restoringVisual = false;
    Utils::clearSearchHighlights(hwnd);
}

void NormalMode::enterInsertMode() {
    state.lastInsertPos = Utils::caretPos(Utils::getCurrentScintillaHandle());

    if (g_config.enableKeyboardLayoutSwitching && g_userLayout) {
        ActivateKeyboardLayout(g_userLayout, 0);
    }

    HWND hwnd = Utils::getCurrentScintillaHandle();
    state.mode = INSERT;
    state.reset();

    HWND focusWnd = ::GetFocus();

    if (state.savedInsertLayout) {
        ::PostMessage(focusWnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)state.savedInsertLayout);
        ::ActivateKeyboardLayout(state.savedInsertLayout, 0);
    }

    if (state.recordingMacro) {
        state.recordingInsertMacro = true;
        state.insertMacroBuffers.push_back(std::vector<char>());
    }

    Utils::setStatus(TEXT("-- INSERT --"));
    ::SendMessage(hwnd, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}

void handleInsertModeChar(HWND hwnd, char c, VimState& state) {
    if (state.mode != INSERT) return;

    if (state.recordingInsertMacro) {
        state.insertMacroBuffers.back().push_back(c);
    }

    if (c == 27) { // ESC key
        if (state.recordingInsertMacro) {
            state.insertMacroBuffers.back().push_back('\x1B'); // Record ESC
            state.recordingInsertMacro = false;
        }
        if (g_normalMode) g_normalMode->enter();
        return;
    }

    ::SendMessage(hwnd, SCI_ADDTEXT, 1, (LPARAM)&c);
}

void NormalMode::handleKey(HWND hwnd, char c) {
    if (state.awaitingMacroRegister) {
        if (Utils::isValidRegister(c)) {
            state.macroRegister = c;
            state.recordingMacro = true;
            state.recordingInsertMacro = false;
            state.macroBuffer.clear();
            state.insertMacroBuffers.clear();
            state.awaitingMacroRegister = false;

            std::wstring status = L"-- Recording @";
            status += (wchar_t)c;
            status += L" --";
            Utils::setStatus(status.c_str());
        } else {
            Utils::setStatus(TEXT("Invalid register"));
            state.awaitingMacroRegister = false;
        }
        return;
    }

    if (state.recordingMacro && c != 'q' && !state.awaitingRegister) {
        state.macroBuffer.push_back(c);
    }

    if (!state.opPending && !g_normalKeymap->hasPending() && (c == 'd' || c == 'c' || c == 'y')) {
        state.lastYankLinewise = false;
        state.opPending = c;
        Utils::setStatus(
            c == 'd' ? TEXT("-- DELETE --") :
            c == 'c' ? TEXT("-- CHANGE --") :
                    TEXT("-- YANK --")
        );
        return;
    }

    if (state.replacePending) {
        handleReplaceInput(hwnd, c);
        return;
    }

    if (state.awaitingMarkSet) {
        handleMarkSetInput(hwnd, c);
        return;
    }

    if (state.awaitingMarkJump) {
        handleMarkJumpInput(hwnd, c, state.isBacktickJump);
        return;
    }

    if (state.awaitingRegister) {
        state.awaitingRegister = false;

        if (Utils::isValidRegister(c)) {
            Utils::setCurrentRegister(c);
            state.deleteToBlackhole = (c == '_');
        } else {
            Utils::setStatus(TEXT("-- Invalid register --"));
        }
        Utils::setStatus(TEXT("-- NORMAL --"));
        return;
    }

    if (state.textObjectPending == 'f' || state.textObjectPending == 't') {
        char searchType = state.opPending;
        int count = (state.repeatCount > 0) ? state.repeatCount : 1;
        handleCharSearchInput(hwnd, c, searchType, count);
        return;
    }

    if (state.textObjectPending) {
        char modifier = state.textObjectPending;
        char op = state.opPending;

        state.textObjectPending = 0;
        state.opPending = 0;

        TextObject textObj;
        textObj.apply(hwnd, state, op, modifier, c);
        state.repeatCount = 0;
        return;
    }

    if ((state.opPending == 'd' || state.opPending == 'c' || state.opPending == 'y' ||
         (state.mode == VISUAL && state.opPending == 'v')) &&
        (c == 'i' || c == 'a')) {
        state.textObjectPending = c;
        return;
    }

    if (state.opPending && state.textObjectPending == 'g') {
        state.textObjectPending = 0;

        int count = (state.repeatCount > 0) ? state.repeatCount : 1;

        if (g_normalKeymap) {
            g_normalKeymap->handleKey(hwnd, 'g');
            g_normalKeymap->handleKey(hwnd, c);
        }

        state.opPending = 0;
        state.repeatCount = 0;
        return;
    }

    if (state.opPending && !state.textObjectPending) {
        if (c == 'g') {
            state.textObjectPending = 'g';
            return;
        }
         
        if (c == 'w' || c == 'W' || c == 'b' || c == 'B' || c == 'e' || c == 'E' ||
            c == 'h' || c == 'l' || c == 'j' || c == 'k' ||
            c == '$' || c == '^' || c == '0' || c == 'G' || c == '%' ||
            c == '{' || c == '}') {

            int count = (state.repeatCount > 0) ? state.repeatCount : 1;
            Utils::beginUndo(hwnd);
            applyOperatorToMotion(hwnd, state.opPending, c, count);
            Utils::endUndo(hwnd);

            state.opPending = 0;
            state.repeatCount = 0;
            return;
        }
    }

    if (state.mode == VISUAL && (c == 'i' || c == 'a')) {
        state.opPending = 'v';
        state.textObjectPending = c;
        return;
    }

    if (state.mode == VISUAL) {
        if (state.opPending == 'v' && (c == 'i' || c == 'a')) {
            state.textObjectPending = c;
            return;
        }

        if (state.textObjectPending && state.opPending == 'v') {
            char modifier = state.textObjectPending;
            char op = state.opPending;

            state.textObjectPending = 0;
            state.opPending = 0;

            TextObject textObj;
            textObj.apply(hwnd, state, op, modifier, c);
            state.repeatCount = 0;

            int anchor = (int)::SendMessage(hwnd, SCI_GETANCHOR, 0, 0);
            int caret = (int)Utils::caretPos(hwnd);
            state.lastVisualAnchor = anchor;
            state.lastVisualCaret = caret;
            state.lastVisualWasBlock = false;
            state.lastVisualWasLine = false;

            return;
        }
    }

    if (g_normalKeymap) {
        g_normalKeymap->handleKey(hwnd, c);
    }
}

void NormalMode::handleCharSearchInput(HWND hwnd, char searchChar, char searchType, int count) {
    bool isTill = (searchType == 't' || searchType == 'T');
    bool isForward = (searchType == 'f' || searchType == 't');

    state.lastSearchChar = searchChar;
    state.lastSearchForward = isForward;
    state.lastSearchTill = isTill;

    if (isTill) {
        if (isForward) Motion::tillChar(hwnd, count, searchChar);
        else Motion::tillCharBack(hwnd, count, searchChar);
    } else {
        if (isForward) Motion::nextChar(hwnd, count, searchChar);
        else Motion::prevChar(hwnd, count, searchChar);
    }

    state.recordLastOp(
        OP_MOTION,
        count,
        searchType,
        searchChar
    );

    state.opPending = 0;
    state.textObjectPending = 0;
    Utils::setStatus(TEXT("-- NORMAL --"));
}

void NormalMode::handleMarkSetInput(HWND hwnd, char mark) {
    state.awaitingMarkSet = false;
    if (Marks::isValidMark(mark)) {
        Marks::setMark(hwnd, mark);
        Utils::setStatus(TEXT("-- Mark set --"));
    } else {
        Utils::setStatus(TEXT("-- Invalid mark --"));
    }
}

void NormalMode::handleMarkJumpInput(HWND hwnd, char mark, bool exactPosition) {
    state.awaitingMarkJump = false;

    if (Marks::isValidMark(mark)) {
        long pos = Utils::caretPos(hwnd);
        int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
        state.recordJump(pos, line);

        if (Marks::jumpToMark(hwnd, mark, exactPosition)) {
            long newPos = Utils::caretPos(hwnd);
            int newLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, newPos, 0);
            state.recordJump(newPos, newLine);
            Utils::setStatus(TEXT("-- Jumped to mark --"));
        } else {
            Utils::setStatus(TEXT("-- Mark not set --"));
        }
    } else {
        Utils::setStatus(TEXT("-- Invalid mark --"));
    }

    state.isBacktickJump = false;
    state.pendingJumpCount = 0;
}

void NormalMode::handleReplaceInput(HWND hwnd, char replaceChar) {
    int pos = Utils::caretPos(hwnd);
    int len = ::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);

    if (pos < len) {
        char ch = (char)Utils::sci(hwnd, SCI_GETCHARAT, pos);
        if (ch != '\r' && ch != '\n') {
            Utils::replaceChar(hwnd, pos, replaceChar);
            Utils::sci(hwnd, SCI_SETCURRENTPOS, pos);
        }
    }

    state.replacePending = false;
    state.recordLastOp(OP_REPLACE, 1, 'r');
    Utils::setStatus(TEXT("-- NORMAL --"));
}

void NormalMode::deleteLineOnce(HWND hwnd) {
    int pos = Utils::caretPos(hwnd);
    int line = Utils::caretLine(hwnd);
    auto range = Utils::lineRange(hwnd, line, true);

    // Get the text before deleting
    std::string text = Utils::getTextRange(hwnd, range.first, range.second);

    // Store in register if not blackhole
    if (!state.deleteToBlackhole && g_config.dStoreClipboard) {
        char reg = Utils::getCurrentRegister();
        if (reg != '_') {  // Skip blackhole register
            Utils::storeRegister(reg, text.c_str());
            Utils::setClipboardText(text.c_str());
        }
    }

    Utils::select(hwnd, range.first, range.second);
    ::SendMessage(hwnd, SCI_CUT, 0, 0);

    int newPos = Utils::lineStart(hwnd, line);
    ::SendMessage(hwnd, SCI_GOTOPOS, newPos, 0);
}

void NormalMode::yankLineOnce(HWND hwnd) {
    int pos = Utils::caretPos(hwnd);
    int line = Utils::caretLine(hwnd);
    auto range = Utils::lineRange(hwnd, line, true);

    // Get the text
    std::string text = Utils::getTextRange(hwnd, range.first, range.second);

    // Store in register
    char reg = Utils::getCurrentRegister();
    Utils::storeRegister(reg, text.c_str());

    Utils::select(hwnd, range.first, range.second);
    ::SendMessage(hwnd, SCI_COPY, 0, 0);
    Utils::select(hwnd, pos, pos);

    state.recordLastOp(OP_YANK_LINE, 1);
}

void NormalMode::applyOperatorToMotion(HWND hwnd, char op, char motion, int count) {

    if (motion == '(' || motion == ')' || motion == '[' || motion == ']' || motion == '<' || motion == '>' ||
        motion == '\'' || motion == '\"' || motion == '`' ||
        motion == 't' || motion == 's' || motion == 'p') {

        TextObject textObj;
        textObj.apply(hwnd, state, op, 'a', motion);
        return;
    }

    int start = Utils::caretPos(hwnd);
    int startLine = Utils::caretLine(hwnd);
    bool isLineMotion = false;

    if (motion == 'j' || motion == 'k' || motion == 'G' || motion == '{' || motion == '}') {
        isLineMotion = true;
    }

    switch (motion) {
    case 'h': Motion::charLeft(hwnd, count); break;
    case 'l': Motion::charRight(hwnd, count); break;
    case 'j': Motion::lineDown(hwnd, count); break;
    case 'k': Motion::lineUp(hwnd, count); break;
    case 'w': Motion::wordRight(hwnd, count); break;
    case 'W': Motion::wordRightBig(hwnd, count); break;
    case 'b': Motion::wordLeft(hwnd, count); break;
    case 'B': Motion::wordLeftBig(hwnd, count); break;
    case 'e': Motion::wordEnd(hwnd, count); break;
    case 'E': Motion::wordEndBig(hwnd, count); break;
    case '$': Motion::lineEnd(hwnd, count); break;
    case '^': Motion::lineStart(hwnd, count); break;
    case '0': Motion::lineStart(hwnd, 1); break;
    case 'G': {
        int gCount = (state.repeatCount > 0) ? state.repeatCount : count;
        if (gCount == 1) Motion::documentEnd(hwnd);
        else Motion::gotoLine(hwnd, count);
        break;
    }
    // case 'g':  // Handle gg motion
    //     if (count > 1) Motion::gotoLine(hwnd, count);
    //     else Motion::documentStart(hwnd);
    //     break;
    case '{': Motion::paragraphUp(hwnd, count); break;
    case '}': Motion::paragraphDown(hwnd, count); break;
    case '%': {
        int match = ::SendMessage(hwnd, SCI_BRACEMATCH, start, 0);
        if (match != -1) ::SendMessage(hwnd, SCI_GOTOPOS, match, 0);
        break;
    }
    default: break;
    }

    int end = Utils::caretPos(hwnd);
    int endLine = Utils::caretLine(hwnd);
    if (start == end && op != 'y') {
        state.opPending = 0;
        state.repeatCount = 0;
        return;
    }

    // Get selected text before operation
    std::string selectedText;
    if (start != end) {
        int selStart = (std::min)(start, end);
        int selEnd = (std::max)(start, end);
        if (isLineMotion && op == 'd') {
            // For line motions, we already captured in deleteLineOnce
        } else {
            std::string text = Utils::getTextRange(hwnd, selStart, selEnd);
            selectedText = text;
        }
    }

    // Store in register before deletion
    if (op == 'd' || op == 'c') {
        bool shouldStore = (op == 'd') ? g_config.dStoreClipboard : g_config.cStoreClipboard;
        if (shouldStore && !state.deleteToBlackhole && !selectedText.empty()) {
            char reg = Utils::getCurrentRegister();
            if (reg != '_') {  // Skip blackhole register
                if (reg == '+' || reg == '*') {
                    Utils::setClipboardText(selectedText);
                } else {
                    Utils::setRegisterContent(reg, selectedText);
                }
            }
        }
    } else if (op == 'y' && !selectedText.empty()) {
        char reg = Utils::getCurrentRegister();
        if (reg == '+' || reg == '*') {
            Utils::setClipboardText(selectedText);
        } else {
            Utils::setRegisterContent(reg, selectedText);
        }
    }

    if (isLineMotion) {
        if (startLine > endLine) {
            std::swap(startLine, endLine);
        }
        start = Utils::lineStart(hwnd, startLine);
        end = Utils::lineStart(hwnd, endLine + 1);
        state.lastYankLinewise = true;
    } else {
        if (start > end) std::swap(start, end);

        // For word-end motions, include the character under cursor
        if (motion == 'e' || motion == 'E') {
            int docLen = ::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);
            if (end < docLen) end = ::SendMessage(hwnd, SCI_POSITIONAFTER, end, 0);
        }
        state.lastYankLinewise = false;
    }

    Utils::select(hwnd, start, end);
    switch (op) {
    case 'd':
        ::SendMessage(hwnd, SCI_CUT, 0, 0);
        if (isLineMotion) {
            int newPos = Utils::lineStart(hwnd, startLine);
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, newPos, 0);
        } else {
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, start, 0);
        }
        state.recordLastOp(OP_MOTION, count, motion);
        break;
    case 'y':
        ::SendMessage(hwnd, SCI_COPY, 0, 0);
        Utils::select(hwnd, start, start);
        state.recordLastOp(OP_MOTION, count, motion);
        break;
    case 'c':
        ::SendMessage(hwnd, SCI_CUT, 0, 0);
        enterInsertMode();
        state.recordLastOp(OP_MOTION, count, motion);
        break;
    case 'u': // gu
        Utils::toLower(hwnd, start, end);
        Utils::select(hwnd, start, start);
        break;

    case 'U': // gU
        Utils::toUpper(hwnd, start, end);
        Utils::select(hwnd, start, start);
        break;

    case '~': // g~
        Motion::toggleCase(hwnd, count);
        Utils::select(hwnd, start, start);
        break;
    case '?': Utils::rot13(hwnd, start, end); break;
    }

    Utils::setCurrentRegister('"');
    state.deleteToBlackhole = false;
}

void NormalMode::handlePasteFromRegister(HWND hwnd, char pasteCmd, char reg) {
    std::string content;

    if (reg == '+' || reg == '*') {
        // Get from system clipboard
        if (OpenClipboard(hwnd)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pszText = (char*)GlobalLock(hData);
                if (pszText) {
                    content = pszText;
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
        }
    } else {
        content = Utils::getRegisterContent(reg);
    }

    if (!content.empty()) {
        Utils::beginUndo(hwnd);

        if (pasteCmd == 'p') {
            // Paste after cursor
            Utils::pasteAfter(hwnd, 1, false);
        } else if (pasteCmd == 'P') {
            // Paste before cursor
            Utils::pasteBefore(hwnd, 1, false);
        }

        Utils::endUndo(hwnd);
        state.recordLastOp(OP_PASTE, 1);
    } else {
        Utils::setStatus(TEXT("-- Register empty --"));
    }

    // Reset to default register
    Utils::setCurrentRegister('"');
    state.deleteToBlackhole = false;
}

void NormalMode::handleDeleteCharToRegister(HWND hwnd, char deleteCmd, char reg) {
    Utils::beginUndo(hwnd);

    int pos = Utils::caretPos(hwnd);
    int docLen = ::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);

    if (deleteCmd == 'x') {
        // Delete character under cursor
        if (pos >= docLen) {
            Utils::endUndo(hwnd);
            return;
        }

        int next = ::SendMessage(hwnd, SCI_POSITIONAFTER, pos, 0);
        std::string text = Utils::getTextRange(hwnd, pos, next);

        // Store in register
        if (reg != '_') {  // Skip blackhole register
            Utils::storeRegister(reg, text.c_str());
        }

        ::SendMessage(hwnd, SCI_SETSEL, pos, next);
        ::SendMessage(hwnd, SCI_CLEAR, 0, 0);

    } else if (deleteCmd == 'X') {
        // Delete character before cursor
        if (pos <= 0) {
            Utils::endUndo(hwnd);
            return;
        }

        int prev = ::SendMessage(hwnd, SCI_POSITIONBEFORE, pos, 0);
        std::string text = Utils::getTextRange(hwnd, prev, pos);
        // Store in register
        if (reg != '_') {  // Skip blackhole register
            Utils::storeRegister(reg, text.c_str());
        }

        ::SendMessage(hwnd, SCI_SETSEL, prev, pos);
        ::SendMessage(hwnd, SCI_CLEAR, 0, 0);
    }

    Utils::endUndo(hwnd);
    state.recordLastOp(OP_MOTION, 1, deleteCmd);

    Utils::setCurrentRegister('"');
    state.deleteToBlackhole = false;
}

void NormalMode::gotoDefinition(HWND h, VimState& state, bool applyOp) {
    int pos = Utils::caretPos(h);
    auto bounds = Utils::findWordBounds(h, pos);

    if (bounds.first == bounds.second) return;

    std::string text = Utils::getTextRange(h, bounds.first, bounds.second);
    int len = (int)text.size();

    int searchEnd = bounds.first;

    ::SendMessage(h, SCI_SETTARGETSTART, 0, 0);
    ::SendMessage(h, SCI_SETTARGETEND, searchEnd, 0);
    ::SendMessage(h, SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE | SCFIND_WHOLEWORD, 0);

    int found = ::SendMessage(h, SCI_SEARCHINTARGET, len, (LPARAM)text.c_str());

    if (found == -1) {
        Utils::setStatus(TEXT("Pattern not found"));
        return;
    }

    if (!applyOp) {
        ::SendMessage(h, SCI_GOTOPOS, found, 0);
        return;
    }

    int start = found;
    int end = found + len;

    Utils::select(h, start, end);

    switch (state.opPending) {
        case 'd': ::SendMessage(h, SCI_CUT, 0, 0); break;
        case 'y': ::SendMessage(h, SCI_COPY, 0, 0); break;
        case 'c':
            ::SendMessage(h, SCI_CUT, 0, 0);
            NormalMode::enterInsertMode();
            break;
    }
}