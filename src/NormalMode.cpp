#include "../include/NormalMode.h"
#include "../include/CommandMode.h"
#include "../include/VisualMode.h"
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
    
    k.motion("h", 'h', [](HWND h, int c) { Motion::charLeft(h, c); })
     .motion("j", 'j', [](HWND h, int c) { Motion::lineDown(h, c); })
     .motion("k", 'k', [](HWND h, int c) { Motion::lineUp(h, c); })
     .motion("l", 'l', [](HWND h, int c) { Motion::charRight(h, c); })
     .motion("w", 'w', [](HWND h, int c) { Motion::wordRight(h, c); })
     .motion("W", 'W', [](HWND h, int c) { Motion::wordRightBig(h, c); })
     .motion("b", 'b', [](HWND h, int c) { Motion::wordLeft(h, c); })
     .motion("B", 'B', [](HWND h, int c) { Motion::wordLeftBig(h, c); })
     .motion("e", 'e', [](HWND h, int c) { Motion::wordEnd(h, c); })
     .motion("E", 'E', [](HWND h, int c) { Motion::wordEndBig(h, c); })
     .motion("0", '0', [](HWND h, int c) { Motion::lineStart(h, 1); })
     .motion("$", '$', [](HWND h, int c) { Motion::lineEnd(h, c); })
     .motion("^", '^', [](HWND h, int c) { Motion::lineStart(h, c); })
     .motion("{", '{', [](HWND h, int c) { Motion::paragraphUp(h, c); })
     .motion("}", '}', [](HWND h, int c) { Motion::paragraphDown(h, c); })
     .motion("%", '%', [](HWND h, int c) {
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         int match = ::SendMessage(h, SCI_BRACEMATCH, pos, 0);
         if (match != -1) ::SendMessage(h, SCI_GOTOPOS, match, 0);
     })
     .motion("H", 'H', [](HWND h, int c) { Motion::pageUp(h); })
     .motion("L", 'L', [](HWND h, int c) { Motion::pageDown(h); })
     .motion("G", 'G', [](HWND h, int c) {
         if (c == 1) Motion::documentEnd(h);
         else Motion::gotoLine(h, c);
     })
     .set("gg", [this](HWND h, int c) {
         long pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
         state.recordJump(pos, line);
         if (c > 1) Motion::gotoLine(h, c);
         else Motion::documentStart(h);
         int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         ::SendMessage(h, SCI_SETSEL, caret, caret);
         state.recordLastOp(OP_MOTION, c, 'g');
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
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; ++i) deleteLineOnce(h);
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_DELETE_LINE, c);
     })
     .set("yy", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; ++i) yankLineOnce(h);
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_YANK_LINE, c);
     })
     .set("cc", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; ++i) deleteLineOnce(h);
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         enterInsertMode();
         state.recordLastOp(OP_MOTION, c, 'c');
     })
     .set("D", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; i++) {
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
             int end = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
             if (pos < end) {
                 ::SendMessage(h, SCI_SETSEL, pos, end);
                 ::SendMessage(h, SCI_CLEAR, 0, 0);
             }
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_MOTION, c, 'D');
     })
     .set("C", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; i++) {
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
             int end = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
             if (pos < end) {
                 ::SendMessage(h, SCI_SETSEL, pos, end);
                 ::SendMessage(h, SCI_CLEAR, 0, 0);
             }
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         enterInsertMode();
     });
    
    k.set("x", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; ++i) {
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int docLen = ::SendMessage(h, SCI_GETTEXTLENGTH, 0, 0);
             if (pos >= docLen) break;
             int next = ::SendMessage(h, SCI_POSITIONAFTER, pos, 0);
             ::SendMessage(h, SCI_SETSEL, pos, next);
             ::SendMessage(h, SCI_CLEAR, 0, 0);
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_MOTION, c, 'x');
     })
     .set("X", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; ++i) {
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             if (pos <= 0) break;
             int prev = ::SendMessage(h, SCI_POSITIONBEFORE, pos, 0);
             ::SendMessage(h, SCI_SETSEL, prev, pos);
             ::SendMessage(h, SCI_CLEAR, 0, 0);
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
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
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         for (int i = 0; i < c; ++i) {
             int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
             int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
             if (line >= total - 1) break;
             int endLine = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
             int nextLine = ::SendMessage(h, SCI_POSITIONFROMLINE, line + 1, 0);
             ::SendMessage(h, SCI_SETSEL, endLine, nextLine);
             ::SendMessage(h, SCI_REPLACESEL, 0, (LPARAM)" ");
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_MOTION, c, 'J');
     });
    
    k.set("p", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         if (::SendMessage(h, SCI_CANPASTE, 0, 0)) {
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
             bool isLine = (state.lastOp.type == OP_YANK_LINE || state.lastOp.type == OP_DELETE_LINE);
             if (isLine) {
                 int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
                 if (line == total - 1) {
                     int end = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                     ::SendMessage(h, SCI_GOTOPOS, end, 0);
                     ::SendMessage(h, SCI_NEWLINE, 0, 0);
                 }
                 int next = ::SendMessage(h, SCI_POSITIONFROMLINE, line + 1, 0);
                 ::SendMessage(h, SCI_GOTOPOS, next, 0);
             }
             for (int i = 0; i < c; i++) ::SendMessage(h, SCI_PASTE, 0, 0);
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_PASTE, c);
     })
     .set("P", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         if (::SendMessage(h, SCI_CANPASTE, 0, 0)) {
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
             bool isLine = (state.lastOp.type == OP_YANK_LINE || state.lastOp.type == OP_DELETE_LINE);
             if (isLine) {
                 int start = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
                 ::SendMessage(h, SCI_GOTOPOS, start, 0);
             }
             for (int i = 0; i < c; i++) ::SendMessage(h, SCI_PASTE, 0, 0);
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_PASTE, c);
     });
    
    k.set("/", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter('/'); })
     .set("?", [](HWND h, int c) { if (g_commandMode) g_commandMode->enter('?'); })
     .set("n", [this](HWND h, int c) {
         if (g_commandMode) {
             long pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
             state.recordJump(pos, line);
             for (int i = 0; i < c; i++) g_commandMode->searchNext(h);
             state.recordLastOp(OP_MOTION, c, 'n');
         }
     })
     .set("N", [this](HWND h, int c) {
         if (g_commandMode) {
             long pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
             state.recordJump(pos, line);
             for (int i = 0; i < c; i++) g_commandMode->searchPrevious(h);
             state.recordLastOp(OP_MOTION, c, 'N');
         }
     })
     .set("*", [this](HWND h, int c) {
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
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
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
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
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         ::SendMessage(h, SCI_SETSEL, pos, pos);
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
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         ::SendMessage(h, SCI_SETSEL, pos, pos);
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
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
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
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.repeatCount = 0;
     });
    
    k.set("zz", [this](HWND h, int c) {
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
         ::SendMessage(h, SCI_SETFIRSTVISIBLELINE,
             line - (::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2), 0);
         state.recordLastOp(OP_MOTION, c, 'z');
     });
    
    k.set("m", [this](HWND h, int c) {
         state.awaitingMarkSet = true;
         Utils::setStatus(TEXT("-- Set mark --"));
     })
     .set("`", [this](HWND h, int c) {
         state.awaitingMarkJump = true;
         state.isBacktickJump = true;
         state.pendingJumpCount = c;
         Utils::setStatus(TEXT("-- Jump to mark (exact) --"));
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
     .set("``", [this](HWND h, int c) {
         if (state.jumpList.size() < 2) return;
         int last = state.jumpList.size() - 1;
         std::swap(state.jumpList[last], state.jumpList[last - 1]);
         auto jump = state.jumpList[last];
         if (jump.position != -1) {
             ::SendMessage(h, SCI_GOTOPOS, jump.position, 0);
             ::SendMessage(h, SCI_SETSEL, jump.position, jump.position);
             ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
         }
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
     .set("<", [](HWND h, int c) { Utils::handleIndent(h, c); })
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
    
    int caret = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    ::SendMessage(hwnd, SCI_SETSEL, caret, caret);
    Utils::clearSearchHighlights(hwnd);
}

void NormalMode::enterInsertMode() {
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
    
    if (state.textObjectPending && (state.opPending == 'd' || state.opPending == 'c' || state.opPending == 'y')) {
        char modifier = state.textObjectPending;
        char op = state.opPending;
        
        state.textObjectPending = 0;
        state.opPending = 0;
        
        TextObject textObj;
        textObj.apply(hwnd, state, op, modifier, c);
        state.repeatCount = 0;
        return;
    }
    
    if (state.opPending && !state.textObjectPending) {
        if (c == 'i' || c == 'a') {
            state.textObjectPending = c;
            return;
        }
        
        if (c == 'w' || c == 'W' || c == 'b' || c == 'B' || c == 'e' || c == 'E' ||
            c == 'h' || c == 'l' || c == 'j' || c == 'k' || 
            c == '$' || c == '^' || c == '0' || c == 'G' || c == '%' ||
            c == '{' || c == '}') {
            
            int count = (state.repeatCount > 0) ? state.repeatCount : 1;
            ::SendMessage(hwnd, SCI_BEGINUNDOACTION, 0, 0);
            applyOperatorToMotion(hwnd, state.opPending, c, count);
            ::SendMessage(hwnd, SCI_ENDUNDOACTION, 0, 0);
            
            state.opPending = 0;
            state.repeatCount = 0;
            return;
        }
    }
    
    if (g_normalKeymap) {
        g_normalKeymap->handleKey(hwnd, c);
    }
}

void NormalMode::handleCharSearchInput(HWND hwnd, char searchChar, char searchType, int count) {
    bool isTill = (state.textObjectPending == 't');
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
    
    state.recordLastOp(OP_MOTION, count,
        isForward ? (isTill ? 't' : 'f') : (isTill ? 'T' : 'F'),
        searchChar);
    
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
        long pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
        int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
        state.recordJump(pos, line);
        
        if (Marks::jumpToMark(hwnd, mark, exactPosition)) {
            long newPos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
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
    int pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    int docLen = ::SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0);
    if (pos < docLen) {
        char currentChar = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (currentChar != '\r' && currentChar != '\n') {
            ::SendMessage(hwnd, SCI_SETSEL, pos, pos + 1);
            std::string repl(1, replaceChar);
            ::SendMessage(hwnd, SCI_REPLACESEL, 0, (LPARAM)repl.c_str());
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, pos, 0);
        }
    }
    state.replacePending = false;
    state.recordLastOp(OP_REPLACE, 1, 'r');
    Utils::setStatus(TEXT("-- NORMAL --"));
}

void NormalMode::deleteLineOnce(HWND hwnd) {
    int pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
    int start = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
    int total = ::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
    int end = (line < total - 1)
        ? ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line + 1, 0)
        : ::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0) + 1;
    
    ::SendMessage(hwnd, SCI_SETSEL, start, end);
    ::SendMessage(hwnd, SCI_CUT, 0, 0);
    
    int newPos = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, newPos, 0);
}

