#include "../include/VisualMode.h"
#include "../include/NormalMode.h"
#include "../include/CommandMode.h"
#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../plugin/menuCmdID.h"
#include "../plugin/Scintilla.h"
#include <algorithm>
#include <cctype>

extern NormalMode* g_normalMode;
extern CommandMode* g_commandMode;
extern NppData nppData;
extern std::unique_ptr<Keymap> g_normalKeymap;

std::unique_ptr<Keymap> g_visualKeymap;

VisualMode::VisualMode(VimState& state) : state(state) {
    g_visualKeymap = std::make_unique<Keymap>(state);
    setupKeyMaps();
}

bool VisualMode::iswalnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c));
}

void VisualMode::setupKeyMaps() {
    auto& k = *g_visualKeymap;
    
    k.set("d", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         if (state.isBlockVisual) {
             ::SendMessage(h, SCI_CLEAR, 0, 0);
             ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
             int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
             ::SendMessage(h, SCI_SETCURRENTPOS, pos, 0);
             ::SendMessage(h, SCI_SETANCHOR, pos, 0);
         } else {
             ::SendMessage(h, SCI_CLEAR, 0, 0);
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_MOTION, c, 'd');
         exitToNormal(h);
     })
     .set("x", [this](HWND h, int c) { g_visualKeymap->handleKey(h, 'd'); })
     .set("y", [this](HWND h, int c) {
         if (state.isBlockVisual) {
             int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
             int caret = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
             int anchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
             int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
             int anchorCol = anchor - ::SendMessage(h, SCI_POSITIONFROMLINE, anchorLine, 0);
             int caretCol = caret - ::SendMessage(h, SCI_POSITIONFROMLINE, caretLine, 0);
             
             int startLine = (std::min)(anchorLine, caretLine);
             int endLine = (std::max)(anchorLine, caretLine);
             int startCol = (std::min)(anchorCol, caretCol);
             int endCol = (std::max)(anchorCol, caretCol);
             
             std::string clipText;
             for (int line = startLine; line <= endLine; line++) {
                 int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
                 int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                 int colStart = lineStart + startCol;
                 int colEnd = lineStart + endCol;
                 if (colStart > lineEnd) colStart = lineEnd;
                 if (colEnd > lineEnd) colEnd = lineEnd;
                 
                 if (colStart < colEnd) {
                     std::vector<char> buffer(colEnd - colStart + 1);
                     Sci_TextRangeFull tr;
                     tr.chrg.cpMin = colStart;
                     tr.chrg.cpMax = colEnd;
                     tr.lpstrText = buffer.data();
                     ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
                     clipText += buffer.data();
                 }
                 if (line < endLine) clipText += "\r\n";
             }
             
             if (!clipText.empty()) {
                 if (OpenClipboard(h)) {
                     EmptyClipboard();
                     HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, clipText.size() + 1);
                     if (hMem) {
                         char* pMem = (char*)GlobalLock(hMem);
                         memcpy(pMem, clipText.c_str(), clipText.size() + 1);
                         GlobalUnlock(hMem);
                         SetClipboardData(CF_TEXT, hMem);
                     }
                     CloseClipboard();
                 }
             }
             ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
         } else {
             int start = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
             int end = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
             ::SendMessage(h, SCI_COPYRANGE, start, end);
         }
         state.recordLastOp(OP_MOTION, c, 'y');
         exitToNormal(h);
     })
     .set("c", [this](HWND h, int c) {
         ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
         if (state.isBlockVisual) {
             int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
             int caret = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
             int anchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
             int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
             int anchorCol = anchor - ::SendMessage(h, SCI_POSITIONFROMLINE, anchorLine, 0);
             int caretCol = caret - ::SendMessage(h, SCI_POSITIONFROMLINE, caretLine, 0);
             
             int startLine = (std::min)(anchorLine, caretLine);
             int endLine = (std::max)(anchorLine, caretLine);
             int startCol = (std::min)(anchorCol, caretCol);
             
             ::SendMessage(h, SCI_CLEAR, 0, 0);
             ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
             ::SendMessage(h, SCI_CLEARSELECTIONS, 0, 0);
             
             bool first = true;
             for (int line = startLine; line <= endLine; line++) {
                 int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
                 int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                 int pos = lineStart + startCol;
                 if (pos > lineEnd) pos = lineEnd;
                 
                 if (first) {
                     ::SendMessage(h, SCI_SETCURRENTPOS, pos, 0);
                     ::SendMessage(h, SCI_SETANCHOR, pos, 0);
                     first = false;
                 } else {
                     ::SendMessage(h, SCI_ADDSELECTION, pos, pos);
                 }
             }
         } else {
             ::SendMessage(h, SCI_CLEAR, 0, 0);
         }
         ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
         state.recordLastOp(OP_MOTION, c, 'c');
         if (g_normalMode) g_normalMode->enterInsertMode();
     })
     .set("o", [this](HWND h, int c) {
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         int anchor = ::SendMessage(h, SCI_GETANCHOR, 0, 0);
         ::SendMessage(h, SCI_SETCURRENTPOS, anchor, 0);
         ::SendMessage(h, SCI_SETANCHOR, pos, 0);
         if (state.isLineVisual) {
             state.visualAnchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
             state.visualAnchor = ::SendMessage(h, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
         } else {
             state.visualAnchor = anchor;
             state.visualAnchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
         }
         updateSelection(h);
     });
    
    k.set("v", [this](HWND h, int c) {
         if (!state.isLineVisual && !state.isBlockVisual) exitToNormal(h);
         else enterChar(h);
     })
     .set("V", [this](HWND h, int c) {
         if (state.isLineVisual) exitToNormal(h);
         else enterLine(h);
     })
     .set("\x16", [this](HWND h, int c) {
         if (state.isBlockVisual) exitToNormal(h);
         else enterBlock(h);
     });
    
    k.set("I", [this](HWND h, int c) {
         if (!state.isBlockVisual) return;
         int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
         int caret = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
         int anchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
         int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
         int anchorCol = anchor - ::SendMessage(h, SCI_POSITIONFROMLINE, anchorLine, 0);
         int caretCol = caret - ::SendMessage(h, SCI_POSITIONFROMLINE, caretLine, 0);
         
         int startLine = (std::min)(anchorLine, caretLine);
         int endLine = (std::max)(anchorLine, caretLine);
         int startCol = (std::min)(anchorCol, caretCol);
         
         ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
         ::SendMessage(h, SCI_CLEARSELECTIONS, 0, 0);
         
         bool first = true;
         for (int line = startLine; line <= endLine; line++) {
             int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
             int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
             int pos = lineStart + startCol;
             if (pos > lineEnd) pos = lineEnd;
             
             if (first) {
                 ::SendMessage(h, SCI_SETCURRENTPOS, pos, 0);
                 ::SendMessage(h, SCI_SETANCHOR, pos, 0);
                 first = false;
             } else {
                 ::SendMessage(h, SCI_ADDSELECTION, pos, pos);
             }
         }
         state.isBlockVisual = false;
         if (g_normalMode) g_normalMode->enterInsertMode();
     })
     .set("A", [this](HWND h, int c) {
         if (!state.isBlockVisual) return;
         int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
         int caret = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
         int anchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
         int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
         int anchorCol = anchor - ::SendMessage(h, SCI_POSITIONFROMLINE, anchorLine, 0);
         int caretCol = caret - ::SendMessage(h, SCI_POSITIONFROMLINE, caretLine, 0);
         
         int startLine = (std::min)(anchorLine, caretLine);
         int endLine = (std::max)(anchorLine, caretLine);
         int endCol = (std::max)(anchorCol, caretCol);
         
         ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
         ::SendMessage(h, SCI_CLEARSELECTIONS, 0, 0);
         
         bool first = true;
         for (int line = startLine; line <= endLine; line++) {
             int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
             int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
             int pos = lineStart + endCol;
             if (pos > lineEnd) pos = lineEnd;
             
             if (first) {
                 ::SendMessage(h, SCI_SETCURRENTPOS, pos, 0);
                 ::SendMessage(h, SCI_SETANCHOR, pos, 0);
                 first = false;
             } else {
                 ::SendMessage(h, SCI_ADDSELECTION, pos, pos);
             }
         }
         state.isBlockVisual = false;
         if (g_normalMode) g_normalMode->enterInsertMode();
     });
    
    k.set("i", [this](HWND h, int c) {
         if (state.isBlockVisual) return;
         state.textObjectPending = 'i';
         Utils::setStatus(TEXT("-- inner text object --"));
     })
     .set("a", [this](HWND h, int c) {
         if (state.isBlockVisual) return;
         state.textObjectPending = 'a';
         Utils::setStatus(TEXT("-- around text object --"));
     });

    k.motion("h", 'h', [this](HWND h, int c) {
    Motion::charLeft(h, c);
    updateSelection(h);
    })
    .motion("j", 'j', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            int col = caret - ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            for (int i = 0; i < c; i++) {
                line++;
                int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
                if (line >= total) break;
            }
            int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
            int target = lineStart + col;
            if (target > lineEnd) target = lineEnd;
            ::SendMessage(h, SCI_SETCURRENTPOS, target, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, target, 0);
        } else {
            Motion::lineDown(h, c);
        }
        updateSelection(h);
    })
    .motion("k", 'k', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            int col = caret - ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            for (int i = 0; i < c; i++) {
                line--;
                if (line < 0) break;
            }
            if (line < 0) line = 0;
            int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
            int target = lineStart + col;
            if (target > lineEnd) target = lineEnd;
            ::SendMessage(h, SCI_SETCURRENTPOS, target, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, target, 0);
        } else {
            Motion::lineUp(h, c);
        }
        updateSelection(h);
    })
    .motion("l", 'l', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) ::SendMessage(h, SCI_CHARRIGHT, 0, 0);
            int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, pos, 0);
        } else {
            Motion::charRight(h, c);
        }
        updateSelection(h);
    })
    .motion("w", 'w', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) handleBlockWordRight(h, false);
        } else {
            Motion::wordRight(h, c);
        }
        updateSelection(h);
    })
    .motion("W", 'W', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) handleBlockWordRight(h, true);
        } else {
            Motion::wordRightBig(h, c);
        }
        updateSelection(h);
    })
    .motion("b", 'b', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) handleBlockWordLeft(h, false);
        } else {
            Motion::wordLeft(h, c);
        }
        updateSelection(h);
    })
    .motion("B", 'B', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) handleBlockWordLeft(h, true);
        } else {
            Motion::wordLeftBig(h, c);
        }
        updateSelection(h);
    })
    .motion("e", 'e', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) handleBlockWordEnd(h, false);
        } else {
            Motion::wordEnd(h, c);
        }
        updateSelection(h);
    })
    .motion("E", 'E', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) handleBlockWordEnd(h, true);
        } else {
            Motion::wordEndBig(h, c);
        }
        updateSelection(h);
    })
     .motion("$", '$', [this](HWND h, int c) {
         if (state.isBlockVisual) ::SendMessage(h, SCI_LINEEND, 0, 0);
         else Motion::lineEnd(h, c);
         updateSelection(h);
     })
     .motion("^", '^', [this](HWND h, int c) {
         if (state.isBlockVisual) ::SendMessage(h, SCI_VCHOME, 0, 0);
         else Motion::lineStart(h, c);
         updateSelection(h);
     })
     .motion("0", '0', [this](HWND h, int c) {
         if (state.isBlockVisual) ::SendMessage(h, SCI_VCHOME, 0, 0);
         else Motion::lineStart(h, 1);
         updateSelection(h);
     })
     .motion("{", '{', [this](HWND h, int c) {
         if (state.isBlockVisual) {
             for (int i = 0; i < c; i++) ::SendMessage(h, SCI_PARAUP, 0, 0);
         } else {
             Motion::paragraphUp(h, c);
         }
         updateSelection(h);
     })
     .motion("}", '}', [this](HWND h, int c) {
         if (state.isBlockVisual) {
             for (int i = 0; i < c; i++) ::SendMessage(h, SCI_PARADOWN, 0, 0);
         } else {
             Motion::paragraphDown(h, c);
         }
         updateSelection(h);
     })
     .motion("%", '%', [this](HWND h, int c) {
         int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
         int match = ::SendMessage(h, SCI_BRACEMATCH, pos, 0);
         if (match != -1) {
             ::SendMessage(h, SCI_GOTOPOS, match, 0);
             updateSelection(h);
         }
     })
     .motion("G", 'G', [this](HWND h, int c) {
         if (c == 1) Motion::documentEnd(h);
         else Motion::gotoLine(h, c);
         updateSelection(h);
     })
     .set("gg", [this](HWND h, int c) {
         if (c > 1) Motion::gotoLine(h, c);
         else Motion::documentStart(h);
         updateSelection(h);
     })
     .motion("H", 'H', [this](HWND h, int c) {
         if (state.isBlockVisual) ::SendMessage(h, SCI_PAGEUP, 0, 0);
         else Motion::pageUp(h);
         updateSelection(h);
     })
     .motion("L", 'L', [this](HWND h, int c) {
         if (state.isBlockVisual) ::SendMessage(h, SCI_PAGEDOWN, 0, 0);
         else Motion::pageDown(h);
         updateSelection(h);
     })
     .set("~", [this](HWND h, int c) {
         motion.toggleCase(h, 1);
         exitToNormal(h);
     });
    
    k.set("f", [this](HWND h, int c) {
         state.opPending = 'f';
         state.textObjectPending = 'f';
         Utils::setStatus(TEXT("-- find char --"));
     })
     .set("F", [this](HWND h, int c) {
         state.opPending = 'F';
         state.textObjectPending = 'f';
         Utils::setStatus(TEXT("-- find char backward --"));
     })
     .set("t", [this](HWND h, int c) {
         state.opPending = 't';
         state.textObjectPending = 't';
         Utils::setStatus(TEXT("-- till char --"));
     })
     .set("T", [this](HWND h, int c) {
         state.opPending = 'T';
         state.textObjectPending = 't';
         Utils::setStatus(TEXT("-- till char backward --"));
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
         updateSelection(h);
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
         updateSelection(h);
     });
    
    k.set("/", [this](HWND h, int c) {
         if (g_commandMode) {
             state.visualSearchAnchor = state.visualAnchor;
             g_commandMode->enter('/');
         }
     })
     .set("n", [this](HWND h, int c) {
         if (g_commandMode) {
             if (state.visualSearchAnchor == -1) state.visualSearchAnchor = state.visualAnchor;
             for (int i = 0; i < c; i++) g_commandMode->searchNext(h);
             updateSelection(h);
         }
     })
     .set("N", [this](HWND h, int c) {
         if (g_commandMode) {
             if (state.visualSearchAnchor == -1) state.visualSearchAnchor = state.visualAnchor;
             for (int i = 0; i < c; i++) g_commandMode->searchPrevious(h);
             updateSelection(h);
         }
     });
    
    k.set("<", [](HWND h, int c) { Utils::handleIndent(h, c); })
     .set(">", [](HWND h, int c) { Utils::handleIndent(h, c); })
     .set("=", [](HWND h, int c) { Utils::handleAutoIndent(h, c); });
    
    k.set("gcc", [this](HWND h, int c) {
         ::SendMessage(nppData._nppHandle, WM_COMMAND, IDM_EDIT_BLOCK_COMMENT, 0);
         state.recordLastOp(OP_MOTION, c, 'c');
         exitToNormal(h);
     });
}

