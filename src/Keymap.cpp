#include "../include/Keymap.h"
#include "../include/NppVim.h"
#include "../include/Utils.h"
#include "../include/NormalMode.h"
#include "../include/VisualMode.h"
#include "../include/CommandMode.h"
#include "../include/Motion.h"
#include "../plugin/Scintilla.h"
#include <cctype>

std::unique_ptr<Keymap> g_normalKeymap;
std::unique_ptr<Keymap> g_visualKeymap;
std::unique_ptr<Keymap> g_commandKeymap;
std::unique_ptr<Keymap> g_insertKeymap;

extern NormalMode* g_normalMode;
extern VisualMode* g_visualMode;
extern CommandMode* g_commandMode;

Keymap::Keymap(VimState& state) 
    : state(state), root(std::make_shared<KeymapNode>()), currentNode(root) {}

Keymap& Keymap::set(const std::string& keys, KeyHandler handler) {
    insertKeySequence(keys, handler);
    return *this;
}

Keymap& Keymap::set(const std::string& keys, const std::string& desc, KeyHandler handler) {
    insertKeySequence(keys, handler);
    bindings.push_back({ keys, desc });
    return *this;
}

void Keymap::setAllowCount(bool v) {
    allowCount = v;
}

const std::vector<KeyBinding>& Keymap::getBindings() const {
    return bindings;
}

Keymap& Keymap::motion(const std::string& keys, char motionChar, KeyHandler handler) {
    insertKeySequence(keys, handler, motionChar);
    return *this;
}

Keymap& Keymap::motion(const std::string& keys, char motionChar, const std::string& desc, KeyHandler handler) {
    insertKeySequence(keys, handler, motionChar);
    bindings.push_back({ keys, desc });
    return *this;
}

void Keymap::insertKeySequence(const std::string& keys, KeyHandler handler, char motionChar) {
    auto node = root;
    for (char key : keys) {
        if (node->children.find(key) == node->children.end()) {
            node->children[key] = std::make_shared<KeymapNode>();
        }
        node = node->children[key];
    }
    node->handler = handler;
    node->motionChar = motionChar;
    node->isLeaf = true;
}

void Keymap::insertUserKeySequence(const std::string& keys, KeyHandler handler) {
    auto node = root;
    for (char key : keys) {
        if (node->children.find(key) == node->children.end()) {
            node->children[key] = std::make_shared<KeymapNode>();
        }
        node = node->children[key];
    }
    node->userHandler = handler;
    node->isUserLeaf = true;
}

bool Keymap::handleKey(HWND hwnd, char key) {
    if (allowCount && std::isdigit(static_cast<unsigned char>(key))) {
        int digit = key - '0';
        if (key == '0' && state.repeatCount == 0 && pendingKeys.empty()) {
            auto it = currentNode->children.find(key);
            if (it != currentNode->children.end() && (it->second->isLeaf || (!ignoreUserMappings && it->second->isUserLeaf))) {
                return processKey(hwnd, key, 1);
            }
        }
        state.repeatCount = state.repeatCount * 10 + digit;
        return true;
    }
    
    int count = (state.repeatCount > 0) ? state.repeatCount : 1;
    return processKey(hwnd, key, count);
}

bool Keymap::processKey(HWND hwnd, char key, int count) {
    auto it = currentNode->children.find(key);

    if (it == currentNode->children.end()) {
        if (currentNode != root) {
            std::string oldPending = pendingKeys;
            reset();
            if (this == g_insertKeymap.get()) {
                for (char pc : oldPending) {
                    char str[2] = { pc, '\0' };
                    ::SendMessage(hwnd, SCI_ADDTEXT, 1, (LPARAM)str);
                }
            }
            return processKey(hwnd, key, count); // retry from root
        }

        return false;
    } else {
        currentNode = it->second;
        pendingKeys += key;
    }

    // Check user mapping first (if not ignoring user mappings)
    if (!ignoreUserMappings && currentNode->isUserLeaf && currentNode->userHandler) {
        auto handler = currentNode->userHandler;
        reset();
        handler(hwnd, count);
        return true;
    }

    // Check builtin leaf
    if (currentNode->isLeaf && currentNode->handler) {
        auto handler = currentNode->handler;
        char motion = currentNode->motionChar;
        reset();
        handler(hwnd, count);

        if (motion) {
            state.recordLastOp(OP_MOTION, count, motion);
        }
        return true;
    }

    if (this != g_insertKeymap.get()) {
        std::wstring status = L"-- ";
        for (char c : pendingKeys) status += (wchar_t)c;
        status += L" --";
        Utils::setStatus(status.c_str());
    }

    return true;
}

