#include "../include/Marks.h"
#include "../include/Utils.h"
#include "../include/NppVim.h"
#include "../plugin/Scintilla.h"
#include "../plugin/Notepad_plus_msgs.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <set>

extern NppData nppData;
extern VimState state;

std::map<std::string, std::map<char, MarkInfo>> Marks::localMarksByFile;
std::map<char, MarkInfo> Marks::globalMarks;
std::map<char, MarkInfo> Marks::numberedMarks;
MarkInfo Marks::lastJumpMark;
MarkInfo Marks::lastChangeMark;
MarkInfo Marks::lastInsertMark;
MarkInfo Marks::changeStartMark;
MarkInfo Marks::changeEndMark;

void Marks::initializeMarkers(HWND hwndEdit) {
    if (!hwndEdit) return;

    // Local marks (a-z): Marker 20 - Small circle
    int localMarkerNum = 20;
    ::SendMessage(hwndEdit, SCI_MARKERDEFINE, localMarkerNum, SC_MARK_CIRCLE);
    ::SendMessage(hwndEdit, SCI_MARKERSETFORE, localMarkerNum, RGB(0, 120, 215)); // Blue
    ::SendMessage(hwndEdit, SCI_MARKERSETBACK, localMarkerNum, RGB(255, 255, 255));

    // Global marks (A-Z): Marker 21 - Small rectangle  
    int globalMarkerNum = 21;
    ::SendMessage(hwndEdit, SCI_MARKERDEFINE, globalMarkerNum, SC_MARK_SMALLRECT);
    ::SendMessage(hwndEdit, SCI_MARKERSETFORE, globalMarkerNum, RGB(215, 0, 0)); // Red
    ::SendMessage(hwndEdit, SCI_MARKERSETBACK, globalMarkerNum, RGB(255, 255, 255));

    // Ensure the margin is visible
    ::SendMessage(hwndEdit, SCI_SETMARGINWIDTHN, 1, 16);
    ::SendMessage(hwndEdit, SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);

    // No line highlighting
    ::SendMessage(hwndEdit, SCI_MARKERENABLEHIGHLIGHT, 0, 0);
}

bool Marks::isValidSetMark(char mark) {
    return (mark >= 'a' && mark <= 'z') || (mark >= 'A' && mark <= 'Z');
}

bool Marks::isValidMark(char mark) {
    return (mark >= 'a' && mark <= 'z') ||  // Local marks
           (mark >= 'A' && mark <= 'Z') ||  // Global marks
           (mark >= '0' && mark <= '9') ||  // Numbered marks
           mark == '.' ||                   // Last change position
           mark == '\'' || mark == '`' ||   // Last jump position
           mark == '<' || mark == '>' ||   // Visual selection start/end
           mark == '[' || mark == ']' ||   // Change/yank start/end
           mark == '^' ||                   // Last insert exit
           mark == '"';                     // Last buffer exit
}

int Marks::getMarkerNumber(char mark) {
    if (mark >= 'a' && mark <= 'z') {
        return 20; // Circle marker for local marks
    } else if (mark >= 'A' && mark <= 'Z') {
        return 21; // Rectangle marker for global marks
    }
    return -1;
}

std::string Marks::getCurrentFilename() {
    return Utils::getCurrentFilePath();
}

void Marks::recordLastChange(HWND hwndEdit) {
    if (!hwndEdit) return;
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    lastChangeMark = MarkInfo(line, pos - lineStart, getCurrentFilename(), false);
}

void Marks::recordLastJump(HWND hwndEdit) {
    if (!hwndEdit) return;
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    lastJumpMark = MarkInfo(line, pos - lineStart, getCurrentFilename(), false);
}

void Marks::recordLastInsert(HWND hwndEdit) {
    if (!hwndEdit) return;
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    lastInsertMark = MarkInfo(line, pos - lineStart, getCurrentFilename(), false);
}

