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
            
            if (anchor == caret) {
                state.recordLastOp(OP_MOTION, c, 'y');
                exitToNormal(h);
                return;
            }
            
            int anchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, anchor, 0);
            int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            
            int anchorVirtualCol = ::SendMessage(h, SCI_GETCOLUMN, anchor, 0);
            int caretVirtualCol = ::SendMessage(h, SCI_GETCOLUMN, caret, 0);
            
            int startLine = (std::min)(anchorLine, caretLine);
            int endLine = (std::max)(anchorLine, caretLine);
            int startCol = (std::min)(anchorVirtualCol, caretVirtualCol);
            int endCol = (std::max)(anchorVirtualCol, caretVirtualCol);
            
            if (startCol == endCol) {
                endCol = startCol + 1;
            }
            
            std::string clipText;
            for (int line = startLine; line <= endLine; line++) {
                int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
                int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                
                int colStart = ::SendMessage(h, SCI_FINDCOLUMN, line, startCol);
                int colEnd = ::SendMessage(h, SCI_FINDCOLUMN, line, endCol);
                
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
        } else if (state.isLineVisual) {
            int startLine = ::SendMessage(h, SCI_LINEFROMPOSITION,
                ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0), 0);
            int endLine = ::SendMessage(h, SCI_LINEFROMPOSITION,
                ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0), 0);

            int start = ::SendMessage(h, SCI_POSITIONFROMLINE, startLine, 0);
            int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
            int end = (endLine < total - 1)
                ? ::SendMessage(h, SCI_POSITIONFROMLINE, endLine + 1, 0)
                : ::SendMessage(h, SCI_GETLINEENDPOSITION, endLine, 0) + 1;

            ::SendMessage(h, SCI_SETSEL, start, end);
            ::SendMessage(h, SCI_COPY, 0, 0);
            state.lastYankLinewise = true;
        }
        else {
            int start = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            int end = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
            ::SendMessage(h, SCI_COPYRANGE, start, end);
            state.lastYankLinewise = false;
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
            state.visualAnchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
            state.visualAnchor = ::SendMessage(h, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
        } else {
            state.visualAnchor = pos;
            state.visualAnchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
        }
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
        } else {
            int start = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, start, 0);
            ::SendMessage(h, SCI_SETANCHOR, start, 0);
        }
        if (g_normalMode) g_normalMode->enterInsertMode();
    })
    .set("A", [this](HWND h, int c) {
        if (state.isBlockVisual) {
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
        } else {
            int end = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, end, 0);
            ::SendMessage(h, SCI_SETANCHOR, end, 0);
        }
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
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret - 1, 0);
                int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
            }
        } else {
            Motion::charLeft(h, c);  
        }
    })
    .motion("l", 'l', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret + 1, 0);
                int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
            }
        } else {
           Motion::charRight(h, c); 
        }
    })
    .motion("j", 'j', [this](HWND h, int c) {
        if (state.isLineVisual || state.isBlockVisual) {
            int currentPos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, currentPos, 0);
            int newLine = line + c;
            if (newLine >= ::SendMessage(h, SCI_GETLINECOUNT, 0, 0)) {
                newLine = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0) - 1;
            }
            if (newLine < 0) newLine = 0;
            
            int newPos = ::SendMessage(h, SCI_POSITIONFROMLINE, newLine, 0);
            visualMoveCursor(h, newPos);
        } else {
            Motion::lineDown(h, c);
        }
    })
    .motion("k", 'k', [this](HWND h, int c) {
        if (state.isLineVisual || state.isBlockVisual) {
            int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
            int newLine = line - c;
            if (newLine < 0) newLine = 0;

            int newPos = ::SendMessage(h, SCI_POSITIONFROMLINE, newLine, 0);
            visualMoveCursor(h, newPos);
        } else {
            Motion::lineUp(h, c);
        }
    })
    .motion("w", 'w', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                
                while (caret < lineEnd) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret, 0);
                    if (!std::isalnum(static_cast<unsigned char>(ch))) break;
                    caret++;
                }
                
                while (caret < lineEnd) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret, 0);
                    if (ch == ' ' || ch == '\t') break;
                    caret++;
                }
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
            }
        } else {
            Motion::wordRight(h, c);
        }
    })
    .motion("W", 'W', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                
                while (caret < lineEnd) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret, 0);
                    if (ch == ' ' || ch == '\t') break;
                    caret++;
                }
                
                while (caret < lineEnd) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret, 0);
                    if (ch != ' ' && ch != '\t') break;
                    caret++;
                }
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
            }
        } else {
            Motion::wordRightBig(h, c);
        }
    })
    .motion("b", 'b', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
                
                if (caret > lineStart) caret--;
                
                while (caret > lineStart) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret - 1, 0);
                    if (!std::isalnum(static_cast<unsigned char>(ch))) break;
                    caret--;
                }
                
                while (caret > lineStart) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret - 1, 0);
                    if (ch != ' ' && ch != '\t') break;
                    caret--;
                }
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
            }
        } else {
            Motion::wordLeft(h, c);
        }
    })
    .motion("B", 'B', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
                
                if (caret > lineStart) caret--;
                
                while (caret > lineStart) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret - 1, 0);
                    if (ch == ' ' || ch == '\t') break;
                    caret--;
                }
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
            }
        } else {
            Motion::wordLeftBig(h, c);
        }
    })
    .motion("e", 'e', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                
                if (caret < lineEnd) caret++;
                
                while (caret < lineEnd) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret, 0);
                    if (!std::isalnum(static_cast<unsigned char>(ch))) break;
                    caret++;
                }
                
                caret--;
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
            }
        } else {
            Motion::wordEnd(h, c);
        }
    })
    .motion("E", 'E', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
                
                if (caret < lineEnd) caret++;
                
                while (caret < lineEnd) {
                    char ch = (char)::SendMessage(h, SCI_GETCHARAT, caret, 0);
                    if (ch == ' ' || ch == '\t') break;
                    caret++;
                }
                
                caret--;
                
                ::SendMessage(h, SCI_SETCURRENTPOS, caret, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, caret, 0);
            }
        } else {
            Motion::wordEndBig(h, c);
        }
    })
    .motion("$", '$', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
            
            ::SendMessage(h, SCI_SETCURRENTPOS, lineEnd, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, lineEnd, 0);
        } else {
            Motion::lineEnd(h, c);
        }
    })
    .motion("^", '^', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            
            ::SendMessage(h, SCI_SETCURRENTPOS, lineStart, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, lineStart, 0);
        } else {
            Motion::lineStart(h, c);
        }
    })
    .motion("0", '0', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int line = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            
            ::SendMessage(h, SCI_SETCURRENTPOS, lineStart, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, lineStart, 0);
        } else {
            Motion::lineStart(h, 1);
        }
    })
    .motion("{", '{', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_PARAUP, 0, 0);
                int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
            }
        } else {
            Motion::paragraphUp(h, c);
        }
    })
    .motion("}", '}', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
                int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_PARADOWN, 0, 0);
                int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
            }
        } else {
            Motion::paragraphDown(h, c);
        }
    })
    .motion("%", '%', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            
            int match = ::SendMessage(h, SCI_BRACEMATCH, caret, 0);
            if (match != -1) {
                ::SendMessage(h, SCI_GOTOPOS, match, 0);
                int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
                
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
                ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
            }
        } else {
            int pos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            int match = ::SendMessage(h, SCI_BRACEMATCH, pos, 0);
            if (match != -1) {
                ::SendMessage(h, SCI_GOTOPOS, match, 0);
            }
        }
    })
    .motion("G", 'G', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            
            if (c == 1) {
                ::SendMessage(h, SCI_DOCUMENTEND, 0, 0);
            } else {
                ::SendMessage(h, SCI_GOTOLINE, c - 1, 0);
            }
            
            int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
        } else {
            if (c == 1) Motion::documentEnd(h);
            else Motion::gotoLine(h, c);
        }
    })
    .set("gg", [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            
            if (c > 1) {
                ::SendMessage(h, SCI_GOTOLINE, c - 1, 0);
            } else {
                ::SendMessage(h, SCI_DOCUMENTSTART, 0, 0);
            }
            
            int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
        } else {
            if (c > 1) Motion::gotoLine(h, c);
            else Motion::documentStart(h);
        }
    })
    .motion("H", 'H', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            
            ::SendMessage(h, SCI_PAGEUP, 0, 0);
            int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
        } else {
            ::SendMessage(h, SCI_PAGEUP, 0, 0);
        }
    })
    .motion("L", 'L', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            
            ::SendMessage(h, SCI_PAGEDOWN, 0, 0);
            int newCaret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
            
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
            ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
        } else {
            ::SendMessage(h, SCI_PAGEDOWN, 0, 0);
        }
    })
     .set("~", [this](HWND h, int c) {
         motion.toggleCase(h, 1);
         exitToNormal(h);
     });
    
     k.set("iw", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'i', 'w');
    })
    .set("iW", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'i', 'W');
    })
    .set("aw", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'a', 'w');
    })
    .set("aW", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'a', 'W');
    })
    .set("ip", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'i', 'p');
    })
    .set("ap", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'a', 'p');
    })
    .set("is", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'i', 's');
    })
    .set("as", [this](HWND h, int c) {
        TextObject t; t.apply(h, state, 'v', 'a', 's');
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
             
         }
     })
     .set("N", [this](HWND h, int c) {
         if (g_commandMode) {
             if (state.visualSearchAnchor == -1) state.visualSearchAnchor = state.visualAnchor;
             for (int i = 0; i < c; i++) g_commandMode->searchPrevious(h);
             
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

     k.set("gq", [this](HWND h, int c) {
        int selStart = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
        int selEnd = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
        
        if (selStart == selEnd) {
            exitToNormal(h);
            return;
        }
        
        int startLine = ::SendMessage(h, SCI_LINEFROMPOSITION, selStart, 0);
        int endLine = ::SendMessage(h, SCI_LINEFROMPOSITION, selEnd, 0);
        
        int textWidth = ::SendMessage(h, SCI_GETEDGECOLUMN, 0, 0);
        if (textWidth <= 0) textWidth = 80;
        
        ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
        
        for (int line = startLine; line <= endLine; line++) {
            int lineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
            int lineLength = lineEnd - lineStart;
            
            if (lineLength > textWidth) {
                std::vector<char> buffer(lineLength + 1);
                Sci_TextRangeFull tr;
                tr.chrg.cpMin = lineStart;
                tr.chrg.cpMax = lineEnd;
                tr.lpstrText = buffer.data();
                ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);
                
                std::string lineText(buffer.data());
                
                size_t breakPos = lineText.rfind(' ', textWidth);
                if (breakPos != std::string::npos && breakPos > 0) {
                    ::SendMessage(h, SCI_SETTARGETRANGE, lineStart + breakPos, lineStart + breakPos + 1);
                    ::SendMessage(h, SCI_REPLACETARGET, 1, (LPARAM)"\n");
                    
                    endLine++;
                }
            }
        }
        
        ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
        
        state.recordLastOp(OP_MOTION, c, 'g', 'q');
        exitToNormal(h);
    })
    .set("gw", [this](HWND h, int c) {
        int selStart = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
        int selEnd = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
        
        if (selStart == selEnd) {
            exitToNormal(h);
            return;
        }
        
        int cursorPos = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);
        
        g_visualKeymap->handleKey(h, 'g');
        g_visualKeymap->handleKey(h, 'q');
        
        ::SendMessage(h, SCI_SETCURRENTPOS, cursorPos, 0);
        ::SendMessage(h, SCI_SETSEL, cursorPos, cursorPos);
    });

    k.set("U", [this](HWND h, int c) {
        int start = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
        int end = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
        for (int pos = start; pos < end; pos++) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
            if (std::islower(ch)) {
                ::SendMessage(h, SCI_SETTARGETRANGE, pos, pos + 1);
                std::string upper(1, std::toupper(ch));
                ::SendMessage(h, SCI_REPLACETARGET, 1, (LPARAM)upper.c_str());
            }
        }
        exitToNormal(h);
    })
    .set("u", [this](HWND h, int c) {
        int start = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
        int end = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
        for (int pos = start; pos < end; pos++) {
            char ch = ::SendMessage(h, SCI_GETCHARAT, pos, 0);
            if (std::isupper(ch)) {
                ::SendMessage(h, SCI_SETTARGETRANGE, pos, pos + 1);
                std::string lower(1, std::tolower(ch));
                ::SendMessage(h, SCI_REPLACETARGET, 1, (LPARAM)lower.c_str());
            }
        }
        exitToNormal(h);
    })
    .set("J", [this](HWND h, int c) {
        int startLine = ::SendMessage(h, SCI_LINEFROMPOSITION, ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0), 0);
        int endLine = ::SendMessage(h, SCI_LINEFROMPOSITION, ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0), 0);
        
        ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
        for (int line = startLine; line < endLine; line++) {
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, startLine, 0);
            int nextLine = ::SendMessage(h, SCI_POSITIONFROMLINE, startLine + 1, 0);
            ::SendMessage(h, SCI_SETSEL, lineEnd, nextLine);
            ::SendMessage(h, SCI_REPLACESEL, 0, (LPARAM)" ");
        }
        ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
        exitToNormal(h);
    })
    .set("r", [this](HWND h, int c) {
        state.visualReplacePending = true;
        Utils::setStatus(TEXT("-- VISUAL REPLACE --"));
    })
    .set("S", [this](HWND h, int c) {
        ::SendMessage(h, SCI_BEGINUNDOACTION, 0, 0);
        ::SendMessage(h, SCI_CLEAR, 0, 0);
        ::SendMessage(h, SCI_ENDUNDOACTION, 0, 0);
        if (g_normalMode) g_normalMode->enterInsertMode();
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
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
    
    state.visualAnchorLine = line;
    state.visualAnchor = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
    state.visualSearchAnchor = -1;

    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);

    int lineStart = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
    int lineEnd   = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line + 1, 0);

    ::SendMessage(hwnd, SCI_SETANCHOR, lineStart, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, lineEnd, 0);
    ::SendMessage(hwnd, SCI_SETSEL, lineStart, lineEnd);

    Utils::setStatus(TEXT("-- VISUAL LINE --"));
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
    
    int nextPos = ::SendMessage(hwnd, SCI_POSITIONAFTER, caret, 0);
    
    ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONANCHOR, caret, 0);
    ::SendMessage(hwnd, SCI_SETRECTANGULARSELECTIONCARET, nextPos, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, nextPos, 0);
    
    Utils::setStatus(TEXT("-- VISUAL BLOCK --"));
}

