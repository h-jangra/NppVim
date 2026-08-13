#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <vector>

#define VIM_MARKER_BASE 10
#define VIM_MARKER_LOCAL_START 20
#define VIM_MARKER_GLOBAL_START 21

struct MarkInfo {
    int line;
    int column;
    std::string filename;
    bool isGlobal;
    MarkInfo() : line(-1), column(-1), filename(""), isGlobal(false) {}
    MarkInfo(int l, int c, const std::string& f, bool g) : line(l), column(c), filename(f), isGlobal(g) {}
};

class Marks {
public:
    static void setMark(HWND hwndEdit, char mark);
    static void setMarkAtLine(HWND hwndEdit, char mark, int line, int column = 0);
    static bool jumpToMark(HWND hwndEdit, char mark, bool isBacktick = false);
    static void deleteMark(HWND hwndEdit, char mark);
    static int deleteMarks(HWND hwndEdit, const std::string& args);
    static void clearAllMarks(HWND hwndEdit);
    static void clearLocalMarks(HWND hwndEdit);
    static std::string listMarks(HWND hwndEdit, const std::string& filter = "");
    static void initializeMarkers(HWND hwndEdit);
    static int getMarkerNumber(char mark);
    static bool isValidMark(char mark);
    static bool isValidSetMark(char mark);
    static int getMarkLine(char mark);
    static void recordLastChange(HWND hwndEdit);
    static void recordLastJump(HWND hwndEdit);
    static void recordLastInsert(HWND hwndEdit);
    static void recordChangeRange(HWND hwndEdit, int startLine, int startCol, int endLine, int endCol);

    static std::string getCurrentFilename();

private:
    static std::map<std::string, std::map<char, MarkInfo>> localMarksByFile;
    static std::map<char, MarkInfo> globalMarks;
    static std::map<char, MarkInfo> numberedMarks;
    static MarkInfo lastJumpMark;
    static MarkInfo lastChangeMark;
    static MarkInfo lastInsertMark;
    static MarkInfo changeStartMark;
    static MarkInfo changeEndMark;

    static void updateMarkerDisplay(HWND hwndEdit, char mark);
    static void removeMarkerDisplay(HWND hwndEdit, char mark);
};