void Marks::recordChangeRange(HWND hwndEdit, int startLine, int startCol, int endLine, int endCol) {
    std::string file = getCurrentFilename();
    changeStartMark = MarkInfo(startLine, startCol, file, false);
    changeEndMark = MarkInfo(endLine, endCol, file, false);
}

void Marks::setMark(HWND hwndEdit, char mark) {
    if (!hwndEdit || !isValidSetMark(mark)) return;

    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
    int column = pos - lineStart;

    setMarkAtLine(hwndEdit, mark, line, column);
}

void Marks::setMarkAtLine(HWND hwndEdit, char mark, int line, int column) {
    if (!isValidSetMark(mark)) return;

    std::string curFile = getCurrentFilename();
    MarkInfo markInfo(line, column, curFile, (mark >= 'A' && mark <= 'Z'));

    if (mark >= 'a' && mark <= 'z') {
        localMarksByFile[curFile][mark] = markInfo;
        if (hwndEdit) updateMarkerDisplay(hwndEdit, mark);

        char statusMsg[64];
        sprintf_s(statusMsg, "Mark '%c' set at line %d", mark, line + 1);
        Utils::setStatus(std::wstring(statusMsg, statusMsg + strlen(statusMsg)).c_str());
    }
    else if (mark >= 'A' && mark <= 'Z') {
        globalMarks[mark] = markInfo;
        if (hwndEdit) updateMarkerDisplay(hwndEdit, mark);

        char statusMsg[64];
        sprintf_s(statusMsg, "Global mark '%c' set at line %d", mark, line + 1);
        Utils::setStatus(std::wstring(statusMsg, statusMsg + strlen(statusMsg)).c_str());
    }
}

void Marks::updateMarkerDisplay(HWND hwndEdit, char mark) {
    if (!hwndEdit) return;

    int markerNum = getMarkerNumber(mark);
    if (markerNum == -1) return;

    std::string curFile = getCurrentFilename();
    int line = -1;

    if (mark >= 'a' && mark <= 'z') {
        auto fileIt = localMarksByFile.find(curFile);
        if (fileIt == localMarksByFile.end()) return;
        auto markIt = fileIt->second.find(mark);
        if (markIt == fileIt->second.end()) return;
        line = markIt->second.line;
    } else if (mark >= 'A' && mark <= 'Z') {
        auto markIt = globalMarks.find(mark);
        if (markIt == globalMarks.end()) return;
        if (markIt->second.filename != curFile) return; // only display in that file
        line = markIt->second.line;
    }

    if (line >= 0) {
        ::SendMessage(hwndEdit, SCI_MARKERDELETE, line, markerNum);
        ::SendMessage(hwndEdit, SCI_MARKERADD, line, markerNum);
    }
}

void Marks::removeMarkerDisplay(HWND hwndEdit, char mark) {
    if (!hwndEdit) return;

    int markerNum = getMarkerNumber(mark);
    if (markerNum == -1) return;

    std::string curFile = getCurrentFilename();
    int line = -1;

    if (mark >= 'a' && mark <= 'z') {
        auto fileIt = localMarksByFile.find(curFile);
        if (fileIt != localMarksByFile.end()) {
            auto markIt = fileIt->second.find(mark);
            if (markIt != fileIt->second.end()) {
                line = markIt->second.line;
            }
        }
    } else if (mark >= 'A' && mark <= 'Z') {
        auto markIt = globalMarks.find(mark);
        if (markIt != globalMarks.end() && markIt->second.filename == curFile) {
            line = markIt->second.line;
        }
    }

    if (line >= 0) {
        ::SendMessage(hwndEdit, SCI_MARKERDELETE, line, markerNum);
    }
}

