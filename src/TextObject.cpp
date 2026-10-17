// TextObject.cpp
#include "../include/TextObject.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../include/VisualMode.h"
#include "../include/Registers.h"
#include "../include/EditorOps.h"
#include "../plugin/Scintilla.h"
#include <cctype>

extern NormalMode* g_normalMode;
extern VisualMode* g_visualMode;

void TextObject::apply(HWND hwndEdit, VimState& state, char op, char modifier, char object) {
    bool inner = (modifier == 'i');
    int count = state.repeatCount > 0 ? state.repeatCount : 1;
    state.repeatCount = 0;

    auto record = [&]() {
        state.lastOp.type = OP_MOTION;
        state.lastOp.count = count;
        state.lastOp.motion = op;
        state.lastOp.textModifier = modifier;
        state.lastOp.textObject = object;
    };

    TextObjectType objType = TEXT_OBJECT_WORD;
    char quoteChar = 0;
    char bracketChar = 0;

    switch (object) {
    case 'w':
        handleWordTextObject(hwndEdit, state, op, inner, count, false);
        record();
        return;
    case 'W':
        handleWordTextObject(hwndEdit, state, op, inner, count, true);
        record();
        return;

    case 's': objType = TEXT_OBJECT_SENTENCE; break;
    case 'p': objType = TEXT_OBJECT_PARAGRAPH; break;
    case '\'': quoteChar = '\''; break;
    case '\"': quoteChar = '\"'; break;
    case '`': quoteChar = '`'; break;
    case '(':
    case ')':
    case 'b':
        objType = TEXT_OBJECT_PAREN;
        bracketChar = '(';
        break;
    case '[':
    case ']':
        objType = TEXT_OBJECT_BRACKET;
        bracketChar = object;
        break;
    case '{':
    case '}':
    case 'B':
        objType = TEXT_OBJECT_BRACE;
        bracketChar = '{';
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
            if (start > end) return;
        }
        if (start <= end) {
            executeTextObjectOperation(hwndEdit, state, op, start, end, count);
            record();
        }
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

        auto bounds = findBracketBounds(hwndEdit, (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0), openChar, closeChar, inner, count);
        if (bounds.first <= bounds.second && bounds.first != -1) {
            executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second, count);
            record();
        }
        return;
    }

    auto bounds = getTextObjectBounds(hwndEdit, objType, inner, count);
    if (bounds.first < bounds.second) {
        executeTextObjectOperation(hwndEdit, state, op, bounds.first, bounds.second, count);
        record();
    }
}

std::pair<int, int> TextObject::getTextObjectBounds(HWND hwndEdit, TextObjectType objType, bool inner, int count) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);

    std::pair<int, int> bounds;
    
    switch (objType) {
    case TEXT_OBJECT_WORD:
        bounds = Utils::findWordBounds(hwndEdit, pos);
        bounds = inner ? trimWhitespaceBounds(hwndEdit, bounds) : expandToWordBoundaries(hwndEdit, bounds);
        break;
    case TEXT_OBJECT_BIG_WORD:
        bounds = std::make_pair(
            (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 1),
            (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 1)
        );
        bounds = inner ? trimWhitespaceBounds(hwndEdit, bounds) : expandToWordBoundaries(hwndEdit, bounds);
        break;
    case TEXT_OBJECT_SENTENCE:
        bounds = findSentenceBounds(hwndEdit, pos, inner);
        break;
    case TEXT_OBJECT_PARAGRAPH:
        bounds = findParagraphBounds(hwndEdit, pos, inner);
        break;
    case TEXT_OBJECT_PAREN:
        bounds = findBracketBounds(hwndEdit, pos, '(', ')', inner, count);
        break;
    case TEXT_OBJECT_BRACKET:
        bounds = findBracketBounds(hwndEdit, pos, '[', ']', inner, count);
        break;
    case TEXT_OBJECT_BRACE:
        bounds = findBracketBounds(hwndEdit, pos, '{', '}', inner, count);
        break;
    case TEXT_OBJECT_ANGLE:
        bounds = findBracketBounds(hwndEdit, pos, '<', '>', inner, count);
        break;
    default:
        bounds = { pos, pos };
    }

    if (count > 1 && (objType == TEXT_OBJECT_SENTENCE || objType == TEXT_OBJECT_PARAGRAPH)) {
        for (int i = 1; i < count; i++) {
            auto nextBounds = getTextObjectBounds(hwndEdit, objType, inner, 1);
            if (nextBounds.first >= nextBounds.second) break;
            bounds.second = nextBounds.second;
        }
    }

    return bounds;
}

