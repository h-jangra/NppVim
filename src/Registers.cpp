#include "../include/Registers.h"
#include <windows.h>
#include <cctype>
#include <set>
#include <algorithm>

Registers& Registers::getInstance() {
    static Registers instance;
    return instance;
}

Registers::Registers() : activeRegister('"') {
}

bool Registers::isValidRegister(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '"' || c == '_' || c == '+' || c == '*' ||
           c == '/' || c == ':' || c == '.' || c == '%' || c == '-';
}

bool Registers::isReadOnly(char c) {
    return c == ':' || c == '.' || c == '%' || c == '/';
}

char Registers::getActiveRegister() const {
    return activeRegister;
}

void Registers::setActiveRegister(char reg) {
    if (isValidRegister(reg)) {
        activeRegister = reg;
    }
}

void Registers::resetActiveRegister() {
    activeRegister = '"';
}

std::string Registers::get(char reg) const {
    if (reg == '_') return "";
    if (reg == '+' || reg == '*') {
        return getClipboardText();
    }
    if (reg >= 'A' && reg <= 'Z') {
        reg = (char)std::tolower(static_cast<unsigned char>(reg));
    }
    auto it = registers.find(reg);
    if (it != registers.end()) {
        return it->second.text;
    }
    if (reg == '"') {
        return getClipboardText();
    }
    return "";
}

RegisterEntry Registers::getEntry(char reg) const {
    if (reg == '_') return { "", RegisterType::CharacterWise };
    if (reg == '+' || reg == '*') {
        return { getClipboardText(), RegisterType::CharacterWise };
    }
    if (reg >= 'A' && reg <= 'Z') {
        reg = (char)std::tolower(static_cast<unsigned char>(reg));
    }
    auto it = registers.find(reg);
    if (it != registers.end()) {
        return it->second;
    }
    if (reg == '"') {
        return { getClipboardText(), RegisterType::CharacterWise };
    }
    return { "", RegisterType::CharacterWise };
}

RegisterType Registers::getType(char reg) const {
    return getEntry(reg).type;
}

bool Registers::isLinewise(char reg) const {
    return getType(reg) == RegisterType::LineWise;
}

void Registers::set(char reg, const std::string& text, RegisterType type, bool syncClipboard) {
    if (reg == '_') return;

    if (reg == '+' || reg == '*') {
        setClipboardText(text);
        registers['+'] = { text, type };
        registers['*'] = { text, type };
        return;
    }

    if (reg >= 'A' && reg <= 'Z') {
        append((char)std::tolower(static_cast<unsigned char>(reg)), text);
        return;
    }

    registers[reg] = { text, type };

    if (reg == '"' && syncClipboard) {
        setClipboardText(text);
    }
}

void Registers::append(char reg, const std::string& text) {
    if (reg == '_') return;
    if (reg >= 'A' && reg <= 'Z') {
        reg = (char)std::tolower(static_cast<unsigned char>(reg));
    }
    if (registers.find(reg) != registers.end()) {
        registers[reg].text += text;
    } else {
        registers[reg] = { text, RegisterType::CharacterWise };
    }
}

void Registers::saveDeleted(char reg, const std::string& text, RegisterType type, bool syncClipboard) {
    if (reg == '_') return;

    // Shift numbered registers for linewise/large deletes
    bool isLargeOrLinewise = (type == RegisterType::LineWise) || (text.find('\n') != std::string::npos);
    if (isLargeOrLinewise) {
        for (char i = '9'; i > '1'; i--) {
            char prev = i - 1;
            if (registers.count(prev)) {
                registers[i] = registers[prev];
            }
        }
        registers['1'] = { text, type };
    } else {
        registers['-'] = { text, type };
    }

    // Always update unnamed register
    registers['"'] = { text, type };
    if (syncClipboard) {
        setClipboardText(text);
    }

    // If a specific register was specified
    if (reg != '"') {
        set(reg, text, type, syncClipboard);
    }
}

