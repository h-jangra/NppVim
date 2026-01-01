// NormalMode.cpp
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include "../include/NormalMode.h"
#include "../include/CommandMode.h"
#include "../include/VisualMode.h"
#include "../include/Keymap.h"
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
     .motion("w", 'w', "", [](HWND h, int c) { Motion::wordRight(h, c); })
     .motion("W", 'W', "", [](HWND h, int c) { Motion::wordRightBig(h, c); })
     .motion("b", 'b', "", [](HWND h, int c) { Motion::wordLeft(h, c); })
     .motion("B", 'B', "", [](HWND h, int c) { Motion::wordLeftBig(h, c); })
     .motion("e", 'e', "", [](HWND h, int c) { Motion::wordEnd(h, c); })
     .motion("E", 'E', "", [](HWND h, int c) { Motion::wordEndBig(h, c); })
     .motion("0", '0', "", [](HWND h, int c) { Motion::lineStart(h, 1); })
     .motion("$", '$', "", [](HWND h, int c) { Motion::lineEnd(h, c); })
     .motion("^", '^', "", [](HWND h, int c) { Motion::lineStart(h, c); })
     .motion("{", '{', [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::paragraphUp(h, c);
    })
     .motion("}", '}', [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::paragraphDown(h, c);
     })
     .motion(")", ')', [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            int pos = Utils::caretPos(h);
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, 
                Utils::caretLine(h), 0);
            if (pos < lineEnd) {
                ::SendMessage(h, SCI_GOTOPOS, lineEnd, 0);
            } else {
                ::SendMessage(h, SCI_LINEDOWN, 0, 0);
                ::SendMessage(h, SCI_VCHOME, 0, 0);
            }
        }
    })
    .motion("(", '(', [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            int pos = Utils::caretPos(h);
            int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, 
                Utils::caretLine(h), 0);
            if (pos > lineStart) {
                ::SendMessage(h, SCI_GOTOPOS, lineStart, 0);
            } else {
                ::SendMessage(h, SCI_LINEUP, 0, 0);
                ::SendMessage(h, SCI_VCHOME, 0, 0);
            }
        }
    })
     .motion("%", '%', [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        int match = ::SendMessage(h, SCI_BRACEMATCH, pos, 0);
        if (match != -1) ::SendMessage(h, SCI_GOTOPOS, match, 0);
     })
     .motion("H", 'H', [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::pageUp(h);
    })
     .motion("L", 'L', [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
        Motion::pageDown(h);
     })
    .set("M", [](HWND h, int c) {
        int firstVisible = ::SendMessage(h, SCI_GETFIRSTVISIBLELINE, 0, 0);
        int linesOnScreen = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0);
        int middleLine = firstVisible + linesOnScreen / 2;
        ::SendMessage(h, SCI_GOTOLINE, middleLine, 0);
        ::SendMessage(h, SCI_VCHOME, 0, 0);
    })
     .motion("G", 'G', [this](HWND h, int c) {
        long pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        state.recordJump(pos, line);
         if (c == 1) Motion::documentEnd(h);
         else Motion::gotoLine(h, c);
     })
     .set("gg", [this](HWND h, int c) {
         long pos = Utils::caretPos(h);
         int line = Utils::caretLine(h);
         state.recordJump(pos, line);
         if (c > 1) Motion::gotoLine(h, c);
         else Motion::documentStart(h);
         int caret = Utils::caretPos(h);
         ::SendMessage(h, SCI_SETSEL, caret, caret);
         state.recordLastOp(OP_MOTION, c, 'g');
     })
     .set("ge", [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            int pos = Utils::caretPos(h);
            int start = ::SendMessage(h, SCI_WORDSTARTPOSITION, pos, 1);
            if (start > 0) {
                int prev = ::SendMessage(h, SCI_POSITIONBEFORE, start, 0);
                int prevWordStart = ::SendMessage(h, SCI_WORDSTARTPOSITION, prev, 1);
                int prevWordEnd = ::SendMessage(h, SCI_WORDENDPOSITION, prev, 1);
                if (prevWordStart < prevWordEnd) {
                    ::SendMessage(h, SCI_GOTOPOS, prevWordEnd, 0);
                }
            }
        }
    })
    .set("gE", [](HWND h, int c) {
        for (int i = 0; i < c; i++) {
            int pos = Utils::caretPos(h);
            if (pos > 0) {
                int prev = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
                while (prev > 0) {
                    char ch = ::SendMessage(h, SCI_GETCHARAT, prev, 0);
                    if (std::isalnum(ch) || ch == '_') break;
                    prev = ::SendMessage(h, SCI_POSITIONBEFORE, prev, 0);
                }
                if (prev > 0) {
                    int start = prev;
                    while (start > 0) {
                        char ch = ::SendMessage(h, SCI_GETCHARAT, start - 1, 0);
                        if (!std::isalnum(ch) && ch != '_') break;
                        start--;
                    }
                    ::SendMessage(h, SCI_GOTOPOS, prev, 0);
                }
            }
        }
    })
    .set("gf", [this](HWND h, int c) {
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
            std::vector<char> pathBytes(right - left + 1);
            Sci_TextRangeFull tr;
            tr.chrg.cpMin = left;
            tr.chrg.cpMax = right;
            tr.lpstrText = pathBytes.data();
            ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
            
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, pathBytes.data(), -1, NULL, 0);
            if (wideLen > 0) {
                std::vector<wchar_t> pathWide(wideLen);
                MultiByteToWideChar(CP_UTF8, 0, pathBytes.data(), -1, pathWide.data(), wideLen);
                
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
    .set("gx", [this](HWND h, int c) {
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
            std::vector<char> urlBytes(right - left + 1);
            Sci_TextRangeFull tr;
            tr.chrg.cpMin = left;
            tr.chrg.cpMax = right;
            tr.lpstrText = urlBytes.data();
            ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
            
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, urlBytes.data(), -1, NULL, 0);
            if (wideLen > 0) {
                std::vector<wchar_t> urlWide(wideLen);
                MultiByteToWideChar(CP_UTF8, 0, urlBytes.data(), -1, urlWide.data(), wideLen);
                ShellExecuteW(NULL, L"open", urlWide.data(), NULL, NULL, SW_SHOWNORMAL);
            }
        }
    });
    
    k.set("i", [this](HWND h, int c) { enterInsertMode(); })
     .set("a", [this](HWND h, int c) { Motion::charRight(h, 1); enterInsertMode(); })
     .set("A", [this](HWND h, int c) { ::SendMessage(h, SCI_LINEEND, 0, 0); enterInsertMode(); })
     .set("I", [this](HWND h, int c) { ::SendMessage(h, SCI_VCHOME, 0, 0); enterInsertMode(); })
     .set("o", [this](HWND h, int c) {
         ::SendMessage(h, SCI_LINEEND, 0, 0);
         ::SendMessage(h, SCI_NEWLINE, 0, 0);
         enterInsertMode();
     })
     .set("O", [this](HWND h, int c) {
         ::SendMessage(h, SCI_HOME, 0, 0);
         ::SendMessage(h, SCI_NEWLINE, 0, 0);
         Motion::lineUp(h, 1);
         enterInsertMode();
     });
    
    k.set("d", [this](HWND h, int c) { state.opPending = 'd'; })
     .set("y", [this](HWND h, int c) { state.opPending = 'y'; })
     .set("c", [this](HWND h, int c) { state.opPending = 'c'; })
     .set("dd", [this](HWND h, int c) {
         state.lastYankLinewise = true;
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) deleteLineOnce(h);
         Utils::endUndo(h);
         state.recordLastOp(OP_DELETE_LINE, c);
     })
     .set("yy", [this](HWND h, int c) {
         state.lastYankLinewise = true;
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) yankLineOnce(h);
         Utils::endUndo(h);
         state.recordLastOp(OP_YANK_LINE, c);
     })
     .set("cc", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) deleteLineOnce(h);
         Utils::endUndo(h);
         enterInsertMode();
         state.recordLastOp(OP_MOTION, c, 'c');
     })
     .set("D", [this](HWND h, int c) {
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
     .set("C", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; i++) {
             int pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             int end = Utils::lineEnd(h, line);
             if (pos < end) {
                 ::SendMessage(h, SCI_SETSEL, pos, end);
                 ::SendMessage(h, SCI_CLEAR, 0, 0);
             }
         }
         Utils::endUndo(h);
         enterInsertMode();
     });
    
    k.set("x", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) {
             int pos = Utils::caretPos(h);
             int docLen = ::SendMessage(h, SCI_GETTEXTLENGTH, 0, 0);
             if (pos >= docLen) break;
             int next = ::SendMessage(h, SCI_POSITIONAFTER, pos, 0);
             ::SendMessage(h, SCI_SETSEL, pos, next);
             ::SendMessage(h, SCI_CLEAR, 0, 0);
         }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'x');
     })
     .set("X", [this](HWND h, int c) {
         Utils::beginUndo(h);
         for (int i = 0; i < c; ++i) {
             int pos = Utils::caretPos(h);
             if (pos <= 0) break;
             int prev = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
             ::SendMessage(h, SCI_SETSEL, prev, pos);
             ::SendMessage(h, SCI_CLEAR, 0, 0);
         }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'X');
     })
     .set("r", [this](HWND h, int c) {
         state.replacePending = true;
         Utils::setStatus(TEXT("-- REPLACE CHAR --"));
     })
     .set("R", [this](HWND h, int c) {
         state.mode = INSERT;
         Utils::setStatus(TEXT("-- REPLACE --"));
         ::SendMessage(h, SCI_SETOVERTYPE, true, 0);
         ::SendMessage(h, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
         state.recordLastOp(OP_MOTION, c, 'R');
     })
     .motion("~", '~', [this](HWND h, int c) { motion.toggleCase(h, c); });
    
    k.set("J", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::joinLines(h, Utils::caretLine(h), c, true);
        Utils::endUndo(h);
        state.recordLastOp(OP_MOTION, c, 'J');
     });
    
    k.set("p", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::pasteAfter(h, c, state.lastYankLinewise);
        Utils::endUndo(h);
        state.recordLastOp(OP_PASTE, c);
     })
     .set("P", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::pasteBefore(h, c, state.lastYankLinewise);
        Utils::endUndo(h);
        state.recordLastOp(OP_PASTE, c);
     });
    
    k.set("/", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter('/'); })
     .set("?", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter('?'); })
     .set("n", [this](HWND h, int c) {
         if (g_commandMode) {
             long pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             state.recordJump(pos, line);
             for (int i = 0; i < c; i++) g_commandMode->searchNext(h);
             state.recordLastOp(OP_MOTION, c, 'n');
         }
     })
     .set("N", [this](HWND h, int c) {
         if (g_commandMode) {
             long pos = Utils::caretPos(h);
             int line = Utils::caretLine(h);
             state.recordJump(pos, line);
             for (int i = 0; i < c; i++) g_commandMode->searchPrevious(h);
             state.recordLastOp(OP_MOTION, c, 'N');
         }
     })
     .set("*", [this](HWND h, int c) {
         int pos = Utils::caretPos(h);
         auto bounds = Utils::findWordBounds(h, pos);
         if (bounds.first != bounds.second && g_commandMode) {
             int len = bounds.second - bounds.first;
             std::vector<char> word(len + 1);
             Sci_TextRangeFull tr;
             tr.chrg.cpMin = bounds.first;
             tr.chrg.cpMax = bounds.second;
             tr.lpstrText = word.data();
             ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
             g_commandMode->performSearch(h, word.data(), false);
             g_commandMode->searchNext(h);
             state.recordLastOp(OP_MOTION, c, '*');
         }
     })
     .set("#", [this](HWND h, int c) {
         int pos = Utils::caretPos(h);
         auto bounds = Utils::findWordBounds(h, pos);
         if (bounds.first != bounds.second && g_commandMode) {
             int len = bounds.second - bounds.first;
             std::vector<char> word(len + 1);
             Sci_TextRangeFull tr;
             tr.chrg.cpMin = bounds.first;
             tr.chrg.cpMax = bounds.second;
             tr.lpstrText = word.data();
             ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
             g_commandMode->performSearch(h, word.data(), false);
             g_commandMode->searchPrevious(h);
             state.recordLastOp(OP_MOTION, c, '#');
         }
     });
    
    k.set("f", [this](HWND h, int c) {
         state.opPending = 'f';
         state.textObjectPending = 'f';
         Utils::setStatus(TEXT("-- find char (forward) --"));
     })
     .set("F", [this](HWND h, int c) {
         state.opPending = 'F';
         state.textObjectPending = 'f';
         Utils::setStatus(TEXT("-- find char (backward) --"));
     })
     .set("t", [this](HWND h, int c) {
         state.opPending = 't';
         state.textObjectPending = 't';
         Utils::setStatus(TEXT("-- till char (forward) --"));
     })
     .set("T", [this](HWND h, int c) {
         state.opPending = 'T';
         state.textObjectPending = 't';
         Utils::setStatus(TEXT("-- till char (backward) --"));
     })
     .set(";", [this](HWND h, int c) {
         if (state.lastSearchChar == 0) return;
         bool fwd = state.lastSearchForward;
         bool till = state.lastSearchTill;
         char ch = state.lastSearchChar;
         if (till) {
             if (fwd) Motion::tillChar(h, c, ch);
             else Motion::tillCharBack(h, c, ch);
         } else {
             if (fwd) Motion::nextChar(h, c, ch);
             else Motion::prevChar(h, c, ch);
         }
         state.recordLastOp(OP_MOTION, c, ';');
         int pos = Utils::caretPos(h);
         Utils::select(h, pos, pos);
     })
     .set(",", [this](HWND h, int c) {
         if (state.lastSearchChar == 0) return;
         bool fwd = state.lastSearchForward;
         bool till = state.lastSearchTill;
         char ch = state.lastSearchChar;
         if (till) {
             if (fwd) Motion::tillCharBack(h, c, ch);
             else Motion::tillChar(h, c, ch);
         } else {
             if (fwd) Motion::prevChar(h, c, ch);
             else Motion::nextChar(h, c, ch);
         }
         state.recordLastOp(OP_MOTION, c, ',');
         int pos = Utils::caretPos(h);
         Utils::select(h, pos, pos);
     });
    
    k.set("v", [](HWND h, int c) { if (g_visualMode) g_visualMode->enterChar(h); })
     .set("V", [](HWND h, int c) { if (g_visualMode) g_visualMode->enterLine(h); });
    
    k.set(":", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter(':'); })
     .set("u", [this](HWND h, int c) {
         ::SendMessage(h, SCI_UNDO, 0, 0);
         state.recordLastOp(OP_MOTION, 1, 'u');
     })
     .set(".", [this](HWND h, int c) {
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
             for (int i = 0; i < rc; ++i) ::SendMessage(h, SCI_PASTE, 0, 0);
             break;
         case OP_MOTION:
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
         }
         Utils::endUndo(h);
         state.repeatCount = 0;
     });
    
    k.set("zz", [this](HWND h, int c) {
         int pos = Utils::caretPos(h);
         int line = Utils::caretLine(h);
         ::SendMessage(h, SCI_SETFIRSTVISIBLELINE,
             line - (::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2), 0);
         state.recordLastOp(OP_MOTION, c, 'z');
     })
    .set("zt", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line, 0);
    })
    .set("zb", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int linesOnScreen = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line - linesOnScreen + 1, 0);
    })
    .set("zo", [](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION, 
            Utils::caretPos(h), 0);
        for (int i = 0; i < c; i++) {
            ::SendMessage(h, SCI_FOLDLINE, line, SC_FOLDACTION_EXPAND);
        }
    })
    .set("zc", [](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION, 
            Utils::caretPos(h), 0);
        for (int i = 0; i < c; i++) {
            ::SendMessage(h, SCI_FOLDLINE, line, SC_FOLDACTION_CONTRACT);
        }
    });
    
    k.set("m", [this](HWND h, int c) {
         state.awaitingMarkSet = true;
         Utils::setStatus(TEXT("-- Set mark --"));
     })
    .set("``", [this](HWND h, int c) {
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
     .set("'", [this](HWND h, int c) {
         if (c > 1) {
             if (state.jumpList.size() >= 2) {
                 int last = state.jumpList.size() - 1;
                 std::swap(state.jumpList[last], state.jumpList[last - 1]);
                 auto jump = state.jumpList[last];
                 if (jump.lineNumber != -1) {
                     int start = ::SendMessage(h, SCI_POSITIONFROMLINE, jump.lineNumber, 0);
                     int end = ::SendMessage(h, SCI_GETLINEENDPOSITION, jump.lineNumber, 0);
                     int target = start;
                     while (target < end) {
                         char ch = ::SendMessage(h, SCI_GETCHARAT, target, 0);
                         if (!std::isspace((unsigned char)ch)) break;
                         target++;
                     }
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
     .set("''", [this](HWND h, int c) {
         if (state.jumpList.size() < 2) return;
         int last = state.jumpList.size() - 1;
         std::swap(state.jumpList[last], state.jumpList[last - 1]);
         auto jump = state.jumpList[last];
         if (jump.lineNumber != -1) {
             int start = ::SendMessage(h, SCI_POSITIONFROMLINE, jump.lineNumber, 0);
             int end = ::SendMessage(h, SCI_GETLINEENDPOSITION, jump.lineNumber, 0);
             int target = start;
             while (target < end) {
                 char ch = ::SendMessage(h, SCI_GETCHARAT, target, 0);
                 if (!std::isspace((unsigned char)ch)) break;
                 target++;
             }
             ::SendMessage(h, SCI_GOTOPOS, target, 0);
             ::SendMessage(h, SCI_SETSEL, target, target);
             ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
         }
     });
    
    k.set(">", [](HWND h, int c) { Utils::handleIndent(h, c); })
     .set("<", [](HWND h, int c) { Utils::handleUnindent(h, c); })
     .set("=", [](HWND h, int c) { Utils::handleAutoIndent(h, c); });
    
    k.set("gcc", [this](HWND h, int c) {
         ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_BLOCK_COMMENT, 0);
         state.recordLastOp(OP_MOTION, c, 'c');
     })
     .set("gt", [this](HWND h, int c) {
         for (int i = 0; i < c; i++)
             ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_NEXT);
         state.recordLastOp(OP_MOTION, c, 't');
     })
     .set("gT", [this](HWND h, int c) {
         for (int i = 0; i < c; i++)
             ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_VIEW_TAB_PREV);
         state.recordLastOp(OP_MOTION, c, 'T');
     })
    //  TODO: gv to restore last visual selection
    .set("gv", [this](HWND h, int c) {
        if (state.lastVisualAnchor < 0 || state.lastVisualCaret < 0) return;

        state.restoringVisual = true;

        int a = state.lastVisualAnchor;
        int b = state.lastVisualCaret;

        if (state.lastVisualWasBlock) {
            g_visualMode->enterBlock(h);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, a, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, b, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, b, 0);
        }
        else if (state.lastVisualWasLine) {
            g_visualMode->enterLine(h);

            int la = ::SendMessage(h, SCI_LINEFROMPOSITION, a, 0);
            int lb = ::SendMessage(h, SCI_LINEFROMPOSITION, b, 0);

            int start = ::SendMessage(h, SCI_POSITIONFROMLINE, (std::min)(la, lb), 0);
            int end   = ::SendMessage(h, SCI_POSITIONFROMLINE, (std::max)(la, lb) + 1, 0);

            Utils::select(h, start, end);
            ::SendMessage(h, SCI_SETCURRENTPOS, b, 0);
        }
        else {
            g_visualMode->enterChar(h);
            ::SendMessage(h, SCI_SETANCHOR, a, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, b, 0);
        }
    })
     .set("gi", [this](HWND h, int c) {
        if (state.lastInsertPos == -1) return;
        ::SendMessage(h, SCI_GOTOPOS, state.lastInsertPos, 0);
        enterInsertMode();
    });
    
    k.set("[ ", [](HWND h, int c) {
         ::SendMessage(h, SCI_HOME, 0, 0);
         ::SendMessage(h, SCI_NEWLINE, 0, 0);
         Motion::lineUp(h, 1);
     })
     .set("] ", [](HWND h, int c) {
         ::SendMessage(h, SCI_LINEEND, 0, 0);
         ::SendMessage(h, SCI_NEWLINE, 0, 0);
     });

    k.set("gUU", [this](HWND h, int c) {
        int line = Utils::caretLine(h);
        int last = Utils::lineCount(h) - 1;
        for (int i = 0; i < c && line + i <= last; i++) {
            int start = Utils::lineStart(h, line + i);
            int end = Utils::lineEnd(h, line + i);
            Utils::toUpper(h, start, end);
        }
        Utils::endUndo(h);
    })
    .set("guu", [this](HWND h, int c) {
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

    // Increment/decrement numbers
    k.set("\x01", [this](HWND h, int c) { // Ctrl+A
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int lineStart = Utils::lineStart(h, line);
        int lineEnd = Utils::lineEnd(h, line);
        
        int numStart = pos;
        while (numStart > lineStart) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, numStart - 1, 0);
            if (!std::isdigit(ch) && ch != '-') break;
            numStart--;
        }
        
        int numEnd = pos;
        while (numEnd < lineEnd) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, numEnd, 0);
            if (!std::isdigit(ch)) break;
            numEnd++;
        }
        
        if (numStart < numEnd) {
            std::vector<char> buffer(numEnd - numStart + 1);
            Sci_TextRangeFull tr;
            tr.chrg.cpMin = numStart;
            tr.chrg.cpMax = numEnd;
            tr.lpstrText = buffer.data();
            ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
            
            try {
                int num = std::stoi(buffer.data());
                num += c;
                std::string newNum = std::to_string(num);
                ::SendMessage(h, SCI_SETTARGETRANGE, numStart, numEnd);
                ::SendMessage(h, SCI_REPLACETARGET, newNum.length(), (LPARAM)newNum.c_str());
            } catch (...) {}
        }
    })
    .set("\x18", [this](HWND h, int c) { // Ctrl+X
        g_normalKeymap->handleKey(h, '\x01'); // Reuse Ctrl+A logic with negative count
    });

    // Undo/Redo
    k.set("\x12", [this](HWND h, int c) { // Ctrl+R - Redo
        for (int i = 0; i < c; i++) {
            ::SendMessage(h, SCI_REDO, 0, 0);
        }
        state.recordLastOp(OP_MOTION, c, 'R');
    });

    // Search from beginning for first occurrence
    k.set("gd", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        auto bounds = Utils::findWordBounds(h, pos);
        if (bounds.first != bounds.second) {
            int len = bounds.second - bounds.first;
            std::vector<char> word(len + 1);
            Sci_TextRangeFull tr;
            tr.chrg.cpMin = bounds.first;
            tr.chrg.cpMax = bounds.second;
            tr.lpstrText = word.data();
            ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
 
            ::SendMessage(h, SCI_SETTARGETRANGE, 0, pos);
            ::SendMessage(h, SCI_SETSEARCHFLAGS, SCFIND_WHOLEWORD, 0);
            int found = ::SendMessage(h, SCI_SEARCHINTARGET, len, (LPARAM)word.data());
            if (found != -1) {
                ::SendMessage(h, SCI_GOTOPOS, found, 0);
            }
        }
    });

    // Auto-indent
    k.set("==", [this](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION, Utils::caretPos(h), 0);
        for (int i = 0; i < c; i++) {
            if (line + i < ::SendMessage(h, SCI_GETLINECOUNT, 0, 0)) {
                ::SendMessage(h, SCI_SETSEL, 
                    ::SendMessage(h, SCI_POSITIONFROMLINE, line + i, 0),
                    ::SendMessage(h, SCI_GETLINEENDPOSITION, line + i, 0));
                Utils::handleAutoIndent(h, 1);
            }
        }
    });

    // Scroll without moving cursor
    k.set("\x05", [](HWND h, int c) { // Ctrl+E - scroll down
        ::SendMessage(h, SCI_LINESCROLL, 0, c);
    })
    .set("\x19", [](HWND h, int c) { // Ctrl+Y - scroll up
        ::SendMessage(h, SCI_LINESCROLL, 0, -c);
    })
    .set("\x04", [](HWND h, int c) { // Ctrl+D - half page down
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) {
            Motion::lineDown(h, lines);
        }
    })
    .set("\x15", [](HWND h, int c) { // Ctrl+U - half page up
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) {
            Motion::lineUp(h, lines);
        }
    })
    .set("\x06", [](HWND h, int c) { // Ctrl+F - page forward
        for (int i = 0; i < c; i++) {
            Motion::pageDown(h);
        }
    })
    .set("\x02", [](HWND h, int c) { // Ctrl+B - page backward
        for (int i = 0; i < c; i++) {
            Motion::pageUp(h);
        }
    });

    // Join lines without space
    k.set("gJ", [this](HWND h, int c) {
        Utils::beginUndo(h);
        Utils::joinLines(h, Utils::caretLine(h), c, false);
        Utils::endUndo(h);
    });

    // Change inner/around line
    k.set("S", [this](HWND h, int c) {
        Utils::beginUndo(h);
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int start = Utils::lineStart(h, line);
        int end = Utils::lineEnd(h, line);
        Utils::select(h, start, end);
        ::SendMessage(h, SCI_CLEAR, 0, 0);
        Utils::endUndo(h);
        enterInsertMode();
    });

    // Go to column
    k.set("|", [](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int lineStart = Utils::lineStart(h, line);
        int lineEnd = Utils::lineEnd(h, line);
        int targetCol = (c > 0 ? c - 1 : 0);
        int targetPos = lineStart + targetCol;
        if (targetPos > lineEnd) targetPos = lineEnd;
        ::SendMessage(h, SCI_GOTOPOS, targetPos, 0);
    });

    // Insert at beginning/end of line multiple times
    k.set("gI", [this](HWND h, int c) {
        int pos = Utils::caretPos(h);
        int line = Utils::caretLine(h);
        int lineStart = Utils::lineStart(h, line);
        ::SendMessage(h, SCI_GOTOPOS, lineStart, 0);
        enterInsertMode();
    });
}

