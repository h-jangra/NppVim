#include "../include/EditorOps.h"
#include "../include/NppVim.h"
#include "../plugin/Scintilla.h"
#include <algorithm>
#include <vector>

namespace EditorOps {

void yank(HWND hwnd, const EditRange& range, char reg, bool syncClipboard) {
    if (!hwnd) return;
    if (range.blockwise) {
        yankBlock(hwnd, range.block, reg, syncClipboard);
        return;
    }

    if (range.start >= range.end) return;
    std::string text = Utils::getTextRange(hwnd, range.start, range.end);
    if (text.empty()) return;

    RegisterType type = range.linewise ? RegisterType::LineWise : RegisterType::CharacterWise;
    Registers::getInstance().saveYanked(reg, text, type, syncClipboard);
    state.lastYankLinewise = range.linewise;
}

void erase(HWND hwnd, const EditRange& range, char reg, bool syncClipboard) {
    if (!hwnd) return;
    if (range.blockwise) {
        eraseBlock(hwnd, range.block, reg, syncClipboard);
        return;
    }

    if (range.start >= range.end) return;
    std::string text = Utils::getTextRange(hwnd, range.start, range.end);

    RegisterType type = range.linewise ? RegisterType::LineWise : RegisterType::CharacterWise;
    Registers::getInstance().saveDeleted(reg, text, type, syncClipboard);
    state.lastYankLinewise = range.linewise;

    Utils::beginUndo(hwnd);
    Utils::select(hwnd, range.start, range.end);
    ::SendMessage(hwnd, SCI_REPLACESEL, 0, (LPARAM)"");

    int newPos = range.start;
    int docLen = (int)::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);
    if (newPos > docLen) newPos = docLen;
    Utils::select(hwnd, newPos, newPos);
    Utils::endUndo(hwnd);
}

void change(HWND hwnd, const EditRange& range, char reg, bool syncClipboard) {
    erase(hwnd, range, reg, syncClipboard);
}

void put(HWND hwnd, int pos, char reg, bool before, bool forceLinewise) {
    if (!hwnd) return;

    RegisterEntry entry = Registers::getInstance().getEntry(reg);
    std::string text = entry.text;
    if (text.empty()) {
        text = Registers::getClipboardText();
        if (text.empty()) {
            Utils::setStatus(TEXT("E353: Nothing in register"));
            return;
        }
    }

    bool isLinewise = forceLinewise || (entry.type == RegisterType::LineWise) || state.lastYankLinewise;
    if (entry.type == RegisterType::BlockWise) {
        putBlock(hwnd, text, !before);
        return;
    }

    int caretPos = (pos >= 0) ? pos : Utils::caretPos(hwnd);
    int currentLine = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, caretPos, 0);
    int totalLines = (int)::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);

    int eolMode = (int)::SendMessage(hwnd, SCI_GETEOLMODE, 0, 0);
    std::string eolStr = (eolMode == SC_EOL_LF) ? "\n" : (eolMode == SC_EOL_CR ? "\r" : "\r\n");

    Utils::beginUndo(hwnd);

    if (isLinewise) {
        if (text.back() != '\n' && text.back() != '\r') {
            text += eolStr;
        }

        int targetPos = 0;
        if (before) {
            targetPos = Utils::lineStart(hwnd, currentLine);
        } else {
            if (currentLine >= totalLines - 1) {
                targetPos = (int)::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);
                if (targetPos > 0) {
                    char lastCh = (char)::SendMessage(hwnd, SCI_GETCHARAT, targetPos - 1, 0);
                    if (lastCh != '\n' && lastCh != '\r') {
                        text = eolStr + text;
                    }
                }
            } else {
                targetPos = Utils::lineStart(hwnd, currentLine + 1);
            }
        }

        ::SendMessage(hwnd, SCI_INSERTTEXT, targetPos, (LPARAM)text.c_str());
        int newLine = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, targetPos, 0);
        int finalPos = Utils::lineStart(hwnd, newLine);
        Utils::select(hwnd, finalPos, finalPos);
    } else {
        int insertPos = caretPos;
        if (!before) {
            int lineEndPos = Utils::lineEnd(hwnd, currentLine);
            if (insertPos < lineEndPos) {
                insertPos = (int)::SendMessage(hwnd, SCI_POSITIONAFTER, insertPos, 0);
            }
        }

        ::SendMessage(hwnd, SCI_INSERTTEXT, insertPos, (LPARAM)text.c_str());
        int finalPos = insertPos + (int)text.length();
        Utils::select(hwnd, finalPos, finalPos);
    }

    Utils::endUndo(hwnd);
}

void indent(HWND hwnd, int startLine, int endLine, int count) {
    if (!hwnd) return;
    int sL = (std::min)(startLine, endLine);
    int eL = (std::max)(startLine, endLine);

    Utils::beginUndo(hwnd);
    int startPos = Utils::lineStart(hwnd, sL);
    auto endRange = Utils::lineRange(hwnd, eL, true);
    Utils::select(hwnd, startPos, endRange.second);

    for (int i = 0; i < count; i++) {
        ::SendMessage(hwnd, SCI_TAB, 0, 0);
    }
    Utils::endUndo(hwnd);
}

