#pragma once
#include <windows.h>
#include <map>
#include <string>

#define VIM_MARKER_BASE 10
#define VIM_MARKER_LOCAL_START 10
#define VIM_MARKER_GLOBAL_START 20

struct MarkInfo {
    int line;
    int column;
    std::string filename;
    bool isGlobal;
    MarkInfo() : line(-1), column(-1), isGlobal(false) {}
    MarkInfo(int l, int c, const std::string& f, bool g) : line(l), column(c), filename(f), isGlobal(g) {}
};

class Marks {
public:
    static void setMark(HWND hwndEdit, char mark);
    static bool jumpToMark(HWND hwndEdit, char mark, bool isBacktick = false);
    static void deleteMark(HWND hwndEdit, char mark);
    static void clearAllMarks(HWND hwndEdit);
    static std::string listMarks(HWND hwndEdit);
    static void initializeMarkers(HWND hwndEdit);
    static int getMarkerNumber(char mark);
    static bool isValidMark(char mark);

private:
    static std::map<char, MarkInfo> localMarks;
    static std::map<char, MarkInfo> globalMarks;
    static MarkInfo lastJumpMark;
    static MarkInfo lastChangeMark;

    static std::string getCurrentFilename();
    static void updateMarkerDisplay(HWND hwndEdit, char mark);
    static void removeMarkerDisplay(HWND hwndEdit, char mark);
};