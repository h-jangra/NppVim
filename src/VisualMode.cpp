#include "../include/VisualMode.h"
#include "../include/NormalMode.h"
#include "../include/CommandMode.h"
#include "../include/Keymap.h"
#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../plugin/menuCmdID.h"
#include "../plugin/Scintilla.h"
#include <algorithm>
#include <cctype>

extern NormalMode* g_normalMode;
extern CommandMode* g_commandMode;
extern NppData nppData;

VisualMode::VisualMode(VimState& state) : state(state) {
    g_visualKeymap = std::make_unique<Keymap>(state);
    setupKeyMaps();
}

bool VisualMode::iswalnum(char c) {
    return std::isalnum(static_cast<unsigned char>(c));
}

std::string VisualMode::getSelectedText(HWND h) {
    int startPos = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
    int endPos = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);

    if (startPos == endPos) return "";

    std::vector<char> buffer(endPos - startPos + 1);
    Sci_TextRangeFull tr;
    tr.chrg.cpMin = startPos;
    tr.chrg.cpMax = endPos;
    tr.lpstrText = buffer.data();
    ::SendMessage(h, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);

    return std::string(buffer.data(), endPos - startPos);
}

void VisualMode::updateBlockAfterMove(HWND h, int newCaret) {
    int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
    ::SendMessage(h, SCI_SETCURRENTPOS, newCaret, 0);
    ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, anchor, 0);
    ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newCaret, 0);
}