bool Marks::jumpToMark(HWND hwndEdit, char mark, bool isBacktick) {
    if (!hwndEdit || !isValidMark(mark)) {
        Utils::setStatus(TEXT("Invalid mark"));
        return false;
    }

    MarkInfo markInfo;
    bool found = false;
    std::string currentFile = getCurrentFilename();

    if (mark >= 'a' && mark <= 'z') {
        auto fileIt = localMarksByFile.find(currentFile);
        if (fileIt != localMarksByFile.end()) {
            auto markIt = fileIt->second.find(mark);
            if (markIt != fileIt->second.end()) {
                markInfo = markIt->second;
                found = true;
            }
        }
        if (!found) {
            Utils::setStatus(TEXT("Local mark not set"));
            return false;
        }
    }
    else if (mark >= 'A' && mark <= 'Z') {
        auto markIt = globalMarks.find(mark);
        if (markIt != globalMarks.end()) {
            markInfo = markIt->second;
            found = true;
        }
        if (!found) {
            Utils::setStatus(TEXT("Global mark not set"));
            return false;
        }
    }
    else if (mark >= '0' && mark <= '9') {
        auto markIt = numberedMarks.find(mark);
        if (markIt != numberedMarks.end()) {
            markInfo = markIt->second;
            found = true;
        }
        if (!found) {
            Utils::setStatus(TEXT("Mark not set"));
            return false;
        }
    }
    else if (mark == '.') {
        if (lastChangeMark.line != -1) {
            markInfo = lastChangeMark;
            found = true;
        } else {
            Utils::setStatus(TEXT("Last change mark not set"));
            return false;
        }
    }
    else if (mark == '\'' || mark == '`' || mark == '"') {
        if (lastJumpMark.line != -1) {
            markInfo = lastJumpMark;
            found = true;
        } else {
            Utils::setStatus(TEXT("Previous jump mark not set"));
            return false;
        }
    }
    else if (mark == '^') {
        if (lastInsertMark.line != -1) {
            markInfo = lastInsertMark;
            found = true;
        } else {
            Utils::setStatus(TEXT("Last insert mark not set"));
            return false;
        }
    }
    else if (mark == '[') {
        if (changeStartMark.line != -1) {
            markInfo = changeStartMark;
            found = true;
        } else {
            Utils::setStatus(TEXT("Mark '[' not set"));
            return false;
        }
    }
    else if (mark == ']') {
        if (changeEndMark.line != -1) {
            markInfo = changeEndMark;
            found = true;
        } else {
            Utils::setStatus(TEXT("Mark ']' not set"));
            return false;
        }
    }
    else if (mark == '<' || mark == '>') {
        if (state.lastVisualAnchor != -1 && state.lastVisualCaret != -1) {
            int anchor = state.lastVisualAnchor;
            int caret = state.lastVisualCaret;
            int pos = (mark == '<') ? (std::min)(anchor, caret) : (std::max)(anchor, caret);
            int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
            int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);
            markInfo = MarkInfo(line, pos - lineStart, currentFile, false);
            found = true;
        } else {
            Utils::setStatus(TEXT("Visual mark not set"));
            return false;
        }
    }

    if (!found || markInfo.line < 0) {
        Utils::setStatus(TEXT("Mark not set"));
        return false;
    }

    // Save previous position before jump
    recordLastJump(hwndEdit);

    // Switch files if needed for global marks
    bool fileSwitched = false;
    if (!markInfo.filename.empty() && markInfo.filename != currentFile) {
#ifdef UNICODE
        std::wstring wideFilename = Utils::toWide(markInfo.filename);
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)wideFilename.c_str());
#else
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)markInfo.filename.c_str());
#endif

        hwndEdit = Utils::getCurrentScintillaHandle();
        if (!hwndEdit) {
            Utils::setStatus(TEXT("Failed to switch file"));
            return false;
        }
        fileSwitched = true;
    }

    int lineCount = (int)::SendMessage(hwndEdit, SCI_GETLINECOUNT, 0, 0);
    if (markInfo.line >= lineCount) {
        Utils::setStatus(TEXT("Mark line out of range"));
        return false;
    }

    int targetPos;
    if (isBacktick) {
        int linePos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, markInfo.line, 0);
        int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, markInfo.line, 0);
        targetPos = linePos + (markInfo.column >= 0 ? markInfo.column : 0);
        if (targetPos > lineEnd) targetPos = lineEnd;
    } else {
        targetPos = (int)::SendMessage(hwndEdit, SCI_GETLINEINDENTPOSITION, markInfo.line, 0);
        if (targetPos < 0) {
            targetPos = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, markInfo.line, 0);
        }
    }

    ::SendMessage(hwndEdit, SCI_GOTOPOS, targetPos, 0);
    ::SendMessage(hwndEdit, SCI_SCROLLCARET, 0, 0);

    if (fileSwitched) {
        ::SendMessage(hwndEdit, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
        int caret = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, caret, caret);
        Utils::setStatus(TEXT("-- NORMAL --"));
    }

    char statusMsg[64];
    sprintf_s(statusMsg, "Jumped to mark '%c' at line %d", mark, markInfo.line + 1);
    Utils::setStatus(std::wstring(statusMsg, statusMsg + strlen(statusMsg)).c_str());

    return true;
}

