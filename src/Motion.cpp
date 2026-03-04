//Motion.cpp
#include "../include/Motion.h"
#include "../plugin/Scintilla.h"
#include "../include/NppVim.h"
#include "../include/Utils.h"

Motion motion;

static inline void doMotion(HWND h,int normalCmd,int extendCmd,int count){
    Utils::sci(h, (state.mode==VISUAL) ? extendCmd : normalCmd, count);
}

void Motion::charLeft(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_CHARLEFT, SCI_CHARLEFTEXTEND, count);
}

void Motion::charRight(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_CHARRIGHT, SCI_CHARRIGHTEXTEND, count);
}

void Motion::lineUp(HWND hwndEdit, int count) {
    if(state.mode!=VISUAL){
        Utils::sci(hwndEdit, SCI_LINEUP, count);
        return;
    }

    int anchor=state.visualAnchor;

    for(int i=0;i<count;i++){
        int pos=Utils::sci(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int line=Utils::sci(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
        int newLine=(line>0)?line-1:0;

        int newPos=Utils::sci(hwndEdit, SCI_FINDCOLUMN, newLine, state.visualPreferredColumn);
        Utils::sci(hwndEdit, SCI_SETSEL, anchor, newPos);
    }
}

void Motion::lineDown(HWND hwndEdit, int count) {
    if(state.mode!=VISUAL){
        Utils::sci(hwndEdit, SCI_LINEDOWN, count);
        return;
    }

    int anchor=state.visualAnchor;
    int maxLine=Utils::sci(hwndEdit, SCI_GETLINECOUNT, 0, 0) - 1;

    for(int i=0;i<count;i++){
        int pos=Utils::sci(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        int line=Utils::sci(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
        int newLine=(line<maxLine)?line+1:maxLine;

        int newPos=Utils::sci(hwndEdit, SCI_FINDCOLUMN, newLine, state.visualPreferredColumn);
        Utils::sci(hwndEdit, SCI_SETSEL, anchor, newPos);
    }
}

void Motion::wordRight(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_WORDRIGHT, SCI_WORDRIGHTEXTEND, count);
}

void Motion::wordRightBig(HWND h,int count){
    doMotion(h,SCI_WORDPARTRIGHT,SCI_WORDPARTRIGHTEXTEND,count);
}

// void Motion::wordRightBig(HWND hwndEdit, int count) {
//     int docLen = (int)::SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
//     int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
//     for (int i = 0; i < count; i++) {
//         while (pos < docLen) {
//             char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
//             if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
//                 break;
//             }
//             pos++;
//         }
//         while (pos < docLen) {
//             char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
//             if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')) {
//                 break;
//             }
//             pos++;
//         }
//     }
//     if (state.mode == VISUAL) {
//         int anchor = state.visualAnchor;
//         ::SendMessage(hwndEdit, SCI_SETSEL, anchor, pos);
//     }
//     else {
//         ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
//         ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
//     }
// }

void Motion::wordLeft(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_WORDLEFT, SCI_WORDLEFTEXTEND, count);
}

void Motion::wordLeftBig(HWND h,int count){
    doMotion(h,SCI_WORDPARTLEFT,SCI_WORDPARTLEFTEXTEND,count);
}

// void Motion::wordLeftBig(HWND hwndEdit, int count) {
//     int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
//     for (int i = 0; i < count; i++) {
//         if (pos <= 0) break;
//         pos--;
//         while (pos > 0) {
//             char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
//             if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')) {
//                 break;
//             }
//             pos--;
//         }
//         while (pos > 0) {
//             char prevCh = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos - 1, 0);
//             if (prevCh == ' ' || prevCh == '\t' || prevCh == '\r' || prevCh == '\n') {
//                 break;
//             }
//             pos--;
//         }
//     }
//     if (state.mode == VISUAL) {
//         int anchor = state.visualAnchor;
//         ::SendMessage(hwndEdit, SCI_SETSEL, anchor, pos);
//     }
//     else {
//         ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, pos, 0);
//         ::SendMessage(hwndEdit, SCI_SETSEL, pos, pos);
//     }
// }

void Motion::wordEnd(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_WORDRIGHTEND, SCI_WORDRIGHTENDEXTEND, count);
}

void Motion::wordEndBig(HWND hwndEdit, int count) {
    int docLen = (int)::SendMessage(hwndEdit, SCI_GETLENGTH, 0, 0);
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    for (int i = 0; i < count; i++) {
        if (pos < docLen) pos++;

        while (pos < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
            if (!(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')) {
                break;
            }
            pos++;
        }
        while (pos < docLen) {
            char ch = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, pos, 0);
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
                pos--;
                break;
            }
            pos++;
        }
        if (pos >= docLen) {
            pos = docLen - 1;
        }
    }
    if (state.mode == VISUAL) {
        int anchor = state.visualAnchor;
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, pos + 1);
    }
    else {
        Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,pos);
    }
}

void Motion::wordEndPrev(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_WORDLEFTEND, SCI_WORDLEFTENDEXTEND, count);
}

void Motion::lineEnd(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_LINEEND, SCI_LINEENDEXTEND, count);
}

void Motion::lineStart(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_VCHOME, SCI_VCHOMEEXTEND, count);
}