void VisualMode::setupKeyMaps() {
    auto& k = *g_visualKeymap;

    k.set("d", "Delete selection", [this](HWND h, int c) {
        char reg = Utils::getCurrentRegister();
        bool toBlackhole = (reg == '_' || state.deleteToBlackhole);

        Utils::beginUndo(h);

        if (state.isBlockVisual) {
            BlockSelection blk = Utils::blockSelection(h);

            // Store in register unless it's blackhole
            if (!toBlackhole) {
                std::string content = getSelectedText(h);

                if (!content.empty()) {
                    Utils::setRegisterContent(reg, content);
                }
            }

            // Clear the block selection
            ::SendMessage(h, SCI_CLEAR, 0, 0);
            Utils::clearBlockSelection(h);
            int pos = Utils::caretPos(h);
            Utils::select(h, pos, pos);
            Utils::setCurrentRegister('"');
            state.deleteToBlackhole = false;
        }
        else if (state.isLineVisual) {
            int startPos = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            int endPos = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);

            // Store in register unless it's blackhole
            if (!toBlackhole) {
                std::string content = getSelectedText(h);
                if (!content.empty()) {
                    Utils::setRegisterContent(reg, content);
                }
                state.lastYankLinewise = true;
            }

            Utils::clear(h, startPos, endPos);
            Utils::setCurrentRegister('"');
            state.deleteToBlackhole = false;
        }
        else {
            int startPos = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            int endPos = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);

            // Store in register unless it's blackhole
            if (!toBlackhole) {
                std::string content = getSelectedText(h);
                if (!content.empty()) {
                    Utils::setRegisterContent(reg, content);
                }
                state.lastYankLinewise = false;
            }

            Utils::clear(h, startPos, endPos);
        }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'd');
         exitToNormal(h);
     })
    .set("x", "Clear selection", [this](HWND h, int c) { g_visualKeymap->handleKey(h, 'd'); })
    .set("y", "Yank selection", [this](HWND h, int c) {
        char reg = Utils::getCurrentRegister();

        if (state.isBlockVisual) {
            int anchor = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETRECTANGULARSELECTIONCARET, 0, 0);

            if (anchor == caret) {
                state.recordLastOp(OP_MOTION, c, 'y');
                exitToNormal(h);
                return;
            }

            BlockSelection blk = Utils::blockSelection(h);

            std::string content = getSelectedText(h);

            if (!content.empty() && reg != '_') {
                Utils::setRegisterContent(reg, content);
            }
            Utils::clearBlockSelection(h);
        }
        else if (state.isLineVisual) {
            int startPos = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            int endPos = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);

            std::string content = getSelectedText(h);
            if (!content.empty() && reg != '_') {
                Utils::setRegisterContent(reg, content);
            }
            state.lastYankLinewise = true;
        }
        else {
            int startPos = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            int endPos = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);

            std::string content = getSelectedText(h);
            if (!content.empty() && reg != '_') {
                Utils::setRegisterContent(reg, content);
            }
            state.lastYankLinewise = false;
        }

        Utils::setCurrentRegister('"');
        state.deleteToBlackhole = false;
        state.recordLastOp(OP_MOTION, c, 'y');
        exitToNormal(h);
    })
     .set("c", "Change selection", [this](HWND h, int c) {
        char reg = Utils::getCurrentRegister();
        bool toBlackhole = (reg == '_' || state.deleteToBlackhole);

        Utils::beginUndo(h);

        if (state.isBlockVisual) {
            BlockSelection blk = Utils::blockSelection(h);

            // Store in register unless it's blackhole
            if (!toBlackhole) {
                std::string content = getSelectedText(h);

                if (!content.empty()) {
                    Utils::setRegisterContent(reg, content);
                }
            }

            // Clear the block selection
            ::SendMessage(h, SCI_CLEAR, 0, 0);
            Utils::clearBlockSelection(h);
            ::SendMessage(h, SCI_CLEARSELECTIONS, 0, 0);

            // Set up multiple cursors at the start of each line in the block
            bool first = true;
            for (int line = blk.startLine; line <= blk.endLine; line++) {
                int lineStart = Utils::lineStart(h, line);
                int lineEnd = Utils::lineEnd(h, line);
                int pos = lineStart + blk.startCol;
                if (pos > lineEnd) pos = lineEnd;

                if (first) {
                    Utils::select(h, pos, pos);
                    first = false;
                } else {
                    ::SendMessage(h, SCI_ADDSELECTION, pos, pos);
                }
            }
        }
        else {
            int startPos = ::SendMessage(h, SCI_GETSELECTIONSTART, 0, 0);
            int endPos = ::SendMessage(h, SCI_GETSELECTIONEND, 0, 0);

            // Store in register unless it's blackhole
            if (!toBlackhole) {
                std::string content = getSelectedText(h);
                if (!content.empty()) {
                    Utils::setRegisterContent(reg, content);
                }
            }

            Utils::clear(h, startPos, endPos);
        }
         Utils::endUndo(h);
         state.recordLastOp(OP_MOTION, c, 'c');
         Utils::setCurrentRegister('"');
         state.deleteToBlackhole = false;
         if (g_normalMode) g_normalMode->enterInsertMode();
     })
    .set("o", "Switch cursor", [this](HWND h, int c) {
        if (state.isLineVisual) {
            int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
            int anchor = ::SendMessage(h, SCI_GETANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);

            int caretLine;
            if (caret > anchor) {
                int cl = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                caretLine = (caret == ::SendMessage(h, SCI_POSITIONFROMLINE, cl, 0) && cl > 0)
                    ? cl - 1 : cl;
            } else {
                caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            }

            int oldAnchorLine = state.visualAnchorLine;
            state.visualAnchorLine = caretLine;

            int startLine = (std::min)(caretLine, oldAnchorLine);
            int endLine   = (std::max)(caretLine, oldAnchorLine);

            int startPos = ::SendMessage(h, SCI_POSITIONFROMLINE, startLine, 0);
            int endPos = (endLine >= total - 1)
                ? ::SendMessage(h, SCI_GETLENGTH, 0, 0)
                : ::SendMessage(h, SCI_POSITIONFROMLINE, endLine + 1, 0);

            if (oldAnchorLine >= caretLine) {
                ::SendMessage(h, SCI_SETANCHOR, startPos, 0);
                ::SendMessage(h, SCI_SETCURRENTPOS, endPos, 0);
            } else {
                ::SendMessage(h, SCI_SETANCHOR, endPos, 0);
                ::SendMessage(h, SCI_SETCURRENTPOS, startPos, 0);
            }
        } else if (!state.isBlockVisual) {
            int pos = Utils::caretPos(h);
            int anchor = ::SendMessage(h, SCI_GETANCHOR, 0, 0);

            ::SendMessage(h, SCI_SETANCHOR, pos, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, anchor, 0);

            state.visualAnchor = pos;
            state.visualAnchorLine = ::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
        } else {
            int pos = Utils::caretPos(h);
            int anchor = ::SendMessage(h, SCI_GETANCHOR, 0, 0);

            ::SendMessage(h, SCI_SETCURRENTPOS, anchor, 0);
            ::SendMessage(h, SCI_SETANCHOR, pos, 0);

            state.visualAnchor = pos;
            state.visualAnchorLine = Utils::caretLine(h);
        }
        ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
    });

    k.set("v", "Character visual", [this](HWND h, int c) {
        if (state.isLineVisual || state.isBlockVisual) enterChar(h);
        else exitToNormal(h);
     })
     .set("V", "Line visual", [this](HWND h, int c) {
         if (state.isLineVisual) exitToNormal(h);
         else enterLine(h);
     })
     .set("\x16", "Block visual", [this](HWND h, int c) {
         if (state.isBlockVisual) exitToNormal(h);
         else enterBlock(h);
     });

    k.set("I", "Insert before", [this](HWND h, int c) {
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
                int lineStart = Utils::lineStart(h, line);
                int lineEnd = Utils::lineEnd(h, line);
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
    .set("A", "Insert after", [this](HWND h, int c) {
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
                int lineStart = Utils::lineStart(h, line);
                int lineEnd = Utils::lineEnd(h, line);
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
    })
    .set("_d", "Delete to blackhole", [this](HWND h, int c) {
        state.deleteToBlackhole = true;
        g_visualKeymap->handleKey(h, 'd');
        state.deleteToBlackhole = false;
    })
    .set("_x", "Delete to blackhole", [this](HWND h, int c) {
        state.deleteToBlackhole = true;
        g_visualKeymap->handleKey(h, 'x');
        state.deleteToBlackhole = false;
    });

    // k.set("i", [this](HWND h, int c) {
    //      if (state.isBlockVisual) return;
    //      state.textObjectPending = 'i';
    //      Utils::setStatus(TEXT("-- inner text object --"));
    //  })
    //  .set("a", [this](HWND h, int c) {
    //      if (state.isBlockVisual) return;
    //      state.textObjectPending = 'a';
    //      Utils::setStatus(TEXT("-- around text object --"));
    //     });

    k.motion("h", 'h', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int caret = Utils::caretPos(h);
                updateBlockAfterMove(h, caret - 1);
            }
        } else {
            int caret = Utils::caretPos(h);
            for (int i = 0; i < c; ++i)
                caret = (int)::SendMessage(h, SCI_POSITIONBEFORE, caret, 0);
            extendSelection(h, caret);
        }
    })
    .motion("l", 'l', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                int caret = Utils::caretPos(h);
                updateBlockAfterMove(h, caret + 1);
            }
        } else {
            int caret = Utils::caretPos(h);
            for (int i = 0; i < c; ++i)
                caret = (int)::SendMessage(h, SCI_POSITIONAFTER, caret, 0);
            extendSelection(h, caret);
        }
    })
    .motion("j", 'j', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int line = Utils::caretLine(h);
            int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
            int newLine = line + c;
            if (newLine >= total) newLine = total - 1;

            int newPos = ::SendMessage(h, SCI_POSITIONFROMLINE, newLine, 0);
            updateBlockAfterMove(h, newPos);
        }
        else if (state.isLineVisual) {
            int anchorLine = state.visualAnchorLine;
            int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
            int anchor = ::SendMessage(h, SCI_GETANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);

            int currentLine;
            if (caret > anchor) {
                int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                currentLine = (caret == ::SendMessage(h, SCI_POSITIONFROMLINE, caretLine, 0) && caretLine > 0)
                    ? caretLine - 1 : caretLine;
            } else {
                currentLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            }

            int newLine = currentLine + c;
            if (newLine >= total) newLine = total - 1;

            int startLine = (std::min)(anchorLine, newLine);
            int endLine = (std::max)(anchorLine, newLine);

            int startPos = ::SendMessage(h, SCI_POSITIONFROMLINE, startLine, 0);
            int endPos = (endLine >= total - 1)
                ? ::SendMessage(h, SCI_GETLENGTH, 0, 0)
                : ::SendMessage(h, SCI_POSITIONFROMLINE, endLine + 1, 0);

            if (newLine >= anchorLine) {
                ::SendMessage(h, SCI_SETANCHOR, startPos, 0);
                ::SendMessage(h, SCI_SETCURRENTPOS, endPos, 0);
            } else {
                ::SendMessage(h, SCI_SETANCHOR, endPos, 0);
                ::SendMessage(h, SCI_SETCURRENTPOS, startPos, 0);
            }
            ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
        }
        else {
            Motion::lineDown(h, c);
            extendSelection(h, Utils::caretPos(h));
        }
    })
    .motion("k", 'k', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int line = Utils::caretLine(h);
            int newLine = line - c;
            if (newLine < 0) newLine = 0;

            int newPos = ::SendMessage(h, SCI_POSITIONFROMLINE, newLine, 0);
            updateBlockAfterMove(h, newPos);
        } 
        else if (state.isLineVisual) {
            int anchorLine = state.visualAnchorLine;
            int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
            int anchor = ::SendMessage(h, SCI_GETANCHOR, 0, 0);
            int caret = ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0);

            int currentLine;
            if (caret > anchor) {
                int caretLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
                currentLine = (caret == ::SendMessage(h, SCI_POSITIONFROMLINE, caretLine, 0) && caretLine > 0)
                    ? caretLine - 1 : caretLine;
            } else {
                currentLine = ::SendMessage(h, SCI_LINEFROMPOSITION, caret, 0);
            }

            int newLine = currentLine - c;
            if (newLine < 0) newLine = 0;

            int startLine = (std::min)(anchorLine, newLine);
            int endLine = (std::max)(anchorLine, newLine);

            int startPos = ::SendMessage(h, SCI_POSITIONFROMLINE, startLine, 0);
            int endPos = (endLine >= total - 1)
                ? ::SendMessage(h, SCI_GETLENGTH, 0, 0)
                : ::SendMessage(h, SCI_POSITIONFROMLINE, endLine + 1, 0);

            if (newLine <= anchorLine) {
                ::SendMessage(h, SCI_SETANCHOR, endPos, 0);
                ::SendMessage(h, SCI_SETCURRENTPOS, startPos, 0);
            } else {
                ::SendMessage(h, SCI_SETANCHOR, startPos, 0);
                ::SendMessage(h, SCI_SETCURRENTPOS, endPos, 0);
            }
            ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
        }
        else {
            Motion::lineUp(h, c);
            extendSelection(h, Utils::caretPos(h));
        }
    })
    .motion("w", 'w', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                handleBlockWordRight(h, false);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::wordRight(h, c);
        }
    })
    .motion("W", 'W', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                handleBlockWordRight(h, true);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::wordRightBig(h, c);
        }
    })
    .motion("b", 'b', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                handleBlockWordLeft(h, false);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::wordLeft(h, c);
        }
    })
    .motion("B", 'B', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                handleBlockWordLeft(h, true);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::wordLeftBig(h, c);
        }
    })
    .motion("e", 'e', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                handleBlockWordEnd(h, false);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::wordEnd(h, c);
        }
    })
    .motion("E", 'E', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                handleBlockWordEnd(h, true);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::wordEndBig(h, c);
        }
    })
    .motion("$", '$', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int line = Utils::caretLine(h);
            int lineEnd = Utils::lineEnd(h, line);
            updateBlockAfterMove(h, lineEnd);
        } else {
            Motion::lineEnd(h, c);
        }
    })
    .motion("^", '^', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int line = Utils::caretLine(h);
            int lineStart = Utils::lineStart(h, line);
            updateBlockAfterMove(h, lineStart);
        } else {
            Motion::lineStart(h, c);
        }
    })
    .motion("0", '0', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int line = Utils::caretLine(h);
            int lineStart = Utils::lineStart(h, line);
            updateBlockAfterMove(h, lineStart);
        } else {
            Motion::lineStart(h, 1);
        }
    })
    .motion("{", '{', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                ::SendMessage(h, SCI_PARAUP, 0, 0);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::paragraphUp(h, c);
        }
    })
    .motion("}", '}', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            for (int i = 0; i < c; i++) {
                ::SendMessage(h, SCI_PARADOWN, 0, 0);
                updateBlockAfterMove(h, Utils::caretPos(h));
            }
        } else {
            Motion::paragraphDown(h, c);
        }
    })
    .motion("%", '%', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            int match = ::SendMessage(h, SCI_BRACEMATCH, Utils::caretPos(h), 0);
            if (match != -1) updateBlockAfterMove(h, match);
        } else {
            int pos = Utils::caretPos(h);
            int match = ::SendMessage(h, SCI_BRACEMATCH, pos, 0);
            if (match != -1) {
                ::SendMessage(h, SCI_GOTOPOS, match, 0);
            }
        }
    })
    .motion("G", 'G', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            if (c == 1) ::SendMessage(h, SCI_DOCUMENTEND, 0, 0);
            else ::SendMessage(h, SCI_GOTOLINE, c - 1, 0);
            updateBlockAfterMove(h, Utils::caretPos(h));
        } else {
            if (c == 1) Motion::documentEnd(h);
            else Motion::gotoLine(h, c);
        }
    })
    .set("gg", [this](HWND h, int c) {
        if (state.isBlockVisual) {
            if (c > 1) ::SendMessage(h, SCI_GOTOLINE, c - 1, 0);
            else ::SendMessage(h, SCI_DOCUMENTSTART, 0, 0);
            updateBlockAfterMove(h, Utils::caretPos(h));
        } else {
            if (c > 1) Motion::gotoLine(h, c);
            else Motion::documentStart(h);
        }
    })
    .motion("H", 'H', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            ::SendMessage(h, SCI_PAGEUP, 0, 0);
            updateBlockAfterMove(h, Utils::caretPos(h));
        } else {
            ::SendMessage(h, SCI_PAGEUP, 0, 0);
        }
    })
    .motion("L", 'L', [this](HWND h, int c) {
        if (state.isBlockVisual) {
            ::SendMessage(h, SCI_PAGEDOWN, 0, 0);
            updateBlockAfterMove(h, Utils::caretPos(h));
        } else {
            ::SendMessage(h, SCI_PAGEDOWN, 0, 0);
        }
    })
     .set("~", [this](HWND h, int c) {
         motion.toggleCase(h, c);
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
         state.repeatCount = c;
         Utils::setStatus(TEXT("-- find char --"));
     })
     .set("F", [this](HWND h, int c) {
         state.opPending = 'F';
         state.textObjectPending = 'f';
         state.repeatCount = c;
         Utils::setStatus(TEXT("-- find char backward --"));
     })
     .set("t", [this](HWND h, int c) {
         state.opPending = 't';
         state.textObjectPending = 't';
         state.repeatCount = c;
         Utils::setStatus(TEXT("-- till char --"));
     })
     .set("T", [this](HWND h, int c) {
         state.opPending = 'T';
         state.textObjectPending = 't';
         state.repeatCount = c;
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
    
    k.set("zz", [this](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION, ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0), 0);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line - (::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2), 0);
    })
    .set("zt", [this](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION, ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0), 0);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line, 0);
    })
    .set("zb", [this](HWND h, int c) {
        int line = ::SendMessage(h, SCI_LINEFROMPOSITION, ::SendMessage(h, SCI_GETCURRENTPOS, 0, 0), 0);
        int linesOnScreen = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0);
        ::SendMessage(h, SCI_SETFIRSTVISIBLELINE, line - linesOnScreen + 1, 0);
    })
    .set("\x05", [](HWND h, int c) { ::SendMessage(h, SCI_LINESCROLL, 0, c); })
    .set("\x19", [](HWND h, int c) { ::SendMessage(h, SCI_LINESCROLL, 0, -c); })
    .set("\x04", [](HWND h, int c) {
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) Motion::lineDown(h, lines);
    })
    .set("\x15", [](HWND h, int c) {
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) Motion::lineUp(h, lines);
    })
    .set("\x06", [](HWND h, int c) {
        for (int i = 0; i < c; i++) Motion::pageDown(h);
    })
    .set("\x02", [](HWND h, int c) {
        for (int i = 0; i < c; i++) Motion::pageUp(h);
    })
    .set("\x12", [](HWND h, int c) {
        for (int i = 0; i < c; i++) ::SendMessage(h, SCI_REDO, 0, 0);
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
    
    k.set("\x04", [this](HWND h, int c) {
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) {
            Motion::lineDown(h, lines);
            extendSelection(h, Utils::caretPos(h));
        }
    })
    .set("\x15", [this](HWND h, int c) {
        int lines = ::SendMessage(h, SCI_LINESONSCREEN, 0, 0) / 2;
        for (int i = 0; i < c; i++) {
            Motion::lineUp(h, lines);
            extendSelection(h, Utils::caretPos(h));
        }
    });

    k.set("<", [](HWND h, int c) { Utils::handleUnindent(h, c); })
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

        Utils::beginUndo(h);

        for (int line = startLine; line <= endLine; line++) {
            int lineStart = Utils::lineStart(h, line);
            int lineEnd = Utils::lineEnd(h, line);
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

        Utils::endUndo(h);

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

        int cursorPos = Utils::caretPos(h);

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

        Utils::beginUndo(h);
        for (int line = startLine; line < endLine; line++) {
            int lineEnd = ::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
            int nextLine = ::SendMessage(h, SCI_POSITIONFROMLINE, line + 1, 0);
            ::SendMessage(h, SCI_SETSEL, lineEnd, nextLine);
            ::SendMessage(h, SCI_REPLACESEL, 0, (LPARAM)" ");
        }
        Utils::endUndo(h);
        exitToNormal(h);
    })
    .set("r", [this](HWND h, int c) {
        state.visualReplacePending = true;
        Utils::setStatus(TEXT("-- VISUAL REPLACE --"));
    })
    .set("S", [this](HWND h, int c) {
        char reg = Utils::getCurrentRegister();
        bool toBlackhole = (reg == '_' || state.deleteToBlackhole);

        Utils::beginUndo(h);

        // Store the selected text in register unless it's blackhole
        if (!toBlackhole) {
            std::string content = getSelectedText(h);
            if (!content.empty()) {
                Utils::setRegisterContent(reg, content);
            }
        }

        ::SendMessage(h, SCI_CLEAR, 0, 0);
        Utils::endUndo(h);
        if (g_normalMode) g_normalMode->enterInsertMode();
    });

}