std::pair<int, int> TextObject::findBracketBounds(HWND hwndEdit, int pos, char openChar, char closeChar, bool inner, int count) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETTEXTLENGTH, 0, 0);
    if (docLen <= 0 || pos < 0 || pos >= docLen) return { pos, pos };

    auto findMatchingForward = [&](int openPos) -> int {
        int m = (int)::SendMessage(hwndEdit, SCI_BRACEMATCH, openPos, 0);
        if (m != -1 && m > openPos && (char)::SendMessage(hwndEdit, SCI_GETCHARAT, m, 0) == closeChar) {
            return m;
        }
        int depth = 1;
        for (int p = openPos + 1; p < docLen; p++) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, p, 0);
            if (c == openChar) {
                depth++;
            } else if (c == closeChar) {
                depth--;
                if (depth == 0) return p;
            }
        }
        return -1;
    };

    auto findMatchingBackward = [&](int closePos) -> int {
        int m = (int)::SendMessage(hwndEdit, SCI_BRACEMATCH, closePos, 0);
        if (m != -1 && m < closePos && (char)::SendMessage(hwndEdit, SCI_GETCHARAT, m, 0) == openChar) {
            return m;
        }
        int depth = 1;
        for (int p = closePos - 1; p >= 0; p--) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, p, 0);
            if (c == closeChar) {
                depth++;
            } else if (c == openChar) {
                depth--;
                if (depth == 0) return p;
            }
        }
        return -1;
    };

    int startPos = -1;
    int endPos = -1;

    char chAtPos = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);

    // 1. Cursor is directly on openChar
    if (chAtPos == openChar) {
        int match = findMatchingForward(pos);
        if (match != -1) {
            int selStart = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
            int selEnd = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
            bool alreadyOuterSelected = (!inner && selStart == pos && selEnd == match + 1);
            if (!alreadyOuterSelected) {
                startPos = pos;
                endPos = match;
            }
        }
    }
    // 2. Cursor is directly on closeChar
    else if (chAtPos == closeChar) {
        int match = findMatchingBackward(pos);
        if (match != -1) {
            int selStart = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONSTART, 0, 0);
            int selEnd = (int)::SendMessage(hwndEdit, SCI_GETSELECTIONEND, 0, 0);
            bool alreadyInnerSelected = (inner && selStart == match + 1 && selEnd == pos);
            bool alreadyOuterSelected = (!inner && selStart == match && selEnd == pos + 1);
            if (!alreadyInnerSelected && !alreadyOuterSelected) {
                startPos = match;
                endPos = pos;
            }
        }
    }

    // 3. Cursor is inside a pair: scan backwards from pos - 1 to find innermost enclosing pair
    if (startPos == -1) {
        int depth = 0;
        for (int p = pos - 1; p >= 0; p--) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, p, 0);
            if (c == closeChar) {
                depth++;
            } else if (c == openChar) {
                if (depth > 0) {
                    depth--;
                } else {
                    int match = findMatchingForward(p);
                    if (match >= pos) {
                        startPos = p;
                        endPos = match;
                        break;
                    }
                }
            }
        }
    }

    // 4. Fallback: Cursor is before a pair on the current line
    if (startPos == -1) {
        int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);
        for (int p = pos + 1; p <= lineEnd; p++) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, p, 0);
            if (c == openChar) {
                int match = findMatchingForward(p);
                if (match != -1) {
                    startPos = p;
                    endPos = match;
                    break;
                }
            }
        }
    }

    if (startPos == -1 || endPos == -1) {
        return { pos, pos };
    }

    // Handle count > 1 by expanding to enclosing outer pairs
    for (int i = 1; i < count; i++) {
        int nextStart = -1;
        int nextEnd = -1;
        int depth = 0;
        for (int p = startPos - 1; p >= 0; p--) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, p, 0);
            if (c == closeChar) {
                depth++;
            } else if (c == openChar) {
                if (depth > 0) {
                    depth--;
                } else {
                    int match = findMatchingForward(p);
                    if (match >= endPos) {
                        nextStart = p;
                        nextEnd = match;
                        break;
                    }
                }
            }
        }
        if (nextStart != -1 && nextEnd != -1) {
            startPos = nextStart;
            endPos = nextEnd;
        } else {
            break;
        }
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

    if (bigWord) {
        int len = docLen;
        int start = pos;
        int end = pos;

        auto is_space = [&](int p) {
            return std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, p, 0));
        };

        while (start > 0 && !is_space(start - 1))
            start--;

        end = start;
        while (end < len && !is_space(end))
            end++;

        if (count > 1) {
            int remaining = count - 1;
            while (remaining > 0 && end < len) {
                int temp = end;
                while (temp < len && is_space(temp))
                    temp++;
                
                if (temp >= len) break;
                
                while (temp < len && !is_space(temp))
                    temp++;
                
                end = temp;
                remaining--;
            }
        }

        if (inner) {
            while (start < end && is_space(start)) start++;
            while (end > start && is_space(end - 1)) end--;
        }

        executeTextObjectOperation(hwndEdit, state, op, start, end, count);
        return;
    }

    int start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, pos, 0);
    int end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, pos, 0);
    
    if (start == end && pos < docLen) {
        int nextPos = (int)::SendMessage(hwndEdit, SCI_POSITIONAFTER, pos, 0);
        if (nextPos < docLen) {
            start = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, nextPos, 0);
            end = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, nextPos, 0);
        }
    }

    if (inner) {
        while (start < end && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, start, 0))) start++;
        while (end > start && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, end - 1, 0))) end--;
    } else {
        int origStart = start;
        int origEnd = end;
        
        while (start > 0 && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0))) start--;
        while (end < docLen && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0))) end++;
        
        if (start == origStart && end == origEnd && start > 0) {
            while (start > 0 && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, start - 1, 0))) start--;
        }
    }

    for (int i = 1; i < count; i++) {
        int nextPos = end;
        while (nextPos < docLen && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, nextPos, 0))) nextPos++;
        if (nextPos >= docLen) break;
        
        int nextStart = (int)::SendMessage(hwndEdit, SCI_WORDSTARTPOSITION, nextPos, 0);
        int nextEnd = (int)::SendMessage(hwndEdit, SCI_WORDENDPOSITION, nextPos, 0);
        
        if (nextStart >= nextEnd) break;
        
        end = nextEnd;
        
        if (!inner) {
            while (end < docLen && std::isspace((unsigned char)::SendMessage(hwndEdit, SCI_GETCHARAT, end, 0))) end++;
        }
    }

    if (start < end) {
        executeTextObjectOperation(hwndEdit, state, op, start, end, count);
    }
}

