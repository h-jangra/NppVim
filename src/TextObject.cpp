// TextObject.cpp
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
    char bracketChar = 0;

    switch (object) {
    case 'w':
        handleWordTextObject(hwndEdit, state, op, inner, count, false);
        return;
    case 'W':
        handleWordTextObject(hwndEdit, state, op, inner, count, true);
        return;

    case 's': objType = TEXT_OBJECT_SENTENCE; break;
    case 'p': objType = TEXT_OBJECT_PARAGRAPH; break;
    case '\'': quoteChar = '\''; break;
    case '\"': quoteChar = '\"'; break;
    case '`': quoteChar = '`'; break;
    case '(':
    case ')':
        objType = TEXT_OBJECT_PAREN;
        bracketChar = object;
        break;
    case '[':
    case ']':
        objType = TEXT_OBJECT_BRACKET;
        bracketChar = object;
        break;
    case '{':
    case '}':
        objType = TEXT_OBJECT_BRACE;
        bracketChar = object;
        break;
    case '<':
    case '>':
        objType = TEXT_OBJECT_ANGLE;
        bracketChar = object;
        break;
    case 't':
        if (handleCustomTextObject(hwndEdit, state, op, inner, object)) return;
        break;
    default: return;
    }

    if (quoteChar != 0) {
        auto bounds = Utils::findQuoteBounds(hwndEdit,
            (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), quoteChar);
        if (bounds.first == -1 || bounds.second == -1) return;

        int start = bounds.first;
        int end = bounds.second + 1;

        if (inner && start < end) {
            start++;
            end--;
            if (start >= end) return;
        }
        if (start < end) executeTextObjectOperation(hwndEdit, state, op, start, end, count);
        return;
    }

    if (bracketChar != 0) {
        char openChar, closeChar;
        switch (bracketChar) {
        case '(': case ')': openChar = '('; closeChar = ')'; break;
        case '[': case ']': openChar = '['; closeChar = ']'; break;
        case '{': case '}': openChar = '{'; closeChar = '}'; break;
        case '<': case '>': openChar = '<'; closeChar = '>'; break;
        default: return;
        }

        auto bounds = findBracketBounds(hwndEdit, (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), openChar, closeChar, inner);
        if (bounds.first < bounds.second) {
            executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second, count);
        }
        return;
    }

    auto bounds = getTextObjectBounds(hwndEdit, objType, inner, count);
    if (bounds.first < bounds.second) {
        executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second, count);
    }
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
    case TEXT_OBJECT_PAREN:
        return findBracketBounds(hwndEdit, pos, '(', ')', inner);
    case TEXT_OBJECT_BRACKET:
        return findBracketBounds(hwndEdit, pos, '[', ']', inner);
    case TEXT_OBJECT_BRACE:
        return findBracketBounds(hwndEdit, pos, '{', '}', inner);
    case TEXT_OBJECT_ANGLE:
        return findBracketBounds(hwndEdit, pos, '<', '>', inner);
    default:
        return { pos, pos };
    }
}

std::pair<int, int> TextObject::findBracketBounds(HWND hwndEdit, int pos, char openChar, char closeChar, bool inner) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    int startPos = -1;
    int endPos = -1;

    int searchPos = pos;
    int bracketCount = 0;
    bool foundOpening = false;

    while (searchPos >= 0) {
        char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
        if (ch == closeChar) {
            bracketCount++;
        }
        else if (ch == openChar) {
            if (bracketCount == 0) {
                startPos = searchPos;
                foundOpening = true;
                break;
            }
            else {
                bracketCount--;
            }
        }
        searchPos--;
    }

    if (foundOpening) {
        bracketCount = 1;
        searchPos = startPos + 1;

        while (searchPos < docLen && bracketCount > 0) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (ch == openChar) {
                bracketCount++;
            }
            else if (ch == closeChar) {
                bracketCount--;
                if (bracketCount == 0) {
                    endPos = searchPos;
                    break;
                }
            }
            searchPos++;
        }
    }

    if (startPos == -1 || endPos == -1) {
        return { pos, pos };
    }

    if (inner) {
        startPos++;
    }
    else {
        endPos++;
    }

    return { startPos, endPos };
}

