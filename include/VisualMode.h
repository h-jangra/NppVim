#pragma once

#include <windows.h>
#include <map>
#include <functional>

class VimState;

class VisualMode {
public:
    VisualMode(VimState& state);

    bool iswalnum(char c);

    void enterChar(HWND hwndEdit);
    void enterLine(HWND hwndEdit);
    void enterBlock(HWND hwndEdit);
    void handleVisualBlock(HWND hwndEdit, int count);
    void handleKey(HWND hwndEdit, char c);
    void setSelection(HWND hwndEdit);

private:
    VimState& state;

    using KeyHandler = std::function<void(HWND, int)>;
    std::map<char, KeyHandler> keyHandlers;

    void setupKeyHandlers();

    void handleDelete(HWND hwndEdit, int count);
    void handleYank(HWND hwndEdit, int count);
    void handleChange(HWND hwndEdit, int count);
    void handleSwapAnchor(HWND hwndEdit, int count);

    void handleVisualChar(HWND hwndEdit, int count);
    void handleVisualLine(HWND hwndEdit, int count);

    void handleMotion(HWND hwndEdit, char motion, int count);
    void handleBlockVisualMotion(HWND hwndEdit, char motionChar, int count);
    void handleBlockWordRight(HWND hwndEdit, bool bigWord);
    void handleBlockWordLeft(HWND hwndEdit, bool bigWord);
    void handleBlockWordEnd(HWND hwndEdit, bool bigWord);
    void handleStandardVisualMotion(HWND hwndEdit, char motionChar, int count);
    void updateVisualAnchor(HWND hwndEdit);
    void handleGCommand(HWND hwndEdit, int count);
    void handleFindChar(HWND hwndEdit, int count);
    void handleFindCharBack(HWND hwndEdit, int count);
    void handleTillChar(HWND hwndEdit, int count);
    void handleTillCharBack(HWND hwndEdit, int count);
    void handleRepeatFind(HWND hwndEdit, int count);
    void handleRepeatFindReverse(HWND hwndEdit, int count);

    void handleSearchForward(HWND hwndEdit, int count);
    void handleSearchNext(HWND hwndEdit, int count);
    void handleSearchPrevious(HWND hwndEdit, int count);

    void handleToggleComment(HWND hwndEdit, int count);
    void handleBlockInsert(HWND hwndEdit, int count);
    void handleBlockAppend(HWND hwndEdit, int count);
    void handleIndent(HWND hwndEdit, int count);
    void handleUnindent(HWND hwndEdit, int count);
    void handleAutoIndent(HWND hwndEdit, int count);
};