void Marks::deleteMark(HWND hwndEdit, char mark) {
    if (!isValidMark(mark)) return;

    std::string curFile = getCurrentFilename();
    if (mark >= 'a' && mark <= 'z') {
        removeMarkerDisplay(hwndEdit, mark);
        auto fileIt = localMarksByFile.find(curFile);
        if (fileIt != localMarksByFile.end()) {
            fileIt->second.erase(mark);
        }
    } else if (mark >= 'A' && mark <= 'Z') {
        removeMarkerDisplay(hwndEdit, mark);
        globalMarks.erase(mark);
    } else if (mark >= '0' && mark <= '9') {
        numberedMarks.erase(mark);
    }
}

int Marks::deleteMarks(HWND hwndEdit, const std::string& args) {
    std::string trimmed = Utils::trim(args);
    if (trimmed.empty()) return 0;

    int deletedCount = 0;
    size_t i = 0;
    while (i < trimmed.size()) {
        if (trimmed[i] == ' ' || trimmed[i] == '\t') {
            i++;
            continue;
        }

        // Check for range like a-d or A-Z or 0-9
        if (i + 2 < trimmed.size() && trimmed[i + 1] == '-') {
            char startC = trimmed[i];
            char endC = trimmed[i + 2];
            if (startC <= endC) {
                for (char c = startC; c <= endC; c++) {
                    if (isValidMark(c)) {
                        deleteMark(hwndEdit, c);
                        deletedCount++;
                    }
                }
            }
            i += 3;
        } else {
            char c = trimmed[i];
            if (isValidMark(c)) {
                deleteMark(hwndEdit, c);
                deletedCount++;
            }
            i++;
        }
    }
    return deletedCount;
}

void Marks::clearLocalMarks(HWND hwndEdit) {
    std::string curFile = getCurrentFilename();
    auto fileIt = localMarksByFile.find(curFile);
    if (fileIt != localMarksByFile.end()) {
        for (const auto& pair : fileIt->second) {
            removeMarkerDisplay(hwndEdit, pair.first);
        }
        fileIt->second.clear();
    }
    if (hwndEdit) {
        ::SendMessage(hwndEdit, SCI_MARKERDELETEALL, 20, 0);
    }
}

void Marks::clearAllMarks(HWND hwndEdit) {
    if (hwndEdit) {
        ::SendMessage(hwndEdit, SCI_MARKERDELETEALL, 20, 0);
        ::SendMessage(hwndEdit, SCI_MARKERDELETEALL, 21, 0);
    }
    localMarksByFile.clear();
    globalMarks.clear();
    numberedMarks.clear();
    lastChangeMark = MarkInfo(-1, -1, "", false);
    lastJumpMark = MarkInfo(-1, -1, "", false);
    lastInsertMark = MarkInfo(-1, -1, "", false);
    changeStartMark = MarkInfo(-1, -1, "", false);
    changeEndMark = MarkInfo(-1, -1, "", false);
}

