#pragma once
#include <windows.h>
#include <string>
#include "Registers.h"
#include "Utils.h"

struct EditRange {
    int start = 0;
    int end = 0;
    bool linewise = false;
    bool blockwise = false;
    BlockSelection block = { 0, 0, 0, 0 };

    static EditRange fromPositions(int s, int e, bool isLinewise = false) {
        EditRange r;
        r.start = (std::min)(s, e);
        r.end = (std::max)(s, e);
        r.linewise = isLinewise;
        return r;
    }

    static EditRange fromLines(HWND hwnd, int startLine, int endLine) {
        EditRange r;
        int sL = (std::min)(startLine, endLine);
        int eL = (std::max)(startLine, endLine);
        r.start = Utils::lineStart(hwnd, sL);
        auto endR = Utils::lineRange(hwnd, eL, true);
        r.end = endR.second;
        r.linewise = true;
        return r;
    }

    static EditRange fromBlock(const BlockSelection& blk) {
        EditRange r;
        r.blockwise = true;
        r.block = blk;
        return r;
    }
};

namespace EditorOps {
    // Core editing operations
    void yank(HWND hwnd, const EditRange& range, char reg = '"', bool syncClipboard = true);
    void erase(HWND hwnd, const EditRange& range, char reg = '"', bool syncClipboard = true);
    void change(HWND hwnd, const EditRange& range, char reg = '"', bool syncClipboard = true);
    void put(HWND hwnd, int pos, char reg = '"', bool before = false, bool forceLinewise = false);

    // Indentation
    void indent(HWND hwnd, int startLine, int endLine, int count = 1);
    void unindent(HWND hwnd, int startLine, int endLine, int count = 1);
    void autoIndent(HWND hwnd, int startLine, int endLine);

    // Transformations
    void uppercase(HWND hwnd, int start, int end);
    void lowercase(HWND hwnd, int start, int end);
    void toggleCase(HWND hwnd, int start, int end);
    void rot13(HWND hwnd, int start, int end);

    // Block operations
    void eraseBlock(HWND hwnd, const BlockSelection& blk, char reg = '"', bool syncClipboard = true);
    void yankBlock(HWND hwnd, const BlockSelection& blk, char reg = '"', bool syncClipboard = true);
    void putBlock(HWND hwnd, const std::string& content, bool pasteAfter);

    // Line & character operations
    void joinLines(HWND hwnd, int startLine, int count, bool withSpace = true);
    void replaceRange(HWND hwnd, int start, int end, char ch);
    void replaceBlock(HWND hwnd, const BlockSelection& blk, char ch);
}
