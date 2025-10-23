#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../plugin/Scintilla.h"

extern NormalMode* g_normalMode;

void TextObject::apply(HWND hwndEdit, VimState& state, char op, char modifier, char object) {
    bool inner = (modifier == 'i');

    TextObjectType objType;
    char quoteChar = 0;

    // Map character to text object type
    switch (object) {
        case 'w':
            handleWordTextObject(hwndEdit, state, op, inner);
            return;
        case 'W': objType = TEXT_OBJECT_BIG_WORD; break;
        case '\'': quoteChar = '\''; break;
        case '"': quoteChar = '"'; break;
        case '`': quoteChar = '`'; break;
        case '(': case ')': objType = TEXT_OBJECT_PAREN; break;
        case '[': case ']': objType = TEXT_OBJECT_BRACKET; break;
        case '{': case '}': objType = TEXT_OBJECT_BRACE; break;
        case '<': case '>': objType = TEXT_OBJECT_ANGLE; break;
        default: return;
    }

    // Special handling for quotes
    if (quoteChar != 0) {
        auto bounds = Utils::findQuoteBounds(hwndEdit,
            (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), quoteChar);
        if (bounds.first == -1 || bounds.second == -1) return;

        int start = bounds.first;
        int end = bounds.second + 1;

        if (inner) {
            start++;
            end--;
        }

        if (start < end) {
            ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
            performOperation(hwndEdit, state, op, start, end);
        }
        return;
    }

    // For other text objects
    auto bounds = getTextObjectBounds(hwndEdit, objType, inner);
    if (bounds.first < bounds.second) {
        ::SendMessage(hwndEdit, SCI_SETSEL, bounds.first, bounds.second);
        performOperation(hwndEdit, state, op, bounds.first, bounds.second);
    }
}

void TextObject::handleWordTextObject(HWND hwndEdit, VimState& state, char op, bool inner) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    auto bounds = Utils::findWordBounds(hwndEdit, pos);

    if (bounds.first == bounds.second) {
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (pos < docLen) {
            bounds = Utils::findWordBounds(hwndEdit, pos + 1);
        }
    }

    if (bounds.first == bounds.second) return;

    int start = bounds.first;
    int end = bounds.second;
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    if (!inner) {
        while (start > 0) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0);
            if (!std::isspace(static_cast<unsigned char>(ch))) break;
            start--;
        }

        while (end < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0);
            if (!std::isspace(static_cast<unsigned char>(ch))) {
                if (end > bounds.second) break;
            }
            end++;
        }
    } else {
        start = bounds.first;
        end = bounds.second;
    }

    if (start < end) {
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        performOperation(hwndEdit, state, op, start, end);
    }
}

std::pair<int, int> TextObject::getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int start = pos, end = pos;

    switch (objType) {
        case TEXT_OBJECT_WORD: {
            auto bounds = Utils::findWordBounds(hwndEdit, pos);
            start = bounds.first;
            end = bounds.second;
            if (inner && start < end) {
                int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

                while (start < end) {
                    char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start, 0);
                    if (!std::isspace(static_cast<unsigned char>(ch))) break;
                    start++;
                }

                while (end > start) {
                    char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end - 1, 0);
                    if (!std::isspace(static_cast<unsigned char>(ch))) break;
                    end--;
                }
            }
            break;
        }

        case TEXT_OBJECT_BIG_WORD: {
            start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
            end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
            break;
        }

        case TEXT_OBJECT_PAREN: {
            int match = Utils::findMatchingBracket(hwndEdit, pos, '(', ')');
            if (match != -1) {
                if (match < pos) {
                    start = match;
                    end = pos;
                    int closeMatch = Utils::findMatchingBracket(hwndEdit, start, '(', ')');
                    if (closeMatch != -1) end = closeMatch + 1;
                } else {
                    start = pos;
                    end = match + 1;
                    int openMatch = Utils::findMatchingBracket(hwndEdit, end - 1, '(', ')');
                    if (openMatch != -1) start = openMatch;
                }

                if (inner && start < end) {
                    start++;
                    end--;
                }
            }
            break;
        }

        case TEXT_OBJECT_BRACE: {
            int match = Utils::findMatchingBracket(hwndEdit, pos, '{', '}');
            if (match != -1) {
                if (match < pos) {
                    start = match;
                    end = pos;
                    int closeMatch = Utils::findMatchingBracket(hwndEdit, start, '{', '}');
                    if (closeMatch != -1) end = closeMatch + 1;
                } else {
                    start = pos;
                    end = match + 1;
                    int openMatch = Utils::findMatchingBracket(hwndEdit, end - 1, '{', '}');
                    if (openMatch != -1) start = openMatch;
                }

                if (inner && start < end) {
                    start++;
                    end--;
                }
            }
            break;
        }

        case TEXT_OBJECT_BRACKET: {
            int match = Utils::findMatchingBracket(hwndEdit, pos, '[', ']');
            if (match != -1) {
                if (match < pos) {
                    start = match;
                    end = pos;
                    int closeMatch = Utils::findMatchingBracket(hwndEdit, start, '[', ']');
                    if (closeMatch != -1) end = closeMatch + 1;
                } else {
                    start = pos;
                    end = match + 1;
                    int openMatch = Utils::findMatchingBracket(hwndEdit, end - 1, '[', ']');
                    if (openMatch != -1) start = openMatch;
                }

                if (inner && start < end) {
                    start++;
                    end--;
                }
            }
            break;
        }

        case TEXT_OBJECT_ANGLE: {
            int match = Utils::findMatchingBracket(hwndEdit, pos, '<', '>');
            if (match != -1) {
                if (match < pos) {
                    start = match;
                    end = pos;
                    int closeMatch = Utils::findMatchingBracket(hwndEdit, start, '<', '>');
                    if (closeMatch != -1) end = closeMatch + 1;
                } else {
                    start = pos;
                    end = match + 1;
                    int openMatch = Utils::findMatchingBracket(hwndEdit, end - 1, '<', '>');
                    if (openMatch != -1) start = openMatch;
                }

                if (inner && start < end) {
                    start++;
                    end--;
                }
            }
            break;
        }

        default:
            break;
    }

    return { start, end };
}

void TextObject::performOperation(HWND hwndEdit, VimState& state, char op, int start, int end) {
    switch (op) {
        case 'd':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
            state.recordLastOp(OP_MOTION, state.repeatCount, 'd');
            break;

        case 'c':
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
            if (g_normalMode) {
                g_normalMode->enterInsertMode();
            }
            state.recordLastOp(OP_MOTION, state.repeatCount, 'c');
            break;

        case 'y':
            ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
            ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
            state.recordLastOp(OP_MOTION, state.repeatCount, 'y');
            break;
    }
}
