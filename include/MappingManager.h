#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "NppVim.h"

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
    bool recursive;
    MappingMode mode;
};

class MappingManager {
public:
    static MappingManager& getInstance();

    void addMapping(MappingMode mode, const std::string& from, const std::string& to, bool recursive);
    void removeMapping(MappingMode mode, const std::string& from);
    void clearMappings(MappingMode mode = MappingMode::All);

    bool resolveMapping(MappingMode mode, const std::string& input, std::string& output, bool& isRecursive) const;
    
    std::vector<Mapping> getMappings(MappingMode mode) const;

private:
    MappingManager() = default;
    
    // mode -> {from -> Mapping}
    std::unordered_map<MappingMode, std::unordered_map<std::string, Mapping>> mappings;
};