void VisualMode::enterChar(HWND hwnd) {
    state.mode = VISUAL;
    state.isLineVisual = false;
    state.isBlockVisual = false;
    
    int caret = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchor = caret;
    state.visualAnchorLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
    state.visualSearchAnchor = -1;
    
    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
    ::SendMessage(hwnd, SCI_SETANCHOR, caret, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, caret, 0);
    
    Utils::setStatus(TEXT("-- VISUAL --"));
}

void VisualMode::enterLine(HWND hwnd) {
    state.mode = VISUAL;
    state.isLineVisual = true;
    state.isBlockVisual = false;
    
    int caret = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchorLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
    state.visualAnchor = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
    state.visualSearchAnchor = -1;
    
    Utils::setStatus(TEXT("-- VISUAL LINE --"));
    setSelection(hwnd);
}

void VisualMode::enterBlock(HWND hwnd) {
    state.mode = VISUAL;
    state.isLineVisual = false;
    state.isBlockVisual = true;
    
    int caret = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    state.visualAnchor = caret;
    state.visualAnchorLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
    state.visualSearchAnchor = -1;
    
    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);
    ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONANCHOR, caret, 0);
    ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
    
    Utils::setStatus(TEXT("-- VISUAL BLOCK --"));
}

void VisualMode::setSelection(HWND hwnd) {
    int caret = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    
    if (state.isLineVisual) {
        int currentLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
        int anchorLine = state.visualAnchorLine;
        
        int startLine = (std::min)(anchorLine, currentLine);
        int endLine = (std::max)(anchorLine, currentLine);
        
        int startPos = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, startLine, 0);
        int endPos;
        
        int totalLines = ::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
        if (endLine < totalLines - 1) {
            endPos = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, endLine + 1, 0);
        } else {
            endPos = ::SendMessage(hwnd, SCI_GETTEXTLENGTH, 0, 0);
        }
        
        if (anchorLine <= currentLine) {
            ::SendMessage(hwnd, SCI_SETSEL, startPos, endPos);
        } else {
            ::SendMessage(hwnd, SCI_SETSEL, endPos, startPos);
        }
    } else if (state.isBlockVisual) {
        ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);
        ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONANCHOR, state.visualAnchor, 0);
        ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
    } else {
        ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
        int start = state.visualAnchor;
        int end = caret;
        if (start < end) {
            end = ::SendMessage(hwnd, SCI_POSITIONAFTER, end, 0);
        } else if (end < start) {
            end = ::SendMessage(hwnd, SCI_POSITIONAFTER, end, 0);
        }
        ::SendMessage(hwnd, SCI_SETSEL, start, end);
    }
}