void VisualMode::enterChar(HWND hwnd) {
    state.mode = VISUAL;
    state.isLineVisual = false;
    state.isBlockVisual = false;

    int caret = Utils::caretPos(hwnd);

    state.visualAnchor = caret;
    state.visualAnchorLine = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);
    state.visualSearchAnchor = -1;

    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
    ::SendMessage(hwnd, SCI_SETANCHOR, caret, 0);
    int nextPos = (int)::SendMessage(hwnd, SCI_POSITIONAFTER, caret, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, nextPos, 0);
    ::SendMessage(hwnd, SCI_SETSEL, caret, nextPos);

    Utils::setStatus(TEXT("-- VISUAL --"));
}

void VisualMode::enterLine(HWND hwnd) {
    state.mode = VISUAL;
    state.isLineVisual = true;
    state.isBlockVisual = false;

    int caret = Utils::caretPos(hwnd);
    int line = ::SendMessage(hwnd, SCI_LINEFROMPOSITION, caret, 0);

    state.visualAnchorLine = line;
    state.visualAnchor = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, state.visualAnchorLine, 0);
    state.visualSearchAnchor = -1;

    ::SendMessage(hwnd, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
    
    int lineStart = ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
    int totalLines = ::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
    int selectEnd = (line < totalLines - 1)
        ? ::SendMessage(hwnd, SCI_POSITIONFROMLINE, line + 1, 0)
        : ::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);

    ::SendMessage(hwnd, SCI_SETANCHOR, lineStart, 0);
    ::SendMessage(hwnd, SCI_SETCURRENTPOS, selectEnd, 0);
    ::SendMessage(hwnd, SCI_SETSEL, lineStart, selectEnd);

    Utils::setStatus(TEXT("-- VISUAL LINE --"));
}

