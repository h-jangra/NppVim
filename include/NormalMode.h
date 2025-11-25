#ifndef NORMALMODE_H
#define NORMALMODE_H

#include <windows.h>
#include <map>
#include <functional>
#include "NppVim.h"

class NormalMode {
public:
    explicit NormalMode(VimState& state);
    void enter();
    void enterInsertMode();
    void handleKey(HWND hwndEdit, char c);

    void handleTabCommand(HWND hwnd, int count);

    void handleTabReverseCommand(HWND hwnd, int count);

private:
    VimState& state;
    std::map<char, std::function<void(HWND, int)>> keyHandlers;

    void setupKeyHandlers();

    void handleInsert(HWND hwndEdit, int count);
    void handleAppend(HWND hwndEdit, int count);
    void handleAppendEnd(HWND hwndEdit, int count);
    void handleInsertStart(HWND hwndEdit, int count);
    void handleOpenBelow(HWND hwndEdit, int count);
    void handleOpenAbove(HWND hwndEdit, int count);

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

    void handleMotion(HWND hwndEdit, char motionChar, int count);
    void handleGCommand(HWND hwndEdit, int count);
    void handleTCommand(HWND hwndEdit, int count);
    void handleTReverseCommand(HWND hwndEdit, int count);

    void handleSearchForward(HWND hwndEdit, int count);
    void handleSearchNext(HWND hwndEdit, int count);
    void handleSearchPrevious(HWND hwndEdit, int count);
    void handleSearchWordForward(HWND hwndEdit, int count);
    void handleSearchWordBackward(HWND hwndEdit, int count);
    void handleFindChar(HWND hwndEdit, int count);
    void handleFindCharBack(HWND hwndEdit, int count);
    void handleRepeatFind(HWND hwndEdit, int count);
    void handleRepeatFindReverse(HWND hwndEdit, int count);

    void handleVisualChar(HWND hwndEdit, int count);
    void handleVisualLine(HWND hwndEdit, int count);

    void handleUndo(HWND hwndEdit, int count);
    void handleRepeat(HWND hwndEdit, int count);
    void handleCommandMode(HWND hwndEdit, int count);

    void handleJumpBack(HWND hwndEdit, int count);
    void handleJumpBackToLine(HWND hwndEdit, int count);

    void handleZCommand(HWND hwndEdit, int count);

    void handleToggleComment(HWND hwndEdit, int count);

    void deleteLineOnce(HWND hwndEdit);
    void yankLineOnce(HWND hwndEdit);
    void applyOperatorToMotion(HWND hwndEdit, char op, char motion, int count);

    void handleIndent(HWND hwndEdit, int count);
    void handleUnindent(HWND hwndEdit, int count);
    void handleAutoIndent(HWND hwndEdit, int count);
};

#endif // NORMALMODE_H
