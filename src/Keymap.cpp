#include "../include/Keymap.h"
#include "../include/NppVim.h"
#include "../include/Utils.h"
#include "../plugin/resource.h"

std::unique_ptr<Keymap> g_normalKeymap;
std::unique_ptr<Keymap> g_visualKeymap;
std::unique_ptr<Keymap> g_commandKeymap;

Keymap::Keymap(VimState& state) 
    : state(state), root(std::make_shared<KeymapNode>()), currentNode(root), savedHwnd(nullptr) {}

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

bool Keymap::handleKey(HWND hwnd, char key) {
    if (allowCount && currentNode == root && std::isdigit(static_cast<unsigned char>(key))) {
        if (key == '0' && state.repeatCount == 0) {
            auto it = root->children.find(key);
            if (it != root->children.end() && it->second->isLeaf) {
                return processKey(hwnd, key, 1);
            }
        }
        state.repeatCount = state.repeatCount * 10 + (key - '0');
        return true;
    }
    
    int count = (state.repeatCount > 0) ? state.repeatCount : 1;
    return processKey(hwnd, key, count);
}

bool Keymap::processKey(HWND hwnd, char key, int count) {
    auto it = currentNode->children.find(key);

    if (it != currentNode->children.end()) {
        currentNode = it->second;
        pendingKeys.push_back(key);

        if (currentNode->isLeaf && currentNode->handler) {
            if (currentNode->children.empty()) {
                currentNode->handler(hwnd, count);
                if (currentNode->motionChar)
                    state.recordLastOp(OP_MOTION, count, currentNode->motionChar);
                reset();
                return true;
            }

            return true;
        }
        return true;
    }

    if (currentNode != root) {
        auto node = currentNode;

        reset();

        if (node->isLeaf && node->handler) {
            node->handler(hwnd, count);
            if (node->motionChar)
                state.recordLastOp(OP_MOTION, count, node->motionChar);
            return true;
        }

        return processKey(hwnd, key, count);
    }
    return false;
}

bool Keymap::isWaitingForMoreKeys() const {
    return currentNode != root;
}

void Keymap::handleTimer() {
    if (currentNode != root && currentNode->isLeaf && currentNode->handler) {
        auto nodeToFire = currentNode;
        reset();
        if (savedHwnd) {
            nodeToFire->handler(savedHwnd, 1);
            if (nodeToFire->motionChar)
                state.recordLastOp(OP_MOTION, 1, nodeToFire->motionChar);
        }
    }
}

void Keymap::reset() {
    if (savedHwnd)
        KillTimer(savedHwnd, KEYMAP_TIMER_ID);

    savedHwnd = nullptr;
    currentNode = root;
    pendingKeys.clear();
    state.repeatCount = 0;
}