void NormalMode::yankLineOnce(HWND hwnd) {
    int pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
    int start = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
    int total = ::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
    int end = (line < total - 1)
        ? ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line + 1, 0)
        : ::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0);
    
    ::SendMessage(hwnd, SCI_SETSEL, start, end);
    ::SendMessage(hwnd, SCI_COPY, 0, 0);
    ::SendMessage(hwnd, SCI_SETSEL, pos, pos);
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
    
    int start = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    
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
    
    int end = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    if (start > end) std::swap(start, end);
    
    if (motion == 'e' || motion == 'E') {
        int docLen = ::SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0);
        if (end < docLen) end++;
    }
    
    ::SendMessage(hwnd, SCI_SETSEL, start, end);
    switch (op) {
    case 'd':
        ::SendMessage(hwnd, SCI_CUT, 0, 0);
        ::SendMessage(hwnd, SCI_SETCURRENTPOS, start, 0);
        state.recordLastOp(OP_MOTION, count, motion);
        break;
    case 'y':
        ::SendMessage(hwnd, SCI_COPY, 0, 0);
        ::SendMessage(hwnd, SCI_SETSEL, start, start);
        state.recordLastOp(OP_MOTION, count, motion);
        break;
    case 'c':
        ::SendMessage(hwnd, SCI_CUT, 0, 0);
        enterInsertMode();
        state.recordLastOp(OP_MOTION, count, motion);
        break;
    }
}