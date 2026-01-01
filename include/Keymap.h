#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

struct VimState;
class Keymap;

extern std::unique_ptr<Keymap> g_normalKeymap;
extern std::unique_ptr<Keymap> g_visualKeymap;
extern std::unique_ptr<Keymap> g_commandKeymap;

using KeyHandler = std::function<void(HWND, int)>;

class KeymapNode {
public:
    KeyHandler handler;
    std::unordered_map<char, std::shared_ptr<KeymapNode>> children;
    bool isLeaf = false;
    char motionChar = 0;  // For automatic motion tracking
};

struct KeyBinding {
    std::string keys;
    std::string desc;
};

class Keymap {
public:
    Keymap(VimState& state);
    
    Keymap& set(const std::string& keys, KeyHandler handler);
    Keymap& set(const std::string& keys, const std::string& desc, KeyHandler handler);

    Keymap& motion(const std::string& keys, char motionChar, KeyHandler handler);
    Keymap& motion(const std::string& keys, char motionChar, const std::string& desc, KeyHandler handler);

    void setAllowCount(bool v);
    const std::vector<KeyBinding>& getBindings() const;

    bool handleKey(HWND hwnd, char key);
    void reset();
    
    std::string getPendingSequence() const { return pendingKeys; }
    bool hasPending() const { return !pendingKeys.empty(); }

private:
    VimState& state;
    std::shared_ptr<KeymapNode> root;
    std::shared_ptr<KeymapNode> currentNode;
    std::string pendingKeys;

    bool allowCount = true;
    std::vector<KeyBinding> bindings;
    
    void insertKeySequence(const std::string& keys, KeyHandler handler, char motionChar = 0);
    bool processKey(HWND hwnd, char key, int count);
};