void Keymap::reset() {
    currentNode = root;
    pendingKeys.clear();
    state.repeatCount = 0;
}

void Keymap::feedKey(HWND hwnd, char c) {
    if (::state.commandMode) {
        if (c == '\r' || c == '\n') {
            if (g_commandMode) g_commandMode->handleEnter(hwnd);
            return;
        }
        if (c == 27) {
            Utils::clearSearchHighlights(hwnd);
            ::state.lastSearchMatchCount = -1;
            if (g_commandMode) g_commandMode->exit();
            return;
        }
        if (c == 8) {
            if (g_commandMode) g_commandMode->handleBackspace(hwnd);
            return;
        }
        if (g_commandMode) g_commandMode->handleKey(hwnd, (wchar_t)(unsigned char)c);
        return;
    }

    if (::state.mode == INSERT) {
        if (c == 27) { // ESC
            ::SendMessage(hwnd, SCI_SETOVERTYPE, false, 0);
            if (::state.recordingInsertMacro && !::state.insertMacroBuffers.empty()) {
                ::state.insertMacroBuffers.back().push_back('\x1B');
                ::state.recordingInsertMacro = false;
            }
            if (g_normalMode) g_normalMode->enter();
            return;
        }

        if (g_insertKeymap && (g_insertKeymap->hasPending() || g_insertKeymap->handleKey(hwnd, c))) {
            return;
        }

        if (::state.recordingInsertMacro && !::state.insertMacroBuffers.empty()) {
            ::state.insertMacroBuffers.back().push_back(c);
        }

        char str[2] = { c, '\0' };
        ::SendMessage(hwnd, SCI_ADDTEXT, 1, (LPARAM)str);
        return;
    }

    if (c == 27) {
        if (g_normalMode) g_normalMode->enter();
        return;
    }

    // Handle Ctrl key characters in Normal mode
    if (::state.mode == NORMAL && (unsigned char)c >= 1 && (unsigned char)c <= 26) {
        if (c == 4 && g_config.overrideCtrlD) { Motion::pageDown(hwnd); ::state.repeatCount = 0; return; }
        if (c == 21 && g_config.overrideCtrlU) { Motion::pageUp(hwnd); ::state.repeatCount = 0; return; }
        if (c == 18 && g_config.overrideCtrlR) { ::SendMessage(hwnd, SCI_REDO, 0, 0); return; }
        if (c == 6 && g_config.overrideCtrlF) { Motion::pageDown(hwnd); ::state.repeatCount = 0; return; }
        if (c == 2 && g_config.overrideCtrlB) { Motion::pageUp(hwnd); ::state.repeatCount = 0; return; }
        if (c == 15 && g_config.overrideCtrlO) { if (g_normalMode) g_normalMode->jumpBackward(hwnd); return; }
        if (c == 9 && g_config.overrideCtrlI) { if (g_normalMode) g_normalMode->jumpForward(hwnd); return; }
        if (c == 1 && g_config.overrideCtrlA) { if (g_normalMode) g_normalMode->incrementNumber(hwnd, 1); return; }
        if (c == 24 && g_config.overrideCtrlX) { if (g_normalMode) g_normalMode->decrementNumber(hwnd, 1); return; }
    }

    if (::state.mode == NORMAL) {
        if (g_normalMode) g_normalMode->handleKey(hwnd, c);
        return;
    } else if (::state.mode == VISUAL) {
        if (g_visualMode) g_visualMode->handleKey(hwnd, c);
        return;
    }
}

