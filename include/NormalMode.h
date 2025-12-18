#pragma once

#include "NppVim.h"
#include "Keymap.h"
#include "Motion.h"
#include <windows.h>

class NormalMode {
public:
    NormalMode(VimState& state);
    
    void enter();
    void handleKey(HWND hwnd, char c);
    void enterInsertMode();
    
private:
    VimState& state;
    Motion motion;
    
    void setupKeyMaps();
    
    void deleteLineOnce(HWND hwnd);
    void yankLineOnce(HWND hwnd);
    void applyOperatorToMotion(HWND hwnd, char op, char motion, int count);
    
    void handleCharSearchInput(HWND hwnd, char searchChar, char searchType, int count);
    void handleMarkSetInput(HWND hwnd, char mark);
    void handleMarkJumpInput(HWND hwnd, char mark, bool exactPosition);
    void handleReplaceInput(HWND hwnd, char replaceChar);
};

extern NormalMode* g_normalMode;