std::pair<int, int> TextObject::findSentenceBounds(HWND h, int pos, bool) {
    int line = Utils::caretLine(h);
    int start = Utils::lineStart(h, line);
    int end = Utils::lineEnd(h, line);
    return { start, end };
}

std::pair<int, int> TextObject::findParagraphBounds(HWND h, int pos, bool inner) {
    int line = Utils::caretLine(h);
    int total = ::SendMessage(h, SCI_GETLINECOUNT, 0, 0);

    auto blank = [&](int l) {
        int s = ::SendMessage(h, SCI_POSITIONFROMLINE, l, 0);
        int e = ::SendMessage(h, SCI_GETLINEENDPOSITION, l, 0);
        for (int i = s; i < e; i++)
            if (!std::isspace(::SendMessage(h, SCI_GETCHARAT, i, 0)))
                return false;
        return true;
    };

    int s = line;
    while (s > 0 && !blank(s - 1)) s--;

    int e = line;
    while (e < total - 1 && !blank(e + 1)) e++;

    if (!inner) {
        if (s > 0 && blank(s - 1)) s--;
        if (e < total - 1 && blank(e + 1)) e++;
    }

    return {
        (int)::SendMessage(h, SCI_POSITIONFROMLINE, s, 0),
        (int)::SendMessage(h, SCI_GETLINEENDPOSITION, e, 0)
    };
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

void TextObject::executeTextObjectOperation(HWND h, VimState& state, char op, int start, int end, int count) {
    if (start > end) return;
    if (start == end) {
        if (op == 'c') {
            ::SendMessage(h, SCI_SETCURRENTPOS, start, 0);
            ::SendMessage(h, SCI_SETANCHOR, start, 0);
            state.recordLastOp(OP_MOTION, count, 'c');
            if (g_normalMode) g_normalMode->enterInsertMode();
        } else if (op == 'v') {
            state.mode = VISUAL;
            state.isLineVisual = false;
            state.isBlockVisual = false;
            state.visualAnchor = start;
            state.visualAnchorLine = (int)::SendMessage(h, SCI_LINEFROMPOSITION, start, 0);
            ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
            ::SendMessage(h, SCI_SETANCHOR, start, 0);
            ::SendMessage(h, SCI_SETCURRENTPOS, start, 0);
            Utils::select(h, start, start);
            state.lastVisualAnchor = start;
            state.lastVisualCaret = start;
            state.lastVisualWasBlock = false;
            state.lastVisualWasLine = false;
            Utils::setStatus(TEXT("-- VISUAL --"));
            ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
        }
        return;
    }

    if (op == 'v') {
        state.mode = VISUAL;
        state.isLineVisual = false;
        state.isBlockVisual = false;
        state.visualAnchor = start;
        state.visualAnchorLine = (int)::SendMessage(h, SCI_LINEFROMPOSITION, start, 0);
        ::SendMessage(h, SCI_SETSELECTIONMODE, SC_SEL_STREAM, 0);
        ::SendMessage(h, SCI_SETANCHOR, start, 0);
        ::SendMessage(h, SCI_SETCURRENTPOS, end, 0);
        Utils::select(h, start, end);
        state.lastVisualAnchor = start;
        state.lastVisualCaret = end;
        state.lastVisualWasBlock = false;
        state.lastVisualWasLine = false;
        Utils::setStatus(TEXT("-- VISUAL --"));
        ::SendMessage(h, SCI_SCROLLCARET, 0, 0);
        return;
    }

    char reg = Registers::getInstance().getActiveRegister();
    if (state.deleteToBlackhole && op != 'y') reg = '_';

    EditRange r = EditRange::fromPositions(start, end, false);

    switch (op) {
    case 'd':
        EditorOps::erase(h, r, reg, g_config.dStoreClipboard);
        state.recordLastOp(OP_MOTION, count, 'd');
        break;

    case 'c':
        EditorOps::change(h, r, reg, g_config.cStoreClipboard);
        state.recordLastOp(OP_MOTION, count, 'c');
        if (g_normalMode) g_normalMode->enterInsertMode();
        break;

    case 'y':
        EditorOps::yank(h, r, reg, true);
        Utils::select(h, start, start);
        state.recordLastOp(OP_MOTION, count, 'y');
        break;
    }
}
