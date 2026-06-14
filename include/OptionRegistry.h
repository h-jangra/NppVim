#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <variant>
#include <vector>

enum class OptionType {
    Bool,
    Number,
    String
};

using OptionValue = std::variant<bool, int, std::string>;
using OptionSetter = std::function<void(const OptionValue&)>;

struct OptionInfo {
    std::string name;
    OptionType type;
    OptionValue value;
    OptionValue defaultValue;
    OptionSetter setter;
    std::string description;
};

class OptionRegistry {
public:
    static OptionRegistry& getInstance();

    void registerOption(const std::string& name, OptionType type, OptionValue defaultValue, OptionSetter setter = nullptr, const std::string& desc = "");
    
    bool setOption(const std::string& name, const OptionValue& value);
    bool setOptionFromString(const std::string& line); // parses "set nu", "set nonu", "set clipboard=unnamed"
    
    void resetToDefaults();

    OptionValue getOption(const std::string& name) const;
    
    std::vector<OptionInfo> getAllOptions() const;

private:
    OptionRegistry() = default;
    std::unordered_map<std::string, OptionInfo> options;
};