void VisualMode::exitToNormal(HWND h) {
    if (state.isBlockVisual) {
        state.lastVisualAnchor =
            ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
        state.lastVisualCaret =
            ::SendMessage(h, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
    } else {
        state.lastVisualAnchor =
            ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
        state.lastVisualCaret =
            ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);
    }

    state.lastVisualWasLine  = state.isLineVisual;
    state.lastVisualWasBlock = state.isBlockVisual;

    state.isLineVisual = false;
    state.isBlockVisual = false;

    if (g_normalMode) g_normalMode->enter();
}

void VisualMode::handleKey(HWND hwnd, char c) {

     if (state.visualReplacePending) {
        handleVisualReplaceInput(hwnd, c);
        return;
    }

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

void VisualMode::extendSelection(HWND hwndEdit, int newPos) {
    if (state.mode != VISUAL) {
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, newPos, newPos);
        return;
    }
    
    int anchor = state.visualAnchor;
    
    if (state.isLineVisual) {
        int anchorLine = state.visualAnchorLine;
        int newLine = ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, newPos, 0);
        
        int startLine = (std::min)(anchorLine, newLine);
        int endLine = (std::max)(anchorLine, newLine);
        
        int startPos = ::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        int total = ::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        int endPos = (endLine < total - 1)
            ? ::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0)
            : ::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0) + 1;
        
        if (endPos < startPos) {
            int temp = startPos;
            startPos = endPos;
            endPos = temp;
        }
        
        int docLength = ::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (endPos > docLength) {
            endPos = docLength;
        }
        
        ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
    }
    else if (state.isBlockVisual) {
        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONCARET, newPos, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
    }
    else {
        if (newPos >= anchor) {
            ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newPos);
        } else {
            ::SendMessage(hwndEdit, SCI_SETSEL, newPos, anchor);
        }
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
    }
}