void VisualMode::updateSelection(HWND hwnd) {
    setSelection(hwnd);
}

void VisualMode::exitToNormal(HWND hwnd) {
    if (g_normalMode) g_normalMode->enter();
}

void VisualMode::handleKey(HWND hwnd, char c) {
    if ((state.textObjectPending == 'f' && (state.opPending == 'f' || state.opPending == 'F')) ||
        (state.textObjectPending == 't' && (state.opPending == 't' || state.opPending == 'T'))) {
        int count = state.repeatCount > 0 ? state.repeatCount : 1;
        handleCharSearchInput(hwnd, c, state.opPending, count);
        return;
    }

    if (state.textObjectPending && !state.isBlockVisual &&
        (c == 'w' || c == 'W' || c == 'p' || c == 's' ||
         c == '"' || c == '\'' || c == '`' ||
         c == '(' || c == ')' || c == '[' || c == ']' ||
         c == '{' || c == '}' || c == '<' || c == '>' || c == 't')) {
        TextObject::apply(hwnd, state, 'v', state.textObjectPending, c == 't' ? 't' : c);
        state.textObjectPending = 0;
        state.repeatCount = 0;
        return;
    }

    if (g_visualKeymap && g_visualKeymap->handleKey(hwnd, c)) {
        state.textObjectPending = 0;
        return;
    }

    state.textObjectPending = 0;
}

