#pragma once

#include "NppVim.h"
#include "Keymap.h"
#include "Motion.h"
#include <windows.h>

class VisualMode {
public:
    VisualMode(VimState& state);
    
    void enterChar(HWND hwnd);
    void enterLine(HWND hwnd);
    void enterBlock(HWND hwnd);
    void handleKey(HWND hwnd, char c);
    void visualMoveCursor(HWND hwndEdit, int newPos);
    
private:
    VimState& state;
    Motion motion;
    
    void setupKeyMaps();
    void exitToNormal(HWND hwnd);
    
    void handleCharSearchInput(HWND hwnd, char searchChar, char searchType, int count);
    
    bool iswalnum(char c);
    std::string getSelectedText(HWND h);
    void updateBlockAfterMove(HWND h, int newCaret);
    void handleBlockWordRight(HWND hwnd, bool bigWord);
    void handleBlockWordLeft(HWND hwnd, bool bigWord);
    void handleBlockWordEnd(HWND hwnd, bool bigWord);

    void extendSelection(HWND hwndEdit, int newPos);
    void setSelection(HWND hwndEdit, int startPos, int endPos);
    void moveCursor(HWND hwndEdit, int newPos, bool extend = false);
    void handleVisualReplaceInput(HWND hwnd, char replaceChar);
};

extern VisualMode* g_visualMode;