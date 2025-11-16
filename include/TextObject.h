#pragma once
#include <windows.h>
#include <utility>
#include "NppVim.h"

struct VimState;

class TextObject {
public:
    static void apply(HWND hwndEdit, VimState& state, char op, char modifier, char object);

    static void handleWordTextObject(HWND hwndEdit, VimState& state, char op, bool inner, int count, bool bigWord);

private:
    static std::pair<int, int> getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner, int count);
    static std::pair<int, int> findSentenceBounds(HWND hwndEdit, int pos, bool inner);
    static std::pair<int, int> findParagraphBounds(HWND hwndEdit, int pos, bool inner);
    static std::pair<int, int> findBracketBounds(HWND hwndEdit, int pos, char openChar, char closeChar, bool inner);
    static std::pair<int, int> findTagBounds(HWND hwndEdit, int pos, bool inner);
    static std::pair<int, int> trimWhitespaceBounds(HWND hwndEdit, std::pair<int, int> bounds);
    static std::pair<int, int> expandToWordBoundaries(HWND hwndEdit, std::pair<int, int> bounds);
    static bool handleCustomTextObject(HWND hwndEdit, VimState& state, char op, bool inner, char object);
    static void executeTextObjectOperation(HWND hwndEdit, VimState& state, char op, int start, int end, int count);
};