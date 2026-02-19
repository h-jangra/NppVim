// NppVim.h
#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <map>

enum VimMode {
    NORMAL,
    INSERT,
    VISUAL,
    COMMAND
};

enum OperationType {
    OP_NONE,
    OP_DELETE_LINE,
    OP_YANK_LINE,
    OP_PASTE_LINE,
    OP_MOTION,
    OP_REPLACE,
    OP_PASTE
};

enum TextObjectType {
    TEXT_OBJECT_WORD,
    TEXT_OBJECT_BIG_WORD,
    TEXT_OBJECT_SENTENCE,
    TEXT_OBJECT_PARAGRAPH,
    TEXT_OBJECT_PAREN,
    TEXT_OBJECT_BRACKET,
    TEXT_OBJECT_BRACE,
    TEXT_OBJECT_ANGLE
};

struct VimConfig {
    std::string escapeKey = "esc";
    std::string customEscape = "";
    int escapeTimeout = 300;
    bool overrideCtrlD = false;
    bool overrideCtrlU = false;
    bool overrideCtrlR = false;
    bool overrideCtrlF = false;
    bool overrideCtrlB = false;
    bool overrideCtrlO = false;
    bool overrideCtrlI = false;
    bool xStoreClipboard = true;
    bool dStoreClipboard = true;
    bool cStoreClipboard = true;
};

extern VimConfig g_config;

struct LastOperation {
    OperationType type = OP_NONE;
    int count = 0;
    char motion = 0;
    char searchChar = 0;

    char textModifier = 0;
    char textObject = 0;
};

struct JumpPosition {
    long position = -1;
    int lineNumber = -1;
};

extern int g_macroDepth;
const int MAX_MACRO_DEPTH = 10;

struct VimState {
    VimMode mode = NORMAL;
    bool vimEnabled = true;
    bool commandMode = false;
    bool isLineVisual = false;
    bool isBlockVisual = false;
    bool lastYankLinewise = false;

    int lastVisualStart = -1;
    int lastVisualEnd = -1;
    int lastVisualAnchor = -1;
    int lastVisualCaret = -1;
    int lastInsertPos = -1;

    bool restoringVisual = false;
    bool lastVisualWasLine = false;
    bool lastVisualWasBlock = false;

    int repeatCount = 0;
    char opPending = 0;
    char textObjectPending = 0;
    bool replacePending = false;
    bool visualReplacePending = false;

    std::map<char, std::string> registers;
    char defaultRegister = '"';
    bool deleteToBlackhole = false;
    bool awaitingRegister = false;
    bool awaitingRegisterOperation = false;

    bool awaitingBracketAbove = false;
    bool awaitingBracketBelow = false;

    int visualAnchor = -1;
    int visualAnchorLine = -1;

    char lastSearchChar = 0;
    bool lastSearchForward = true;
    bool lastSearchTill = false;

    LastOperation lastOp;

    std::vector<JumpPosition> jumpList;
    int jumpIndex = -1;

    std::string commandBuffer;
    std::string lastSearchTerm;
    bool useRegex = false;
    int lastSearchMatchCount = -1;
    int visualSearchAnchor = -1;

    bool awaitingMarkSet = false;
    bool awaitingMarkJump = false;
    bool isBacktickJump = false;
    int pendingJumpCount = 0;

    bool recordingMacro = false;
    char macroRegister = '\0';
    std::vector<char> macroBuffer;
    std::vector<std::vector<char>> insertMacroBuffers; 
    bool recordingInsertMacro = false;
    bool awaitingMacroRegister = false;

    void reset() {
        repeatCount = 0;
        opPending = 0;
        textObjectPending = 0;
        replacePending = false;
        visualReplacePending = false;
        awaitingMarkSet = false;
        awaitingMarkJump = false;
        isBacktickJump = false;
        commandBuffer.clear();
        lastSearchTerm.clear();
        useRegex = false;
        lastSearchMatchCount = -1;
    }

    void resetPending() {
        repeatCount = 0;
        opPending = 0;
        textObjectPending = 0;
        replacePending = false;
        visualReplacePending = false;
    }

    void recordLastOp(OperationType type, int count, char motion = 0, char searchChar = 0) {
        lastOp.type = type;
        lastOp.count = count;
        lastOp.motion = motion;
        lastOp.searchChar = searchChar;
    }

    void recordJump(long position, int lineNumber) {
        if (!jumpList.empty()) {
            auto& last = jumpList.back();
            if (abs(last.position - position) < 5 && last.lineNumber == lineNumber) {
                return;
            }
        }

        JumpPosition jump;
        jump.position = position;
        jump.lineNumber = lineNumber;

        if (jumpList.size() >= 100) {
            jumpList.erase(jumpList.begin());
        }

        jumpList.push_back(jump);
        jumpIndex = (int)jumpList.size() - 1;
    }

    int getJumpStackSize() const {
        return (int)jumpList.size();
    }

    JumpPosition getLastJump() {
        if (jumpList.size() < 2) {
            return JumpPosition();
        }

        return jumpList[jumpList.size() - 2];
    }
};

void showConfigDialog();
void about();
void loadConfig();

extern VimState state;