void VisualMode::setSelection(HWND hwndEdit, int startPos, int endPos) {
    if (state.mode != VISUAL) {
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, endPos, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, endPos, endPos);
        return;
    }
    
    int anchor = state.visualAnchor;
    
    if (state.isLineVisual) {
        int anchorLine = state.visualAnchorLine;
        int startLine = ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, startPos, 0);
        int endLine = ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, endPos, 0);
        
        if ((anchor >= startPos && anchor <= endPos) || 
            (anchorLine >= startLine && anchorLine <= endLine)) {
            ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
        } else if (anchor < startPos) {
            ::SendMessage(hwndEdit, SCI_SETSEL, anchor, endPos);
        } else {
            ::SendMessage(hwndEdit, SCI_SETSEL, startPos, anchor);
        }
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, endPos, 0);
    }
    else if (state.isBlockVisual) {
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONCARET, endPos, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, endPos, 0);
    }
    else {
        if (endPos >= anchor) {
            ::SendMessage(hwndEdit, SCI_SETSEL, anchor, endPos);
        } else {
            ::SendMessage(hwndEdit, SCI_SETSEL, endPos, anchor);
        }
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, endPos, 0);
    }
}

void VisualMode::moveCursor(HWND hwndEdit, int newPos, bool extend) {
    if (extend) {
        extendSelection(hwndEdit, newPos);
    } else {
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, newPos, newPos);
    }
}