void VisualMode::enterBlock(HWND hwnd) {
    state.mode = VISUAL;
    state.isLineVisual = false;
    state.isBlockVisual = true;

    int caret = Utils::caretPos(hwnd);
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

    if (state.opPending == 'f' || state.opPending == 'F' ||
        state.opPending == 't' || state.opPending == 'T') {

        int count = state.repeatCount > 0 ? state.repeatCount : 1;
        handleCharSearchInput(hwnd, c, state.opPending, count);
        return;
    }


    if (c == '"' && !state.visualReplacePending) {
        state.awaitingRegister = true;
        Utils::setStatus(TEXT("-- register --"));
        return;
    }

    if (state.awaitingRegister) {
        Utils::setCurrentRegister(c);
        state.awaitingRegister = false;
        Utils::setStatus(TEXT(""));
        return;
    }

     if (state.visualReplacePending) {
        handleVisualReplaceInput(hwnd, c);
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
        return;
    }

    state.textObjectPending = 0;
}

void VisualMode::handleCharSearchInput(HWND hwnd, char searchChar, char searchType, int count) {
    bool isTill = (searchType == 't' || searchType == 'T');
    bool isForward = (searchType == 'f' || searchType == 't');

    state.lastSearchChar = searchChar;
    state.lastSearchForward = isForward;
    state.lastSearchTill = isTill;

    int pos = Utils::caretPos(hwnd);
    
    if (isTill) {
        if (isForward) {
            Motion::tillChar(hwnd, count, searchChar);
        } else {
            Motion::tillCharBack(hwnd, count, searchChar);
        }
    } else {
        if (isForward) {
            Motion::nextChar(hwnd, count, searchChar);
        } else {
            Motion::prevChar(hwnd, count, searchChar);
        }
    }

    int newPos = Utils::caretPos(hwnd);
    
    if (isForward) {
        newPos = (int)::SendMessage(hwnd, SCI_POSITIONAFTER, newPos, 0);
    }
    extendSelection(hwnd, newPos);

    state.recordLastOp(OP_MOTION, count, isForward ? (isTill ? 't' : 'f') : (isTill ? 'T' : 'F'), searchChar);

    state.opPending = 0;
    state.textObjectPending = 0;
    state.repeatCount = 0;
    Utils::setStatus(TEXT(""));
}

