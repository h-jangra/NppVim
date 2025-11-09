//Motion.h
#pragma once

#include <windows.h>

class Motion {
public:
    static void charLeft(HWND hwndEdit, int count);
    static void charRight(HWND hwndEdit, int count);
    static void lineUp(HWND hwndEdit, int count);
    static void lineDown(HWND hwndEdit, int count);
    static void wordRight(HWND hwndEdit, int count);
    static void wordLeft(HWND hwndEdit, int count);
    static void wordEnd(HWND hwndEdit, int count);
    static void wordEndPrev(HWND hwndEdit, int count);
    static void lineEnd(HWND hwndEdit, int count);
    static void lineStart(HWND hwndEdit, int count);

    // Character search motions
    static void nextChar(HWND hwndEdit, int count, char target);
    static void prevChar(HWND hwndEdit, int count, char target);
    static void tillChar(HWND hwndEdit, int count, char target);      // New: t motion
    static void tillCharBack(HWND hwndEdit, int count, char target);  // New: T motion

    static void setCursorPosition(HWND hwndEdit, int pos, bool isTillMotion = false);

    static void paragraphUp(HWND hwndEdit, int count);
    static void paragraphDown(HWND hwndEdit, int count);
    static void gotoLine(HWND hwndEdit, int lineNum);
    static void documentStart(HWND hwndEdit);
    static void documentEnd(HWND hwndEdit);
    static void pageUp(HWND hwndEdit);
    static void pageDown(HWND hwndEdit);
    static void matchPair(HWND hwndEdit);
    static void toggleCase(HWND hwndEdit, int count);
};

extern Motion motion;