void TextObject::handleWordTextObject(HWND hwndEdit, VimState& state, char op, bool inner, int count, bool bigWord) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);

    if (pos >= docLen) return;

    char charAtPos = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
    while (pos < docLen && (charAtPos == ' ' || charAtPos == '\t' || charAtPos == '\r' || charAtPos == '\n')) {
        pos++;
        if (pos < docLen) {
            charAtPos = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
        }
    }

    if (pos >= docLen) return;

    auto bounds = Utils::findWordBoundsEx(hwndEdit, pos, bigWord);
    int start = bounds.first;
    int end = bounds.second;

    if (start >= end) return;

    for (int i = 1; i < count; i++) {
        int nextPos = end;
        while (nextPos < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, nextPos, 0);
            if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')) {
                break;
            }
            nextPos++;
        }

        if (nextPos >= docLen) break;

        auto nextBounds = Utils::findWordBoundsEx(hwndEdit, nextPos, bigWord);
        if (nextBounds.first >= nextBounds.second) break;
        end = nextBounds.second;
    }

    if (!inner) {
        int originalEnd = end;

        while (end < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0);
            if (!(ch == ' ' || ch == '\t')) {
                break;
            }
            end++;
        }

        if (end == originalEnd && start > 0) {
            while (start > 0) {
                char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0);
                if (!(ch == ' ' || ch == '\t')) {
                    break;
                }
                start--;
            }
        }
    }

    if (start < end) {
        executeTextObjectOperation(hwndEdit, state, op, start, end, count);
    }
}

std::pair<int, int> TextObject::findSentenceBounds(HWND hwndEdit, int pos, bool inner) {
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
    return { lineStart, lineEnd };
}

std::pair<int, int> TextObject::findParagraphBounds(HWND hwndEdit, int pos, bool inner) {
    int currentLine = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);

    auto isBlankLine = [&](int line) -> bool {
        if (line < 0 || line >= lineCount) return true;
        int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
        if (lineStart >= lineEnd) return true;
        for (int i = lineStart; i < lineEnd; i++) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, i, 0);
            if (!std::isspace((unsigned char)ch)) {
                return false;
            }
        }
        return true;
        };

    int startLine = currentLine;
    while (startLine > 0 && !isBlankLine(startLine - 1)) {
        startLine--;
    }

    int endLine = currentLine;
    while (endLine < lineCount - 1 && !isBlankLine(endLine + 1)) {
        endLine++;
    }

    int start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);

    if (!inner) {
        if (endLine < lineCount - 1 && isBlankLine(endLine + 1)) {
            endLine++;
            end = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, endLine, 0);
        }
        else if (startLine > 0 && isBlankLine(startLine - 1)) {
            startLine--;
            start = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, startLine, 0);
        }
    }

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
            executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second, 1);
            return true;
        }
    }
    return false;
}

void TextObject::executeTextObjectOperation(HWND hwndEdit, VimState& state, char op, int start, int end, int count) {
    if (start >= end) return;

    ::SendMessage(hwndEdit, SCI_SETSEL, start, end);

    switch (op) {
    case 'd':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        }
        ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, start, 0);
        state.recordLastOp(OP_MOTION, count, 'd');
        if (state.mode == VISUAL && g_normalMode) {
            g_normalMode->enter();
        }
        break;

    case 'c':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_CLEAR, 0, 0);
        }
        state.recordLastOp(OP_MOTION, count, 'c');
        if (g_normalMode) g_normalMode->enterInsertMode();
        break;

    case 'y':
        for (int i = 0; i < count; i++) {
            ::SendMessage(hwndEdit, SCI_COPYRANGE, start, end);
        }
        ::SendMessage(hwndEdit, SCI_SETSEL, start, start);
        state.recordLastOp(OP_MOTION, count, 'y');
        if (state.mode == VISUAL && g_normalMode) {
            g_normalMode->enter();
        }
        break;

    case 'v':
        ::SendMessage(hwndEdit, SCI_SETSEL, start, end);
        break;
    }
}