void VisualMode::handleBlockWordRight(HWND hwnd, bool bigWord) {
    int pos = Utils::caretPos(hwnd);
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
    int pos = Utils::caretPos(hwnd);
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
    int pos = Utils::caretPos(hwnd);
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

void VisualMode::extendSelection(HWND h, int newPos) {
    if (state.mode != VISUAL) {
        ::SendMessage(h, SCI_SETCURRENTPOS, newPos, 0);
        ::SendMessage(h, SCI_SETSEL, newPos, newPos);
        return;
    }

    if (state.isBlockVisual) {
        ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE, 0);
        ::SendMessage(h, SCI_SETRECTANGULARSELECTIONANCHOR, state.visualAnchor, 0);
        ::SendMessage(h, SCI_SETRECTANGULARSELECTIONCARET, newPos, 0);
        ::SendMessage(h, SCI_SETCURRENTPOS, newPos, 0);
        return;
    }

    if (state.isLineVisual) {
        int anchorLine = state.visualAnchorLine;
        int newLine = ::SendMessage(h, SCI_LINEFROMPOSITION, newPos, 0);

        int startLine = (std::min)(anchorLine, newLine);
        int endLine = (std::max)(anchorLine, newLine);

        int startPos = ::SendMessage(h, SCI_POSITIONFROMLINE, startLine, 0);
        int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);

        int endPos = (endLine >= total - 1)
            ? ::SendMessage(h, SCI_GETLENGTH, 0, 0)
            : ::SendMessage(h, SCI_POSITIONFROMLINE, endLine + 1, 0);

        int caretLineStart = ::SendMessage(h, SCI_POSITIONFROMLINE, newLine, 0);

        if (newLine >= anchorLine) {
            ::SendMessage(h, SCI_SETANCHOR, startPos, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, endPos, 0);
        } else {
            ::SendMessage(h, SCI_SETANCHOR, endPos, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, startPos, 0);
        }
        ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
        return;
    }
                        
    int anchor = state.visualAnchor;
    ::SendMessage(h, SCI_SETSEL, anchor, newPos);
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

        Utils::beginUndo(hwnd);

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

        Utils::endUndo(hwnd);
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

        Utils::beginUndo(hwnd);

        for (int pos = startPos; pos < endPos; pos++) {
            char ch = ::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
            if (ch != '\r' && ch != '\n') {
                ::SendMessage(hwnd, SCI_SETTARGETRANGE, pos, pos + 1);
                ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&replaceChar);
            }
        }

        Utils::endUndo(hwnd);
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

        Utils::beginUndo(hwnd);

        for (int pos = startPos; pos < endPos; pos++) {
            ::SendMessage(hwnd, SCI_SETTARGETRANGE, pos, pos + 1);
            ::SendMessage(hwnd, SCI_REPLACETARGET, 1, (LPARAM)&replaceChar);
        }

        Utils::endUndo(hwnd);
        Utils::setStatus(TEXT("Selection replaced"));
    }

    state.visualReplacePending = false;
    state.recordLastOp(OP_REPLACE, 1, 'r', replaceChar);
    exitToNormal(hwnd);
}