void Motion::nextChar(HWND hwndEdit, int count, char target) {

    int pos=Utils::sci(hwndEdit,SCI_GETCURRENTPOS);
    int line=Utils::sci(hwndEdit,SCI_LINEFROMPOSITION,pos);
    int end=Utils::sci(hwndEdit,SCI_GETLINEENDPOSITION,line);

    int found=0;

    for(int i=pos+1;i<=end;i++){
        if((char)Utils::sci(hwndEdit,SCI_GETCHARAT,i)==target){
            if(++found==count){
                Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,i);
                return;
            }
        }
    }

    // int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    // int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    // int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    // int foundPos = -1;
    // int searchPos = pos + 1;
    // int found = 0;
    // while (searchPos <= lineEnd) {
    //     char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
    //     if (c == target) {
    //         found++;
    //         if (found == count) { foundPos = searchPos; break; }
    //     }
    //     searchPos++;
    // }
    // if (foundPos == -1) return;
    
    // ::SendMessage(hwndEdit, SCI_SETCURRENTPOS, foundPos, 0);
    // ::SendMessage(hwndEdit, SCI_SETSEL, foundPos, foundPos);
}

void Motion::prevChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);

    int foundPos = -1;
    int searchPos = pos - 1;
    int found = 0;
    while (searchPos >= lineStart) {
        char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
        if (c == target) {
            found++;
            if (found == count) { foundPos = searchPos; break; }
        }
        searchPos--;
    }
    if (foundPos == -1) return;
    
    Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,foundPos);
}

void Motion::tillChar(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineEnd = (int)::SendMessage(hwndEdit, SCI_GETLINEENDPOSITION, line, 0);

    int searchStart = pos;
    for (int i = 0; i < count; ++i) {
        int searchPos = searchStart + 1;
        bool found = false;
        while (searchPos <= lineEnd) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (c == target) { searchStart = searchPos; found = true; break; }
            searchPos++;
        }
        if (!found) return;
    }
    int finalPos = searchStart - 1;
    if (finalPos < Utils::lineStart(hwndEdit, line)) finalPos = Utils::lineStart(hwndEdit, line);
    
    Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,finalPos);
}

void Motion::tillCharBack(HWND hwndEdit, int count, char target) {
    int pos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
    int line = (int)::SendMessage(hwndEdit, SCI_LINEFROMPOSITION, pos, 0);
    int lineStart = (int)::SendMessage(hwndEdit, SCI_POSITIONFROMLINE, line, 0);

    int searchStart = pos;
    for (int i = 0; i < count; ++i) {
        int searchPos = searchStart - 1;
        bool found = false;
        while (searchPos >= lineStart) {
            char c = (char)::SendMessage(hwndEdit, SCI_GETCHARAT, searchPos, 0);
            if (c == target) { searchStart = searchPos; found = true; break; }
            searchPos--;
        }
        if (!found) return;
    }
    int finalPos = searchStart + 1;
    int lineEnd = Utils::lineEnd(hwndEdit, line);
    if (finalPos > lineEnd) finalPos = lineEnd;
    Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,finalPos);
}

void Motion::paragraphUp(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_PARAUP, SCI_PARAUPEXTEND, count);
}

void Motion::paragraphDown(HWND hwndEdit, int count) {
    doMotion(hwndEdit, SCI_PARADOWN, SCI_PARADOWNEXTEND, count);
}

void Motion::gotoLine(HWND hwndEdit, int lineNum) {
    if (state.mode == VISUAL) {
        int anchor = state.visualAnchor;
        ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
        int newPos = (int)::SendMessage(hwndEdit, SCI_GETCURRENTPOS, 0, 0);
        ::SendMessage(hwndEdit, SCI_SETSEL, anchor, newPos);
    }
    else {
        ::SendMessage(hwndEdit, SCI_GOTOLINE, lineNum - 1, 0);
    }
}

void Motion::documentStart(HWND hwndEdit) {
    doMotion(hwndEdit, SCI_DOCUMENTSTART, SCI_DOCUMENTSTARTEXTEND, 1);
}

void Motion::documentEnd(HWND hwndEdit) {
    doMotion(hwndEdit, SCI_DOCUMENTEND, SCI_DOCUMENTENDEXTEND, 1);
}

void Motion::pageUp(HWND hwndEdit) {
    doMotion(hwndEdit, SCI_PAGEUP, SCI_PAGEUPEXTEND, 1);
}

void Motion::pageDown(HWND hwndEdit) {
    doMotion(hwndEdit, SCI_PAGEDOWN, SCI_PAGEDOWNEXTEND, 1);
}

void Motion::matchPair(HWND hwndEdit) {
    int pos=Utils::sci(hwndEdit,SCI_GETCURRENTPOS);
    int match=Utils::sci(hwndEdit,SCI_BRACEMATCH,pos);

    if(match==-1)
        match=Utils::sci(hwndEdit,SCI_BRACEMATCH,pos-1);

    if(match==-1) return;

    if(state.mode==VISUAL){
        int anchor=state.visualAnchor;
        Utils::sci(hwndEdit,SCI_SETSEL,(match<anchor)?match:anchor,(match<anchor)?anchor:match+1);
    }else{
        Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,match);
    }
}

void Motion::toggleCase(HWND hwndEdit, int count) {
    int start,end;

    if(state.mode==VISUAL){
        start=Utils::sci(hwndEdit,SCI_GETSELECTIONSTART);
        end=Utils::sci(hwndEdit,SCI_GETSELECTIONEND);
    }else{
        start=Utils::sci(hwndEdit,SCI_GETCURRENTPOS);
        end=start+count;
    }

    for(int i=start;i<end;i++){
        char c=(char)Utils::sci(hwndEdit,SCI_GETCHARAT,i);

        if(std::islower(c)) c=std::toupper(c);
        else if(std::isupper(c)) c=std::tolower(c);
        else continue;

        Utils::sci(hwndEdit,SCI_SETTARGETRANGE,i,i+1);
        Utils::sci(hwndEdit,SCI_REPLACETARGET,1,(LPARAM)&c);
    }

    if(state.mode==NORMAL)
        Utils::sci(hwndEdit,SCI_SETEMPTYSELECTION,end);
}
