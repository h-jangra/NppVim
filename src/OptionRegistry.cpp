#include "../include/OptionRegistry.h"
#include <sstream>
#include <algorithm>

OptionRegistry& OptionRegistry::getInstance() {
    static OptionRegistry instance;
    return instance;
}

void OptionRegistry::registerOption(const std::string& name, OptionType type, OptionValue defaultValue, OptionSetter setter, const std::string& desc) {
    options[name] = { name, type, defaultValue, defaultValue, setter, desc };
    if (setter) {
        setter(defaultValue);
    }
}

bool OptionRegistry::setOption(const std::string& name, const OptionValue& value) {
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
    // Expected formats:
    // set number
    // set nonumber
    // set nu
    // set nonu
    // set clipboard=unnamed
    
    std::string s = line;
    if (s.find("set ") == 0) s = s.substr(4);
    
    // Trim
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);

    if (s.empty()) return false;

    std::string name = s;
    OptionValue value;
    bool isBool = true;
    bool boolVal = true;

    size_t eqPos = s.find('=');
    if (eqPos != std::string::npos) {
        name = s.substr(0, eqPos);
        std::string valStr = s.substr(eqPos + 1);
        isBool = false;
        
        // Check if it's a number or string
        try {
            value = std::stoi(valStr);
        } catch (...) {
            value = valStr;
        }
    } else {
        if (name.find("no") == 0) {
            std::string potentialName = name.substr(2);
            // Handle aliases before checking prefix
            if (potentialName == "nu") potentialName = "number";
            if (potentialName == "rnu") potentialName = "relativenumber";

            if (options.count(potentialName)) {
                name = potentialName;
                boolVal = false;
            } else if (options.count(name)) {
                // it's an option that happens to start with 'no'
                boolVal = true;
            }
        }
        value = boolVal;
    }

    // Handle aliases (for positive case)
    if (name == "nu") name = "number";
    if (name == "rnu") name = "relativenumber";

    return setOption(name, value);
}

OptionValue OptionRegistry::getOption(const std::string& name) const {
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