void unindent(HWND hwnd, int startLine, int endLine, int count) {
    if (!hwnd) return;
    int sL = (std::min)(startLine, endLine);
    int eL = (std::max)(startLine, endLine);

    Utils::beginUndo(hwnd);
    int startPos = Utils::lineStart(hwnd, sL);
    auto endRange = Utils::lineRange(hwnd, eL, true);
    Utils::select(hwnd, startPos, endRange.second);

    for (int i = 0; i < count; i++) {
        ::SendMessage(hwnd, SCI_BACKTAB, 0, 0);
    }
    Utils::endUndo(hwnd);
}

void autoIndent(HWND hwnd, int startLine, int endLine) {
    if (!hwnd) return;
    int sL = (std::min)(startLine, endLine);
    int eL = (std::max)(startLine, endLine);

    Utils::beginUndo(hwnd);
    for (int line = sL; line <= eL; line++) {
        int lineStartPos = (int)::SendMessage(hwnd, SCI_POSITIONFROMLINE, line, 0);
        int lineEndPos = (int)::SendMessage(hwnd, SCI_GETLINEENDPOSITION, line, 0);

        int firstNonSpace = lineStartPos;
        while (firstNonSpace < lineEndPos) {
            char ch = (char)::SendMessage(hwnd, SCI_GETCHARAT, firstNonSpace, 0);
            if (ch != ' ' && ch != '\t') break;
            firstNonSpace++;
        }

        if (firstNonSpace > lineStartPos) {
            ::SendMessage(hwnd, SCI_DELETERANGE, lineStartPos, firstNonSpace - lineStartPos);
        }
    }
    Utils::endUndo(hwnd);
}

void uppercase(HWND hwnd, int start, int end) {
    if (!hwnd || start >= end) return;
    Utils::select(hwnd, start, end);
    ::SendMessage(hwnd, SCI_UPPERCASE, 0, 0);
}

void lowercase(HWND hwnd, int start, int end) {
    if (!hwnd || start >= end) return;
    Utils::select(hwnd, start, end);
    ::SendMessage(hwnd, SCI_LOWERCASE, 0, 0);
}

void toggleCase(HWND hwnd, int start, int end) {
    if (!hwnd || start >= end) return;
    Utils::beginUndo(hwnd);
    for (int pos = start; pos < end; pos++) {
        char ch = (char)::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        if (ch == '\r' || ch == '\n') continue;
        char repl = ch;
        if (std::islower(static_cast<unsigned char>(ch))) {
            repl = (char)std::toupper(static_cast<unsigned char>(ch));
        } else if (std::isupper(static_cast<unsigned char>(ch))) {
            repl = (char)std::tolower(static_cast<unsigned char>(ch));
        }
        if (repl != ch) {
            Utils::replaceChar(hwnd, pos, repl);
        }
    }
    Utils::endUndo(hwnd);
}

void rot13(HWND hwnd, int start, int end) {
    if (!hwnd || start >= end) return;
    Utils::beginUndo(hwnd);
    for (int pos = start; pos < end; pos++) {
        char ch = (char)::SendMessage(hwnd, SCI_GETCHARAT, pos, 0);
        char repl = ch;
        if (ch >= 'a' && ch <= 'z') {
            repl = 'a' + (ch - 'a' + 13) % 26;
        } else if (ch >= 'A' && ch <= 'Z') {
            repl = 'A' + (ch - 'A' + 13) % 26;
        }
        if (repl != ch) {
            Utils::replaceChar(hwnd, pos, repl);
        }
    }
    Utils::endUndo(hwnd);
}

void eraseBlock(HWND hwnd, const BlockSelection& blk, char reg, bool syncClipboard) {
    if (!hwnd) return;
    yankBlock(hwnd, blk, reg, syncClipboard);

    Utils::beginUndo(hwnd);
    ::SendMessage(hwnd, SCI_CLEAR, 0, 0);
    Utils::clearBlockSelection(hwnd);
    int pos = Utils::caretPos(hwnd);
    Utils::select(hwnd, pos, pos);
    Utils::endUndo(hwnd);
}

void yankBlock(HWND hwnd, const BlockSelection& blk, char reg, bool syncClipboard) {
    if (!hwnd) return;
    std::string text;
    for (int line = blk.startLine; line <= blk.endLine; line++) {
        int lineEnd = Utils::lineEnd(hwnd, line);
        int cs = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, line, blk.startCol);
        int ce = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, line, blk.endCol);
        if (cs > lineEnd) cs = lineEnd;
        if (ce > lineEnd) ce = lineEnd;
        if (ce > cs) {
            text += Utils::getTextRange(hwnd, cs, ce);
        }
        if (line < blk.endLine) text += "\n";
    }

    if (!text.empty()) {
        Registers::getInstance().set(reg, text, RegisterType::BlockWise, syncClipboard);
    }
}