void VisualMode::handleCharSearchInput(HWND hwnd, char searchChar, char searchType, int count) {
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
    updateSelection(hwnd);
}

void VisualMode::handleBlockWordRight(HWND hwnd, bool bigWord) {
    int pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
    int lineEnd = ::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0);
    
    if (pos >= lineEnd) {
        int nextLine = line + 1;
        int lineCount = ::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
        if (nextLine < lineCount) {
            int nextLineStart = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, nextLine, 0);
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, nextLineStart, 0);
        }
        return;
    }
    
    while (pos < lineEnd) {
        char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (bigWord ? (ch == ' ' || ch == '\t') : !iswalnum(ch)) break;
        pos++;
    }
    
    while (pos < lineEnd) {
        char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (!(ch == ' ' || ch == '\t')) break;
        pos++;
    }
    
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, pos, 0);
}

void VisualMode::handleBlockWordLeft(HWND hwnd, bool bigWord) {
    int pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
    
    if (pos <= lineStart) {
        int prevLine = line - 1;
        if (prevLine >= 0) {
            int prevLineEnd = ::SendMessage(hwnd, SCI_GETLINEENDPOSITION, prevLine, 0);
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, prevLineEnd, 0);
        }
        return;
    }
    
    pos--;
    
    while (pos > lineStart) {
        char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (!(ch == ' ' || ch == '\t')) break;
        pos--;
    }
    
    while (pos > lineStart) {
        char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos - 1, 0);
        if (bigWord ? (ch == ' ' || ch == '\t') : !iswalnum(ch)) break;
        pos--;
    }
    
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, pos, 0);
}

void VisualMode::handleBlockWordEnd(HWND hwnd, bool bigWord) {
    int pos = ::SendMessage(hwnd, SCI_GETCURRENTPOS, 0, 0);
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, pos, 0);
    int lineEnd = ::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0);
    
    if (pos >= lineEnd) return;
    
    pos++;
    
    while (pos < lineEnd) {
        char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (bigWord ? (ch == ' ' || ch == '\t') : !iswalnum(ch)) break;
        pos++;
    }
    
    if (pos > 0) pos--;
    
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, pos, 0);
}