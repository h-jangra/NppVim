//TextObject.cpp
#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../plugin/Scintilla.h"
#include <cctype>

extern NormalMode* g_normalMode;

void TextObject::apply(HWND hwndEdit, VimState& state, char op, char modifier, char object) {

    bool inner = (modifier == 'i');
    int count = state.repeatCount > 0 ? state.repeatCount : 1;
    state.repeatCount = 0;

    TextObjectType objType = TEXT_OBJECT_WORD;
    char quoteChar = 0;

    switch (object) {
    case 'w': case 'W':
        handleWordTextObject(hwndEdit, state, op, inner, count);
        return;

    case 's': objType = TEXT_OBJECT_SENTENCE; break;
    case 'p': objType = TEXT_OBJECT_PARAGRAPH; break;
    case '\'': quoteChar = '\''; break;
    case '"': quoteChar = '"'; break;
    case '`': quoteChar = '`'; break;
    case '(': case ')': objType = TEXT_OBJECT_PAREN; break;
    case '[': case ']': objType = TEXT_OBJECT_BRACKET; break;
    case '{': case '}': objType = TEXT_OBJECT_BRACE; break;
	case '<': case '>': objType = TEXT_OBJECT_ANGLE; break;
	case 't':
        if (handleCustomTextObject(hwndEdit, state, op, inner, object)) return;
        break;
    default: return;
    }

    // Handle quotes
    if (quoteChar != 0) {
        auto bounds = Utils::findQuoteBounds(hwndEdit,
            (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), quoteChar);
        if (bounds.first == -1 || bounds.second == -1) return;

        int start = bounds.first;
        int end = bounds.second + 1;

        if (inner && start < end) { start++; end--; }
        if (start < end) executeTextObjectOperation(hwndEdit, state, op, start, end);
        return;
    }

    // All other text objects
    auto bounds = getTextObjectBounds(hwndEdit, objType, inner, count);
    if (bounds.first < bounds.second) executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second);
}

void TextObject::handleWordTextObject(HWND hwndEdit, VimState& state, char op, bool inner, int count) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    int start = pos, end = pos;
    for (int i = 0; i < count; i++) {
        auto bounds = Utils::findWordBounds(hwndEdit, end);
        if (bounds.first == bounds.second && end < docLen)
            bounds = Utils::findWordBounds(hwndEdit, end + 1);

        if (bounds.first == bounds.second) break;
        if (i == 0) start = bounds.first;
        end = bounds.second;

        if (i < count - 1) {
            int nextPos = end;
            while (nextPos < docLen) {
                char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, nextPos, 0);
                if (!std::isspace(static_cast<unsigned char>(ch))) break;
                nextPos++;
            }
            end = nextPos;
        }
    }

    if (start == end) return;

    if (!inner) {
        auto expanded = expandToWordBoundaries(hwndEdit, { start, end });
        start = expanded.first;
        end = expanded.second;
    }

    if (start < end) executeTextObjectOperation(hwndEdit, state, op, start, end);
}

std::pair<int, int> TextObject::getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner, int count) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    switch (objType) {
    case TEXT_OBJECT_WORD: {
        auto bounds = Utils::findWordBounds(hwndEdit, pos);
        return inner ? trimWhitespaceBounds(hwndEdit, bounds) : expandToWordBoundaries(hwndEdit, bounds);
    }

    case TEXT_OBJECT_BIG_WORD: {
        int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1);
        int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1);
        return inner ? std::make_pair(start, end) : expandToWordBoundaries(hwndEdit, { start, end });
    }

    case TEXT_OBJECT_SENTENCE: {
        return TextObject::findSentenceBounds(hwndEdit, pos, inner);
    }

    case TEXT_OBJECT_PARAGRAPH: {
        return TextObject::findParagraphBounds(hwndEdit, pos, inner);
    }

    case TEXT_OBJECT_PAREN: return findBracketBounds(hwndEdit, pos, '(', ')', inner);
    case TEXT_OBJECT_BRACKET: return findBracketBounds(hwndEdit, pos, '[', ']', inner);
    case TEXT_OBJECT_BRACE: return findBracketBounds(hwndEdit, pos, '{', '}', inner);
    case TEXT_OBJECT_ANGLE: return findBracketBounds(hwndEdit, pos, '<', '>', inner);
    default: return { pos, pos };
    }
}

std::pair<int, int> TextObject::findSentenceBounds(HWND hwndEdit, int pos, bool inner) {
    // Get current line
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);

    // Get line start and end
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    return { lineStart, lineEnd };
}

