#pragma once
#include <string>
#include <unordered_map>
#include <vector>

enum class RegisterType {
    CharacterWise,
    LineWise,
    BlockWise
};

struct RegisterEntry {
    std::string text;
    RegisterType type = RegisterType::CharacterWise;
};

class Registers {
public:
    static Registers& getInstance();

    // Read
    std::string get(char reg) const;
    RegisterEntry getEntry(char reg) const;
    RegisterType getType(char reg) const;
    bool isLinewise(char reg) const;

    // Write
    void set(char reg, const std::string& text, RegisterType type = RegisterType::CharacterWise, bool syncClipboard = true);
    void append(char reg, const std::string& text);

    // Operator-specific actions (handles uppercase append, shift 1-9 for delete, unnamed sync)
    void saveDeleted(char reg, const std::string& text, RegisterType type, bool syncClipboard);
    void saveYanked(char reg, const std::string& text, RegisterType type, bool syncClipboard);
    void saveChanged(char reg, const std::string& text, RegisterType type, bool syncClipboard);

    static bool isValidRegister(char reg);
    static bool isReadOnly(char reg);

    char getActiveRegister() const;
    void setActiveRegister(char reg);
    void resetActiveRegister();

    // System clipboard
    static void setClipboardText(const std::string& text);
    static std::string getClipboardText();

    // Formatted display for :registers
    std::string formatRegisters(const std::string& filter = "") const;

    void clear();

private:
    Registers();
    char activeRegister = '"';
    std::unordered_map<char, RegisterEntry> registers;
};
