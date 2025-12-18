#include "../include/Keymap.h"
#include "../include/NppVim.h"
#include "../include/Utils.h"

std::unique_ptr<Keymap> g_normalKeymap;

Keymap::Keymap(VimState& state) 
    : state(state), root(std::make_shared<KeymapNode>()), currentNode(root) {}

Keymap& Keymap::set(const std::string& keys, KeyHandler handler) {
    insertKeySequence(keys, handler);
    return *this;
}

Keymap& Keymap::motion(const std::string& keys, char motionChar, KeyHandler handler) {
    insertKeySequence(keys, handler, motionChar);
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
    if (std::isdigit(static_cast<unsigned char>(key))) {
        int digit = key - '0';
        if (key == '0' && state.repeatCount == 0 && pendingKeys.empty()) {
            auto it = currentNode->children.find(key);
            if (it != currentNode->children.end() && it->second->isLeaf) {
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
        reset();

        it = root->children.find(key);
        if (it == root->children.end()) {
            return false;
        }

        currentNode = it->second;
        pendingKeys = key;
    } else {
        currentNode = it->second;
        pendingKeys += key;
    }

    if (currentNode->isLeaf && currentNode->handler) {
        currentNode->handler(hwnd, count);

        if (currentNode->motionChar) {
            state.recordLastOp(OP_MOTION, count, currentNode->motionChar);
        }

        reset();
        return true;
    }

    std::wstring status = L"-- ";
    for (char c : pendingKeys) status += (wchar_t)c;
    status += L" --";
    Utils::setStatus(status.c_str());

    return true;
}

void Keymap::reset() {
    currentNode = root;
    pendingKeys.clear();
    state.repeatCount = 0;
}