std::string Marks::listMarks(HWND hwndEdit, const std::string& filter) {
    std::set<char> filterSet;
    std::string trimmedFilter = Utils::trim(filter);
    if (!trimmedFilter.empty()) {
        size_t i = 0;
        while (i < trimmedFilter.size()) {
            if (trimmedFilter[i] == ' ' || trimmedFilter[i] == '\t') {
                i++;
                continue;
            }
            if (i + 2 < trimmedFilter.size() && trimmedFilter[i + 1] == '-') {
                char startC = trimmedFilter[i];
                char endC = trimmedFilter[i + 2];
                if (startC <= endC) {
                    for (char c = startC; c <= endC; c++) filterSet.insert(c);
                }
                i += 3;
            } else {
                filterSet.insert(trimmedFilter[i]);
                i++;
            }
        }
    }

    auto shouldInclude = [&](char m) -> bool {
        if (filterSet.empty()) return true;
        return filterSet.find(m) != filterSet.end();
    };

    auto getLineSnippet = [&](HWND h, int line, const std::string& filename) -> std::string {
        if (!h) return "";
        std::string curFile = getCurrentFilename();
        if (!filename.empty() && filename != curFile) {
            return "[" + filename + "]";
        }
        int total = (int)::SendMessage(h, SCI_GETLINECOUNT, 0, 0);
        if (line < 0 || line >= total) return "";
        int startPos = (int)::SendMessage(h, SCI_POSITIONFROMLINE, line, 0);
        int endPos = (int)::SendMessage(h, SCI_GETLINEENDPOSITION, line, 0);
        std::string text = Utils::getTextRange(h, startPos, endPos);
        // Trim leading and trailing whitespace and truncate if long
        size_t first = text.find_first_not_of(" \t\r\n");
        if (first != std::string::npos) text = text.substr(first);
        while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
        if (text.size() > 40) text = text.substr(0, 37) + "...";
        return text;
    };

    std::ostringstream oss;
    oss << "mark line  col file/text\n";
    oss << "──── ───── ─── ──────────────────────────────────────────\n";

    int count = 0;
    std::string curFile = getCurrentFilename();

    // 1. Previous jump mark '
    if (lastJumpMark.line != -1 && shouldInclude('\'')) {
        oss << " '   " << std::setw(5) << (lastJumpMark.line + 1)
            << " " << std::setw(3) << lastJumpMark.column
            << " " << getLineSnippet(hwndEdit, lastJumpMark.line, lastJumpMark.filename) << "\n";
        count++;
    }

    // 2. Local marks a-z
    auto fileIt = localMarksByFile.find(curFile);
    if (fileIt != localMarksByFile.end()) {
        for (const auto& pair : fileIt->second) {
            if (shouldInclude(pair.first)) {
                oss << " " << pair.first << "   " << std::setw(5) << (pair.second.line + 1)
                    << " " << std::setw(3) << pair.second.column
                    << " " << getLineSnippet(hwndEdit, pair.second.line, pair.second.filename) << "\n";
                count++;
            }
        }
    }

    // 3. Global marks A-Z
    for (const auto& pair : globalMarks) {
        if (shouldInclude(pair.first)) {
            oss << " " << pair.first << "   " << std::setw(5) << (pair.second.line + 1)
                << " " << std::setw(3) << pair.second.column
                << " " << getLineSnippet(hwndEdit, pair.second.line, pair.second.filename) << "\n";
            count++;
        }
    }

    // 4. Numbered marks 0-9
    for (const auto& pair : numberedMarks) {
        if (shouldInclude(pair.first)) {
            oss << " " << pair.first << "   " << std::setw(5) << (pair.second.line + 1)
                << " " << std::setw(3) << pair.second.column
                << " " << getLineSnippet(hwndEdit, pair.second.line, pair.second.filename) << "\n";
            count++;
        }
    }

    // 5. Last change mark .
    if (lastChangeMark.line != -1 && shouldInclude('.')) {
        oss << " .   " << std::setw(5) << (lastChangeMark.line + 1)
            << " " << std::setw(3) << lastChangeMark.column
            << " " << getLineSnippet(hwndEdit, lastChangeMark.line, lastChangeMark.filename) << "\n";
        count++;
    }

    // 6. Visual selection marks < and >
    if (state.lastVisualAnchor != -1 && state.lastVisualCaret != -1) {
        int posStart = (std::min)(state.lastVisualAnchor, state.lastVisualCaret);
        int posEnd = (std::max)(state.lastVisualAnchor, state.lastVisualCaret);
        int lStart = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, posStart, 0);
        int cStart = posStart - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lStart, 0);
        int lEnd = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, posEnd, 0);
        int cEnd = posEnd - (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, lEnd, 0);

        if (shouldInclude('<')) {
            oss << " <   " << std::setw(5) << (lStart + 1)
                << " " << std::setw(3) << cStart
                << " " << getLineSnippet(hwndEdit, lStart, curFile) << "\n";
            count++;
        }
        if (shouldInclude('>')) {
            oss << " >   " << std::setw(5) << (lEnd + 1)
                << " " << std::setw(3) << cEnd
                << " " << getLineSnippet(hwndEdit, lEnd, curFile) << "\n";
            count++;
        }
    }

    // 7. Last insert mark ^
    if (lastInsertMark.line != -1 && shouldInclude('^')) {
        oss << " ^   " << std::setw(5) << (lastInsertMark.line + 1)
            << " " << std::setw(3) << lastInsertMark.column
            << " " << getLineSnippet(hwndEdit, lastInsertMark.line, lastInsertMark.filename) << "\n";
        count++;
    }

    // 8. Change marks [ and ]
    if (changeStartMark.line != -1 && shouldInclude('[')) {
        oss << " [   " << std::setw(5) << (changeStartMark.line + 1)
            << " " << std::setw(3) << changeStartMark.column
            << " " << getLineSnippet(hwndEdit, changeStartMark.line, changeStartMark.filename) << "\n";
        count++;
    }
    if (changeEndMark.line != -1 && shouldInclude(']')) {
        oss << " ]   " << std::setw(5) << (changeEndMark.line + 1)
            << " " << std::setw(3) << changeEndMark.column
            << " " << getLineSnippet(hwndEdit, changeEndMark.line, changeEndMark.filename) << "\n";
        count++;
    }

    if (count == 0) {
        return "No marks set\n";
    }

    return oss.str();
}

