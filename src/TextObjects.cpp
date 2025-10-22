#include "NppVim.h"

// Text Object Functions implementation
int findMatchingBracket(HWND hwndEdit, int pos, char openChar, char closeChar) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    if (pos >= docLen) return -1;

    char currentChar = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);

    // If on closing bracket, search backwards
    if (currentChar == closeChar) {
        int depth = 1;
        for (int i = pos - 1; i >= 0; i--) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (ch == closeChar) depth++;
            else if (ch == openChar) {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    // If on opening bracket
    else {
        int depth = 1;
        for (int i = pos + 1; i < docLen; i++) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (ch == openChar) depth++;
            else if (ch == closeChar) {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    return -1;
}
std::pair<int, int> findQuoteBounds(HWND hwndEdit, int pos, char quoteChar) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int start = -1, end = -1;

    // Search backwards for opening quote
    for (int i = pos; i >= 0; i--) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
        if (ch == quoteChar) {
            start = i;
            break;
        }
    }

    // Search forwards for closing quote
    if (start != -1) {
        for (int i = start + 1; i < docLen; i++) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (ch == quoteChar) {
                end = i;
                break;
            }
        }
    }

    return { start, end };
}
std::pair<int, int> getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int start = pos, end = pos;

    switch (objType) {
    case TEXT_OBJECT_WORD: {
        auto bounds = findWordBounds(hwndEdit, pos);
        start = bounds.first;
        end = bounds.second;
        if (inner && start < end) {
            // For inner word, exclude surrounding whitespace
            int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

            // Trim left whitespace
            while (start < end) {
                char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start, 0);
                if (!std::isspace(static_cast<unsigned char>(ch))) break;
                start++;
            }

            // Trim right whitespace
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
        int match = findMatchingBracket(hwndEdit, pos, '(', ')');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                // Find the closing paren
                int closeMatch = findMatchingBracket(hwndEdit, start, '(', ')');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                // Find the opening paren
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '(', ')');
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
        int match = findMatchingBracket(hwndEdit, pos, '{', '}');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                int closeMatch = findMatchingBracket(hwndEdit, start, '{', '}');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '{', '}');
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
        int match = findMatchingBracket(hwndEdit, pos, '[', ']');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                int closeMatch = findMatchingBracket(hwndEdit, start, '[', ']');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '[', ']');
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
        int match = findMatchingBracket(hwndEdit, pos, '<', '>');
        if (match != -1) {
            if (match < pos) {
                start = match;
                end = pos;
                int closeMatch = findMatchingBracket(hwndEdit, start, '<', '>');
                if (closeMatch != -1) end = closeMatch + 1;
            }
            else {
                start = pos;
                end = match + 1;
                int openMatch = findMatchingBracket(hwndEdit, end - 1, '<', '>');
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
void applyTextObjectOperation(HWND hwndEdit, char op, bool inner, char object) {
    TextObjectType objType;
    char quoteChar = 0;

    // Map character to text object type
    switch (object) {
    case 'w':
        handleWordTextObject(hwndEdit, op, inner);
        return;
    case 'W': objType = TEXT_OBJECT_BIG_WORD; break;
    case '\'':
        quoteChar = '\'';
        break;
    case '"':
        quoteChar = '"';
        break;
    case '`':
        quoteChar = '`';
        break;
    case '(': case ')': objType = TEXT_OBJECT_PAREN; break;
    case '[': case ']': objType = TEXT_OBJECT_BRACKET; break;
    case '{': case '}': objType = TEXT_OBJECT_BRACE; break;
    case '<': case '>': objType = TEXT_OBJECT_ANGLE; break;
    default: return;
    }

    // Special handling for quotes
    if (quoteChar != 0) {
        auto bounds = findQuoteBounds(hwndEdit,
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
            performTextObjectOperation(hwndEdit, op, start, end);
        }
        return;
    }

    // For other text objects
    auto bounds = getTextObjectBounds(hwndEdit, objType, inner);
    if (bounds.first < bounds.second) {
        ::SendMessage(hwndEdit, SCI_SETSEL, bounds.first, bounds.second);
        performTextObjectOperation(hwndEdit, op, bounds.first, bounds.second);
    }
}

void performTextObjectOperation(HWND hwndEdit, char op, int start, int end) {
    switch (op) {
    case 'd':
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
        recordLastOp(OP_MOTION, state.repeatCount, 'd');
        break;

    case 'c':
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        enterInsertMode();
        recordLastOp(OP_MOTION, state.repeatCount, 'c');
        break;

    case 'y':
        ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
        recordLastOp(OP_MOTION, state.repeatCount, 'y');
        break;
    }
}
void handleTextObjectCommand(HWND hwndEdit, char op, char modifier, char object) {
    bool inner = (modifier == 'i');
    applyTextObjectOperation(hwndEdit, op, inner, object);
}
void handleWordTextObject(HWND hwndEdit, char op, bool inner) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    auto bounds = findWordBounds(hwndEdit, pos);

    if (bounds.first == bounds.second) {
        int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
        if (pos < docLen) {
            bounds = findWordBounds(hwndEdit, pos + 1);
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
                if (end > bounds.second) {
                    break;
                }
            }
            end++;
        }
    }
    else {
        start = bounds.first;
        end = bounds.second;
    }

    if (start < end) {
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        performTextObjectOperation(hwndEdit, op, start, end);
    }
}

void applyTextObject(HWND hwndEdit, char op, char modifier, char object) {
    handleTextObjectCommand(hwndEdit, op, modifier, object);
}