void VisualMode::visualMoveCursor(HWND hwndEdit, int newPos) {
    if (state.mode != VISUAL) return;
    
    if (state.isLineVisual) {
        int anchor = state.visualAnchor;
        int anchorLine = state.visualAnchorLine;
        int newLine = ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, newPos, 0);
        
        if (anchorLine < 0) {
            anchorLine = ::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, anchor, 0);
            state.visualAnchorLine = anchorLine;
        }
        
        int startLine = (std::min)(anchorLine, newLine);
        int endLine = (std::max)(anchorLine, newLine);
        
        int startPos = ::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        int totalLines = ::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
        int endPos = (endLine < totalLines - 1)
            ? ::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, endLine + 1, 0)
            : ::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0) + 1;
        
        int docLength = ::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (endPos > docLength) {
            endPos = docLength;
        }
        
        ::SendMessage(hwndEdit, SCI_SETSEL, startPos, endPos);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
        
        if (newPos < anchor) {
            ::SendMessage(hwndEdit, SCI_SETANCHOR, endPos, 0);
        } else {
            ::SendMessage(hwndEdit, SCI_SETANCHOR, startPos, 0);
        }
    }
    else if (state.isBlockVisual) {
        int anchor = state.visualAnchor;
        ::SendMessage(hwndEdit, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
        ::SendMessage(hwndEdit, SCI_SETRECTANGULARSELECTIONCARET, newPos, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
    }
    else {
        int anchor = state.visualAnchor;
        
        if (newPos >= anchor) {
            ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newPos);
        } else {
            ::SendMessage(hwndEdit, SCI_SETSEL, newPos, anchor);
        }
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, newPos, 0);
    }
}