void Keymap::addMapping(const std::string& from, const std::string& to, bool recursive, MappingMode mode) {
    userMappings[from] = { from, to, recursive, mode };

    auto handler = [to, recursive](HWND hwnd, int count) {
        static int depth = 0;
        if (depth > 20) {
            Utils::setStatus(TEXT("Mapping recursion limit reached"));
            return;
        }
        depth++;
        
        bool prevNormalIgnore = g_normalKeymap ? g_normalKeymap->getIgnoreUserMappings() : false;
        bool prevVisualIgnore = g_visualKeymap ? g_visualKeymap->getIgnoreUserMappings() : false;
        bool prevInsertIgnore = g_insertKeymap ? g_insertKeymap->getIgnoreUserMappings() : false;
        bool prevCommandIgnore = g_commandKeymap ? g_commandKeymap->getIgnoreUserMappings() : false;

        if (!recursive) {
            if (g_normalKeymap) g_normalKeymap->setIgnoreUserMappings(true);
            if (g_visualKeymap) g_visualKeymap->setIgnoreUserMappings(true);
            if (g_insertKeymap) g_insertKeymap->setIgnoreUserMappings(true);
            if (g_commandKeymap) g_commandKeymap->setIgnoreUserMappings(true);
        }

        for (int i = 0; i < count; ++i) {
            for (char c : to) {
                Keymap::feedKey(hwnd, c);
            }
        }

        if (!recursive) {
            if (g_normalKeymap) g_normalKeymap->setIgnoreUserMappings(prevNormalIgnore);
            if (g_visualKeymap) g_visualKeymap->setIgnoreUserMappings(prevVisualIgnore);
            if (g_insertKeymap) g_insertKeymap->setIgnoreUserMappings(prevInsertIgnore);
            if (g_commandKeymap) g_commandKeymap->setIgnoreUserMappings(prevCommandIgnore);
        }

        depth--;
    };
    insertUserKeySequence(from, handler);
}

void Keymap::removeMapping(const std::string& from) {
    userMappings.erase(from);
    auto node = root;
    for (char key : from) {
        if (node->children.find(key) == node->children.end()) return;
        node = node->children[key];
    }
    node->userHandler = nullptr;
    node->isUserLeaf = false;
}

static void clearUserNodes(std::shared_ptr<KeymapNode> node) {
    if (!node) return;
    node->userHandler = nullptr;
    node->isUserLeaf = false;
    for (auto& pair : node->children) {
        clearUserNodes(pair.second);
    }
}

void Keymap::clearDynamicMappings() {
    userMappings.clear();
    clearUserNodes(root);
}

std::vector<Mapping> Keymap::getUserMappings() const {
    std::vector<Mapping> result;
    for (const auto& [_, mapping] : userMappings) {
        result.push_back(mapping);
    }
    return result;
}

std::vector<Mapping> Keymap::getAllUserMappings(MappingMode mode) {
    std::vector<Mapping> result;
    auto appendMappings = [&](const Keymap* km) {
        if (!km) return;
        for (const auto& m : km->getUserMappings()) {
            result.push_back(m);
        }
    };

    if (mode == MappingMode::Normal) {
        appendMappings(g_normalKeymap.get());
    } else if (mode == MappingMode::Visual) {
        appendMappings(g_visualKeymap.get());
    } else if (mode == MappingMode::Insert) {
        appendMappings(g_insertKeymap.get());
    } else if (mode == MappingMode::Command) {
        appendMappings(g_commandKeymap.get());
    } else {
        appendMappings(g_normalKeymap.get());
        appendMappings(g_visualKeymap.get());
        appendMappings(g_insertKeymap.get());
        appendMappings(g_commandKeymap.get());
    }
    return result;
}

void Keymap::clearAllDynamicMappings() {
    if (g_normalKeymap) g_normalKeymap->clearDynamicMappings();
    if (g_visualKeymap) g_visualKeymap->clearDynamicMappings();
    if (g_insertKeymap) g_insertKeymap->clearDynamicMappings();
    if (g_commandKeymap) g_commandKeymap->clearDynamicMappings();
}