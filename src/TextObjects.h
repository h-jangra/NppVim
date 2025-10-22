#pragma once

enum TextObjectType {
    TEXT_OBJECT_WORD,
    TEXT_OBJECT_BIG_WORD,
    TEXT_OBJECT_QUOTE,
    TEXT_OBJECT_BRACKET,
    TEXT_OBJECT_PAREN,
    TEXT_OBJECT_ANGLE,
    TEXT_OBJECT_BRACE,
    TEXT_OBJECT_SENTENCE,
    TEXT_OBJECT_PARAGRAPH
};

int findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar);
std::pair<int, int> findQuoteBounds(HWND hwndEdit, int pos, char quoteChar);
std::pair<int, int> getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner);
void applyTextObjectOperation(HWND hwndEdit, char op, bool inner, char object);
void performTextObjectOperation(HWND hwndEdit, char op, int start, int end);
void handleTextObjectCommand(HWND hwndEdit, char op, char modifier, char object);
void handleWordTextObject(HWND hwndEdit, char op, bool inner);