std::pair<int, int> TextObject::findParagraphBounds(HWND hwndEdit, int pos, bool inner) {
    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);

    // Helper lambda to check if a line is blank (empty or only whitespace)
    auto isBlankLine = [&](int line) -> bool {
        if (line < 0 || line >= lineCount) return true;

        int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

        // Check if line is empty
        if (lineStart >= lineEnd) return true;

        // Check if line contains only whitespace
        for (int i = lineStart; i < lineEnd; i++) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (!std::isspace((unsigned char)ch)) {
                return false;
            }
        }
        return true;
        };

    // Find start of paragraph (look backward for blank line)
    int startLine = currentLine;
    while (startLine > 0 && !isBlankLine(startLine - 1)) {
        startLine--;
    }

    // Find end of paragraph (look forward for blank line)
    int endLine = currentLine;
    while (endLine < lineCount - 1 && !isBlankLine(endLine + 1)) {
        endLine++;
    }

    // Get positions
    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);

    // For 'ap' (around paragraph), include surrounding blank lines
    if (!inner) {
        // Try to include blank line after paragraph
        if (endLine < lineCount - 1 && isBlankLine(endLine + 1)) {
            endLine++;
            end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);
        }
        // If no blank line after, try to include blank line before
        else if (startLine > 0 && isBlankLine(startLine - 1)) {
            startLine--;
            start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        }
    }

    return { start, end };
}

std::pair<int, int> TextObject::findBracketBounds(HWND hwndEdit, int pos, char openChar, char closeChar, bool inner) {
    int match = Utils::findMatchingBracket(hwndEdit, pos, openChar, closeChar);
    if (match == -1) return { pos, pos };

    int start = pos, end = match + 1;
    if (match < pos) {
        start = match;
        end = pos;
        int closeMatch = Utils::findMatchingBracket(hwndEdit, start, openChar, closeChar);
        if (closeMatch != -1) end = closeMatch + 1;
    }
    else {
        int openMatch = Utils::findMatchingBracket(hwndEdit, end - 1, openChar, closeChar);
        if (openMatch != -1) start = openMatch;
    }

    if (inner && start < end) { start++; end--; }
    return { start, end };
}

std::pair<int, int> TextObject::findTagBounds(HWND hwndEdit, int pos, bool inner) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    int tagStart = pos;
    while (tagStart > 0 && (char)::SendMessage(hwndEdit, SCI_GETCHARAT, tagStart, 0) != '<') tagStart--;
    if (tagStart < 0 || (char)::SendMessage(hwndEdit, SCI_GETCHARAT, tagStart, 0) != '<') return { pos, pos };
    int tagEnd = pos;
    while (tagEnd < docLen && (char)::SendMessage(hwndEdit, SCI_GETCHARAT, tagEnd, 0) != '>') tagEnd++;
    if (tagEnd >= docLen || (char)::SendMessage(hwndEdit, SCI_GETCHARAT, tagEnd, 0) != '>') return { pos, pos };
    return inner ? std::make_pair(tagStart + 1, tagEnd) : std::make_pair(tagStart, tagEnd + 1);
}

std::pair<int, int> TextObject::trimWhitespaceBounds(HWND hwndEdit, std::pair<int, int> bounds) {
    int start = bounds.first, end = bounds.second;
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    while (start < end && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, start, 0))) start++;
    while (end > start && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, end - 1, 0))) end--;
    return { start, end };
}

std::pair<int, int> TextObject::expandToWordBoundaries(HWND hwndEdit, std::pair<int, int> bounds) {
    int start = bounds.first, end = bounds.second;
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    while (start > 0 && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0))) start--;
    while (end < docLen && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0))) end++;
    return { start, end };
}

bool TextObject::handleCustomTextObject(HWND hwndEdit, VimState& state, char op, bool inner, char object) {
    if (object == 't') {
        auto bounds = findTagBounds(hwndEdit, (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), inner);
        if (bounds.first < bounds.second) {
            executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second);
            return true;
        }
    }
    return false;
}

void TextObject::executeTextObjectOperation(HWND hwndEdit, VimState& state, char op, int start, int end) {
    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);

    switch (op) {
    case 'd': // delete
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
        state.recordLastOp(OP_MOTION, state.repeatCount, 'd');
        if (state.mode == VISUAL && g_normalMode) {
            g_normalMode->enter();
        }
        break;

    case 'c': // change
        ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        state.recordLastOp(OP_MOTION, state.repeatCount, 'c');
        if (g_normalMode) g_normalMode->enterInsertMode();
        break;

    case 'y': // yank
        ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
        ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
        state.recordLastOp(OP_MOTION, state.repeatCount, 'y');
        if (state.mode == VISUAL && g_normalMode) {
            g_normalMode->enter();
        }
        break;

    case 'v': // visual select
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        break;
    }
}