void putBlock(HWND hwnd, const std::string& content, bool pasteAfter) {
    if (!hwnd || content.empty()) return;
    std::vector<std::string> lines = Utils::splitLines(content);
    if (lines.empty()) return;

    int caretPos = Utils::caretPos(hwnd);
    int startLine = (int)::SendMessage(hwnd, SCI_LINEFROMPOSITION, caretPos, 0);
    int startCol = (int)::SendMessage(hwnd, SCI_GETCOLUMN, caretPos, 0);

    int targetCol = pasteAfter ? (startCol + 1) : startCol;
    if (pasteAfter) {
        int lineStart = (int)::SendMessage(hwnd, SCI_POSITIONFROMLINE, startLine, 0);
        int lineEnd = Utils::lineEnd(hwnd, startLine);
        if (lineStart == lineEnd) {
            targetCol = 0;
        }
    }

    int totalLines = (int)::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);

    Utils::beginUndo(hwnd);

    for (size_t i = 0; i < lines.size(); i++) {
        int curLine = startLine + (int)i;
        
        if (curLine >= totalLines) {
            int docLen = (int)::SendMessage(hwnd, SCI_GETLENGTH, 0, 0);
            ::SendMessage(hwnd, SCI_SETCURRENTPOS, docLen, 0);
            ::SendMessage(hwnd, SCI_REPLACESEL, 0, (LPARAM)"\r\n");
            totalLines = (int)::SendMessage(hwnd, SCI_GETLINECOUNT, 0, 0);
        }

        int lineEnd = Utils::lineEnd(hwnd, curLine);
        int colPos = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, curLine, targetCol);

        int curLineEndCol = (int)::SendMessage(hwnd, SCI_GETCOLUMN, lineEnd, 0);
        if (curLineEndCol < targetCol) {
            int padCount = targetCol - curLineEndCol;
            std::string padding(padCount, ' ');
            ::SendMessage(hwnd, SCI_INSERTTEXT, lineEnd, (LPARAM)padding.c_str());
            colPos = lineEnd + padCount;
        }

        ::SendMessage(hwnd, SCI_INSERTTEXT, colPos, (LPARAM)lines[i].c_str());
    }

    int finalCaret = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, startLine, targetCol);
    Utils::select(hwnd, finalCaret, finalCaret);

    Utils::endUndo(hwnd);
}

void joinLines(HWND hwnd, int startLine, int count, bool withSpace) {
    if (!hwnd || count <= 0) return;
    Utils::beginUndo(hwnd);
    for (int i = 0; i < count; i++) {
        if (startLine + 1 >= Utils::lineCount(hwnd)) break;
        int end = Utils::lineEnd(hwnd, startLine);
        int next = Utils::lineStart(hwnd, startLine + 1);
        int lineEndNext = Utils::lineEnd(hwnd, startLine + 1);
        int nonSpaceNext = next;
        if (withSpace) {
            while (nonSpaceNext < lineEndNext) {
                char ch = (char)::SendMessage(hwnd, SCI_GETCHARAT, nonSpaceNext, 0);
                if (ch != ' ' && ch != '\t') break;
                nonSpaceNext++;
            }
        }
        Utils::select(hwnd, end, nonSpaceNext);
        ::SendMessage(hwnd, SCI_REPLACESEL, 0, (LPARAM)(withSpace ? " " : ""));
    }
    Utils::endUndo(hwnd);
}

void replaceRange(HWND hwnd, int start, int end, char ch) {
    if (!hwnd || start >= end) return;
    Utils::beginUndo(hwnd);
    for (int i = start; i < end; i++) {
        char c = (char)::SendMessage(hwnd, SCI_GETCHARAT, i, 0);
        if (c != '\r' && c != '\n') {
            Utils::replaceChar(hwnd, i, ch);
        }
    }
    Utils::endUndo(hwnd);
}

void replaceBlock(HWND hwnd, const BlockSelection& blk, char ch) {
    if (!hwnd) return;
    Utils::beginUndo(hwnd);
    for (int line = blk.startLine; line <= blk.endLine; line++) {
        int le = Utils::lineEnd(hwnd, line);
        int cs = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, line, blk.startCol);
        int ce = (int)::SendMessage(hwnd, SCI_FINDCOLUMN, line, blk.endCol);
        if (cs > le) cs = le;
        if (ce > le) ce = le;
        for (int p = cs; p < ce; p++) {
            char c = (char)::SendMessage(hwnd, SCI_GETCHARAT, p, 0);
            if (c != '\r' && c != '\n') {
                Utils::replaceChar(hwnd, p, ch);
            }
        }
    }
    Utils::endUndo(hwnd);
}

} // namespace EditorOps