void NormalMode::enter() {
    HWND hwnd = Utils::getCurrentScintillaHandle();
    
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
        ::SendMessage(hwnd, SCI_SETSEL, caret, caret);
    }

    state.restoringVisual = false;
    Utils::clearSearchHighlights(hwnd);
}

void NormalMode::enterInsertMode() {
    state.lastInsertPos = Utils::caretPos(Utils::getCurrentScintillaHandle());

    HWND hwnd = Utils::getCurrentScintillaHandle();
    state.mode = INSERT;
    state.reset();
    Utils::setStatus(TEXT("-- INSERT --"));
    ::SendMessage(hwnd, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);
}

void NormalMode::handleKey(HWND hwnd, char c) {
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
    
     if (state.opPending && !state.textObjectPending) {
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
    Utils::charSearch(hwnd, state, searchType, searchChar, count);

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
    int len = ::SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0);

    if (pos < len) {
        char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (ch != '\r' && ch != '\n') {
            Utils::replaceChar(hwnd, pos, replaceChar);
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, pos, 0);
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

    Utils::select(hwnd, range.first, range.second);
    ::SendMessage(hwnd, SCI_CUT, 0, 0);

    int newPos = Utils::lineStart(hwnd, line);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, newPos, 0);
}

void NormalMode::yankLineOnce(HWND hwnd) {
    int pos = Utils::caretPos(hwnd);
    int line = Utils::caretLine(hwnd);
    auto range = Utils::lineRange(hwnd, line, true);

    Utils::select(hwnd, range.first, range.second);
    ::SendMessage(hwnd, SCI_COPY, 0, 0);
    Utils::select(hwnd, pos, pos);

    state.recordLastOp(OP_YANK_LINE, 1);
}

void NormalMode::applyOperatorToMotion(HWND hwnd, char op, char motion, int count) {
    if (motion == '(' || motion == ')' || motion == '[' || motion == ']' ||
        motion == '{' || motion == '}' || motion == '<' || motion == '>' ||
        motion == '\'' || motion == '\"' || motion == '`' ||
        motion == 't' || motion == 's' || motion == 'p') {
        
        TextObject textObj;
        textObj.apply(hwnd, state, op, 'a', motion);
        return;
    }
    
    int start = Utils::caretPos(hwnd);
    
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
    case 'G': 
        if (count == 1) Motion::documentEnd(hwnd);
        else Motion::gotoLine(hwnd, count);
        break;
    default: break;
    }
    
    int end = Utils::caretPos(hwnd);
    if (start > end) std::swap(start, end);
    
    if (motion == 'e' || motion == 'E') {
        int docLen = ::SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0);
        if (end < docLen) end++;
    }
    
    Utils::select(hwnd, start, end);
    switch (op) {
    case 'd':
        ::SendMessage(hwnd, SCI_CUT, 0, 0);
        ::SendMessage(hwnd, SCI_SETCURRENTPOS, start, 0);
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
    }
}