void VisualMode::handleVisualReplaceInput(HWND hwnd, char replaceChar) {
    if (state.isBlockVisual) {
        int anchor = ::SendMessage(hwnd, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
        int caret = ::SendMessage(hwnd, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);
        
        if (anchor == caret) {
            Utils::setStatus(TEXT("No selection to replace"));
            state.visualReplacePending = false;
            exitToNormal(hwnd);
            return;
        }
        
        int anchorLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, anchor, 0);
        int caretLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
        
        int anchorVirtualCol = ::SendMessage(hwnd, SCI_GETCOLUMN, anchor, 0);
        int caretVirtualCol = ::SendMessage(hwnd, SCI_GETCOLUMN, caret, 0);
        
        int startLine = (std::min)(anchorLine, caretLine);
        int endLine = (std::max)(anchorLine, caretLine);
        int startCol = (std::min)(anchorVirtualCol, caretVirtualCol);
        int endCol = (std::max)(anchorVirtualCol, caretVirtualCol);
        
        if (startCol == endCol) {
            Utils::setStatus(TEXT("No selection to replace"));
            state.visualReplacePending = false;
            exitToNormal(hwnd);
            return;
        }
        
        ::SendMessage(hwnd, SCI_BEGINUNDOACTION, 0, 0);
        
        for (int line = startLine; line <= endLine; line++) {
            int lineStart = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
            int lineEnd = ::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0);
            
            int colStart = ::SendMessage(hwnd, SCI_FINDCOLUMN, line, startCol);
            int colEnd = ::SendMessage(hwnd, SCI_FINDCOLUMN, line, endCol);
            
            if (colStart > lineEnd) colStart = lineEnd;
            if (colEnd > lineEnd) colEnd = lineEnd;
            
            for (int pos = colStart; pos < colEnd; pos++) {
                char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
                if (ch != '\r' && ch != '\n') {
                    ::SendMessage(hwnd, SCI_SETTARGETRANGE, pos, pos + 1);
                    ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&replaceChar);
                }
            }
        }
        
        ::SendMessage(hwnd, SCI_ENDUNDOACTION, 0, 0);
        Utils::setStatus(TEXT("Block selection replaced"));
        
    } else if (state.isLineVisual) {
        int startPos = ::SendMessage(hwnd, SCI_GETSELECTIONSTART, 0, 0);
        int endPos = ::SendMessage(hwnd, SCI_GETSELECTIONEND, 0, 0);
        
        if (startPos == endPos) {
            Utils::setStatus(TEXT("No selection to replace"));
            state.visualReplacePending = false;
            exitToNormal(hwnd);
            return;
        }
        
        ::SendMessage(hwnd, SCI_BEGINUNDOACTION, 0, 0);
        
        for (int pos = startPos; pos < endPos; pos++) {
            char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
            if (ch != '\r' && ch != '\n') {
                ::SendMessage(hwnd, SCI_SETTARGETRANGE, pos, pos + 1);
                ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&replaceChar);
            }
        }
        
        ::SendMessage(hwnd, SCI_ENDUNDOACTION, 0, 0);
        Utils::setStatus(TEXT("Line selection replaced"));
        
    } else {
        int startPos = ::SendMessage(hwnd, SCI_GETSELECTIONSTART, 0, 0);
        int endPos = ::SendMessage(hwnd, SCI_GETSELECTIONEND, 0, 0);
        
        if (startPos == endPos) {
            Utils::setStatus(TEXT("No selection to replace"));
            state.visualReplacePending = false;
            exitToNormal(hwnd);
            return;
        }
        
        ::SendMessage(hwnd, SCI_BEGINUNDOACTION, 0, 0);
        
        for (int pos = startPos; pos < endPos; pos++) {
            ::SendMessage(hwnd, SCI_SETTARGETRANGE, pos, pos + 1);
            ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&replaceChar);
        }
        
        ::SendMessage(hwnd, SCI_ENDUNDOACTION, 0, 0);
        Utils::setStatus(TEXT("Selection replaced"));
    }
    
    state.visualReplacePending = false;
    state.recordLastOp(OP_REPLACE, 1, 'r', replaceChar);
    exitToNormal(hwnd);
}
