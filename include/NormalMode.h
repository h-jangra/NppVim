#pragma once
#include "NppVim.h"
#include "Marks.h"
#include <windows.h>
#include <functional>
#include <map>

class Marks;

class NormalMode {
    public:
    NormalMode(VimState& state);

    void handleKey(HWND hwndEdit, char c);
    void enter();
    void enterInsertMode();

    private:

    VimState& state;

    using KeyHandler = std::function<void(HWND, int)>;
    std::map<char, KeyHandler> keyHandlers;

    void setupKeyHandlers();

    // Mode transitions
    void handleInsert(HWND hwndEdit, int count);
    void handleAppend(HWND hwndEdit, int count);
    void handleAppendEnd(HWND hwndEdit, int count);
    void handleInsertStart(HWND hwndEdit, int count);
    void handleOpenBelow(HWND hwndEdit, int count);
    void handleOpenAbove(HWND hwndEdit, int count);

    // Edit operations
    void handleDelete(HWND hwndEdit, int count);
    void handleYank(HWND hwndEdit, int count);
    void handleChange(HWND hwndEdit, int count);
    void handleDeleteChar(HWND hwndEdit, int count);
    void handleDeleteCharBack(HWND hwndEdit, int count);
    void handleDeleteToEnd(HWND hwndEdit, int count);
    void handleChangeToEnd(HWND hwndEdit, int count);
    void handleJoinLines(HWND hwndEdit, int count);
    void handleReplace(HWND hwndEdit, int count);
    void handleReplaceMode(HWND hwndEdit, int count);
    void handlePaste(HWND hwndEdit, int count);
    void handlePasteBefore(HWND hwndEdit, int count);

    // Motions
    void handleMotion(HWND hwndEdit, char motion, int count);
    void handleGCommand(HWND hwndEdit, int count);

    // Search
    void handleSearchForward(HWND hwndEdit, int count);
    void handleSearchNext(HWND hwndEdit, int count);
    void handleSearchPrevious(HWND hwndEdit, int count);
    void handleSearchWordForward(HWND hwndEdit, int count);
    void handleSearchWordBackward(HWND hwndEdit, int count);
    void handleFindChar(HWND hwndEdit, int count);
    void handleFindCharBack(HWND hwndEdit, int count);
    void handleRepeatFind(HWND hwndEdit, int count);
    void handleRepeatFindReverse(HWND hwndEdit, int count);

    // Visual mode
    void handleVisualChar(HWND hwndEdit, int count);
    void handleVisualLine(HWND hwndEdit, int count);

    // Other
    void handleUndo(HWND hwndEdit, int count);
    void handleRepeat(HWND hwndEdit, int count);
    void handleCommandMode(HWND hwndEdit, int count);
    void handleTabCommand(HWND hwnd, int count);
    void handleTabReverseCommand(HWND hwnd, int count);
    void handleJumpBack(HWND hwndEdit, int count);
    void handleJumpBackToLine(HWND hwndEdit, int count);

    // Helper functions
    void deleteLineOnce(HWND hwndEdit);
    void yankLineOnce(HWND hwndEdit);
    void applyOperatorToMotion(HWND hwndEdit, char op, char motion, int count);

};
