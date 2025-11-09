// NppVim.h
#pragma once

#include <windows.h>
#include <vector>
#include <string>

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
    OP_REPLACE
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

struct LastOperation {
    OperationType type = OP_NONE;
    int count = 0;
    char motion = 0;
    char searchChar = 0;
};

struct JumpPosition {
    long position = -1;
    int lineNumber = -1;
};

struct VimState {
    //  Core Mode State 
    VimMode mode = NORMAL;
    bool vimEnabled = true;
    bool commandMode = false;
    bool isLineVisual = false;

    //  Operator / Repeat Info 
    int repeatCount = 0;
    char opPending = 0;
    char textObjectPending = 0;
    bool replacePending = false;

    //  Visual Mode State 
    int visualAnchor = -1;
    int visualAnchorLine = -1;

    //  Character Search State 
    char lastSearchChar = 0;
    bool lastSearchForward = true;
    bool lastSearchTill = false;

    //  Operation History 
    LastOperation lastOp;

    //  Jump List 
    std::vector<JumpPosition> jumpList;
    int jumpIndex = -1;

    //  Command Mode State (added for CommandMode.cpp) 
    std::string commandBuffer;      // Stores ":" or "/" command text
    std::string lastSearchTerm;     // Last searched string
    bool useRegex = false;          // Whether last search used regex
    int lastSearchMatchCount = -1;  // Number of matches for last search

    //  Mark Flags 
    bool awaitingMarkSet = false;
    bool awaitingMarkJump = false;
    bool isBacktickJump = false;

    //  Reset & Record Helpers 
    void reset() {
        repeatCount = 0;
        opPending = 0;
        textObjectPending = 0;
        replacePending = false;
        awaitingMarkSet = false;
        awaitingMarkJump = false;
        isBacktickJump = false;
        commandBuffer.clear();
        lastSearchTerm.clear();
        useRegex = false;
        lastSearchMatchCount = -1;
    }

    void recordLastOp(OperationType type, int count, char motion = 0, char searchChar = 0) {
        lastOp.type = type;
        lastOp.count = count;
        lastOp.motion = motion;
        lastOp.searchChar = searchChar;
    }

    void recordJump(long position, int lineNumber) {
        if (jumpIndex < (int)jumpList.size() - 1)
            jumpList.erase(jumpList.begin() + jumpIndex + 1, jumpList.end());

        JumpPosition jump;
        jump.position = position;
        jump.lineNumber = lineNumber;

        if (jumpList.size() >= 100)
            jumpList.erase(jumpList.begin());

        jumpList.push_back(jump);
        jumpIndex = (int)jumpList.size() - 1;
    }

    JumpPosition getLastJump() {
        if (jumpIndex > 0) {
            jumpIndex--;
            return jumpList[jumpIndex];
        }
        return JumpPosition();
    }
};

extern VimState state;
