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
extern std::unique_ptr<Keymap> g_insertKeymap;

using KeyHandler = std::function<void(HWND, int)>;

class KeymapNode {
public:
    KeyHandler handler = nullptr;
    bool isLeaf = false;
    char motionChar = 0;  // For automatic motion tracking

    KeyHandler userHandler = nullptr;
    bool isUserLeaf = false;

    std::unordered_map<char, std::shared_ptr<KeymapNode>> children;
};

enum class MappingMode {
    Normal,
    Insert,
    Visual,
    Command,
    All
};

struct Mapping {
    std::string from;
    std::string to;
    bool recursive = true;
    MappingMode mode = MappingMode::All;
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
    
    // Support for dynamic mappings
    void addMapping(const std::string& from, const std::string& to, bool recursive, MappingMode mode = MappingMode::All);
    void removeMapping(const std::string& from);
    void clearDynamicMappings();
    std::vector<Mapping> getUserMappings() const;
    static std::vector<Mapping> getAllUserMappings(MappingMode mode = MappingMode::All);
    static void clearAllDynamicMappings();

    void setIgnoreUserMappings(bool ignore) { ignoreUserMappings = ignore; }
    bool getIgnoreUserMappings() const { return ignoreUserMappings; }

    std::string getPendingSequence() const { return pendingKeys; }
    bool hasPending() const { return !pendingKeys.empty(); }

    static void feedKey(HWND hwnd, char c);

private:
    VimState& state;
    std::shared_ptr<KeymapNode> root;
    std::shared_ptr<KeymapNode> currentNode;
    std::string pendingKeys;

    bool allowCount = true;
    bool ignoreUserMappings = false;
    std::vector<KeyBinding> bindings;
    std::unordered_map<std::string, Mapping> userMappings;
    
    void insertKeySequence(const std::string& keys, KeyHandler handler, char motionChar = 0);
    void insertUserKeySequence(const std::string& keys, KeyHandler handler);
    bool processKey(HWND hwnd, char key, int count);
};
