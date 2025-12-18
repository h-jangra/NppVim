#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>

struct VimState;

using KeyHandler = std::function<void(HWND, int)>;

class KeymapNode {
public:
    KeyHandler handler;
    std::unordered_map<char, std::shared_ptr<KeymapNode>> children;
    bool isLeaf = false;
    char motionChar = 0;  // For automatic motion tracking
};

class Keymap {
public:
    Keymap(VimState& state);
    
    Keymap& set(const std::string& keys, KeyHandler handler);
    Keymap& motion(const std::string& keys, char motionChar, KeyHandler handler);
    
    bool handleKey(HWND hwnd, char key);
    void reset();
    
    std::string getPendingSequence() const { return pendingKeys; }
    bool hasPending() const { return !pendingKeys.empty(); }

private:
    VimState& state;
    std::shared_ptr<KeymapNode> root;
    std::shared_ptr<KeymapNode> currentNode;
    std::string pendingKeys;
    
    void insertKeySequence(const std::string& keys, KeyHandler handler, char motionChar = 0);
    bool processKey(HWND hwnd, char key, int count);
};

extern std::unique_ptr<Keymap> g_normalKeymap;