int Marks::getMarkLine(char mark) {
    std::string curFile = getCurrentFilename();
    if (mark >= 'a' && mark <= 'z') {
        auto fileIt = localMarksByFile.find(curFile);
        if (fileIt != localMarksByFile.end()) {
            auto it = fileIt->second.find(mark);
            if (it != fileIt->second.end()) return it->second.line;
        }
    } else if (mark >= 'A' && mark <= 'Z') {
        auto it = globalMarks.find(mark);
        if (it != globalMarks.end()) return it->second.line;
    } else if (mark >= '0' && mark <= '9') {
        auto it = numberedMarks.find(mark);
        if (it != numberedMarks.end()) return it->second.line;
    } else if (mark == '.') {
        return lastChangeMark.line;
    } else if (mark == '\'' || mark == '`' || mark == '"') {
        return lastJumpMark.line;
    } else if (mark == '^') {
        return lastInsertMark.line;
    } else if (mark == '[') {
        return changeStartMark.line;
    } else if (mark == ']') {
        return changeEndMark.line;
    } else if (mark == '<' || mark == '>') {
        if (state.lastVisualAnchor != -1 && state.lastVisualCaret != -1) {
            int pos = (mark == '<') ? (std::min)(state.lastVisualAnchor, state.lastVisualCaret) : (std::max)(state.lastVisualAnchor, state.lastVisualCaret);
            HWND h = Utils::getCurrentScintillaHandle();
            if (h) return (int)::SendMessage(h, SCI_LINEFROMPOSITION, pos, 0);
        }
    }
    return -1;
}