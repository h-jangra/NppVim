#pragma once
#include "NppVim.h"
#include <windows.h>
#include <utility>

class TextObject {
public:
    static void apply(HWND hwndEdit, VimState& state, char op, char modifier, char object);

private:
    static void handleWordTextObject(HWND hwndEdit, VimState& state, char op, bool inner);
    static std::pair<int, int> getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner);
    static void performOperation(HWND hwndEdit, VimState& state, char op, int start, int end);
};
