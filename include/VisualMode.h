#pragma once

#include <windows.h>
#include <map>
#include <functional>

class VimState;

class VisualMode {
public:
    VisualMode(VimState& state);

    void enterChar(HWND hwndEdit);
    void enterLine(HWND hwndEdit);
    void handleKey(HWND hwndEdit, char c);
    void setSelection(HWND hwndEdit);

private:
    VimState& state;

    using KeyHandler = std::function<void(HWND, int)>;
    std::map<char, KeyHandler> keyHandlers;

    void setupKeyHandlers();

    // Operations
    void handleDelete(HWND hwndEdit, int count);
    void handleYank(HWND hwndEdit, int count);
    void handleChange(HWND hwndEdit, int count);
    void handleSwapAnchor(HWND hwndEdit, int count);

    // Mode switches
    void handleVisualChar(HWND hwndEdit, int count);
    void handleVisualLine(HWND hwndEdit, int count);

    // Motions
    void handleMotion(HWND hwndEdit, char motion, int count);
    void updateVisualAnchor(HWND hwndEdit);
    void handleGCommand(HWND hwndEdit, int count);
    void handleFindChar(HWND hwndEdit, int count);
    void handleFindCharBack(HWND hwndEdit, int count);
    void handleTillChar(HWND hwndEdit, int count);
    void handleTillCharBack(HWND hwndEdit, int count);
    void handleRepeatFind(HWND hwndEdit, int count);
    void handleRepeatFindReverse(HWND hwndEdit, int count);
};