void Registers::saveYanked(char reg, const std::string& text, RegisterType type, bool syncClipboard) {
    if (reg == '_') return;

    registers['0'] = { text, type };
    registers['"'] = { text, type };

    if (syncClipboard) {
        setClipboardText(text);
    }

    if (reg != '"') {
        set(reg, text, type, syncClipboard);
    }
}

void Registers::saveChanged(char reg, const std::string& text, RegisterType type, bool syncClipboard) {
    saveDeleted(reg, text, type, syncClipboard);
}

void Registers::setClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (h) {
        char* p = (char*)GlobalLock(h);
        if (p) {
            memcpy(p, text.c_str(), text.size() + 1);
            GlobalUnlock(h);
            SetClipboardData(CF_TEXT, h);
        }
    }
    CloseClipboard();
}

std::string Registers::getClipboardText() {
    std::string result;
    if (!OpenClipboard(nullptr)) return result;
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char* pszText = (char*)GlobalLock(hData);
        if (pszText) {
            result = pszText;
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

void Registers::clear() {
    registers.clear();
    activeRegister = '"';
}

std::string Registers::formatRegisters(const std::string& filter) const {
    std::set<char> filterSet;
    for (char c : filter) {
        if (c != ' ' && c != '\t') filterSet.insert(c);
    }

    auto shouldInclude = [&](char r) -> bool {
        if (filterSet.empty()) return true;
        return filterSet.find(r) != filterSet.end();
    };

    auto getPreview = [](const std::string& content, size_t maxLength = 35) -> std::string {
        std::string preview;
        size_t count = 0;
        bool truncated = false;
        for (size_t i = 0; i < content.size() && count < maxLength; i++) {
            char ch = content[i];
            if (ch == '\n' || ch == '\r') {
                if (count + 2 <= maxLength) {
                    preview += "\\n";
                    count += 2;
                } else {
                    truncated = true;
                    break;
                }
                if (ch == '\r' && i + 1 < content.size() && content[i + 1] == '\n') i++;
            } else if (ch == '\t') {
                if (count + 2 <= maxLength) {
                    preview += "\\t";
                    count += 2;
                } else {
                    truncated = true;
                    break;
                }
            } else if (ch >= 32 && ch <= 126) {
                preview += ch;
                count++;
            }
        }
        if (truncated || count >= maxLength) preview += "...";
        return preview;
    };

    auto countLines = [](const std::string& content) -> int {
        int lines = 0;
        for (char ch : content) {
            if (ch == '\n') lines++;
        }
        return lines + (content.empty() ? 0 : 1);
    };

    std::string out = "--- Registers ---\n";
    out += "Type Name  Preview                              Lines\n";
    out += "──── ───── ──────────────────────────────────── ─────\n";

    auto formatRow = [&](char name, const RegisterEntry& entry) {
        if (!shouldInclude(name)) return;
        if (entry.text.empty() && filterSet.empty()) return;
        const char* typeStr = "char";
        if (entry.type == RegisterType::LineWise) typeStr = "line";
        else if (entry.type == RegisterType::BlockWise) typeStr = "blok";
        std::string prev = getPreview(entry.text);
        int lines = countLines(entry.text);
        char row[256];
        sprintf_s(row, "%-4s \"%-3c  %-35s %4d\n", typeStr, name, prev.c_str(), lines);
        out += row;
    };

    formatRow('"', getEntry('"'));
    for (char c = '0'; c <= '9'; c++) formatRow(c, getEntry(c));
    for (char c = 'a'; c <= 'z'; c++) formatRow(c, getEntry(c));
    formatRow('-', getEntry('-'));

    if (shouldInclude('+') || shouldInclude('*')) {
        std::string clip = getClipboardText();
        std::string prev = getPreview(clip, 30);
        char row[256];
        sprintf_s(row, "sys  \"+    System clipboard               %s\n", prev.empty() ? "(empty)" : prev.c_str());
        out += row;
        sprintf_s(row, "sys  \"*    System clipboard (selection)   %s\n", prev.empty() ? "(empty)" : prev.c_str());
        out += row;
    }

    return out;
}
