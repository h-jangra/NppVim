#include "../include/OptionRegistry.h"
#include "../include/Utils.h"
#include <sstream>
#include <algorithm>

OptionRegistry& OptionRegistry::getInstance() {
    static OptionRegistry instance;
    return instance;
}

std::string OptionRegistry::resolveAlias(const std::string& name) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"so", "scrolloff"},
        {"nu", "number"},
        {"rnu", "relativenumber"},
        {"tw", "textwidth"},
        {"ic", "ignorecase"},
        {"noic", "noignorecase"},
        {"hls", "hlsearch"},
        {"nohls", "nohlsearch"},
        {"et", "expandtab"},
        {"ts", "tabstop"},
        {"sw", "shiftwidth"},
        {"cul", "cursorline"},
        {"wrap", "wrap"},
        {"list", "list"}
    };
    auto it = aliases.find(name);
    return (it != aliases.end()) ? it->second : name;
}

void OptionRegistry::registerOption(const std::string& name, OptionType type, OptionValue defaultValue, OptionSetter setter, const std::string& desc) {
    options[name] = { name, type, defaultValue, defaultValue, setter, desc };
    if (setter) {
        setter(defaultValue);
    }
}

bool OptionRegistry::setOption(const std::string& nameInput, const OptionValue& value) {
    std::string name = resolveAlias(nameInput);
    auto it = options.find(name);
    if (it != options.end()) {
        it->second.value = value;
        if (it->second.setter) {
            it->second.setter(value);
        }
        return true;
    }
    return false;
}

void OptionRegistry::resetToDefaults() {
    for (auto& pair : options) {
        pair.second.value = pair.second.defaultValue;
        if (pair.second.setter) {
            pair.second.setter(pair.second.defaultValue);
        }
    }
}

bool OptionRegistry::setOptionFromString(const std::string& line) {
    std::string s = Utils::trim(line);
    if (s.find("set ") == 0) s = Utils::trim(s.substr(4));

    if (s.empty()) return false;

    // Split s by whitespace into individual option strings
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(s);
    while (ss >> token) {
        tokens.push_back(token);
    }

    bool allSuccess = true;

    for (const auto& tok : tokens) {
        std::string name = tok;
        OptionValue value;
        bool boolVal = true;

        size_t eqPos = tok.find('=');
        if (eqPos != std::string::npos) {
            name = Utils::trim(tok.substr(0, eqPos));
            std::string valStr = Utils::trim(tok.substr(eqPos + 1));
            name = resolveAlias(name);

            // Check if it's a number or string
            try {
                value = std::stoi(valStr);
            } catch (...) {
                value = valStr;
            }
        } else {
            if (name.find("no") == 0) {
                std::string potentialName = resolveAlias(name.substr(2));
                if (options.count(potentialName)) {
                    name = potentialName;
                    boolVal = false;
                } else if (options.count(resolveAlias(name))) {
                    name = resolveAlias(name);
                    boolVal = true;
                } else {
                    name = potentialName;
                    boolVal = false;
                }
            } else {
                name = resolveAlias(name);
            }
            value = boolVal;
        }

        if (!setOption(name, value)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

OptionValue OptionRegistry::getOption(const std::string& nameInput) const {
    std::string name = resolveAlias(nameInput);
    auto it = options.find(name);
    if (it != options.end()) {
        return it->second.value;
    }
    return false;
}

std::vector<OptionInfo> OptionRegistry::getAllOptions() const {
    std::vector<OptionInfo> result;
    for (auto const& [name, info] : options) {
        result.push_back(info);
    }
    return result;
}
