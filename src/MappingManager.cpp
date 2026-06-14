#include "../include/MappingManager.h"
#include <algorithm>

MappingManager& MappingManager::getInstance() {
    static MappingManager instance;
    return instance;
}

void MappingManager::addMapping(MappingMode mode, const std::string& from, const std::string& to, bool recursive) {
    mappings[mode][from] = { from, to, recursive, mode };
}

void MappingManager::removeMapping(MappingMode mode, const std::string& from) {
    mappings[mode].erase(from);
}

void MappingManager::clearMappings(MappingMode mode) {
    if (mode == MappingMode::All) {
        mappings.clear();
    } else {
        mappings[mode].clear();
    }
}

bool MappingManager::resolveMapping(MappingMode mode, const std::string& input, std::string& output, bool& isRecursive) const {
    auto itMode = mappings.find(mode);
    if (itMode != mappings.end()) {
        auto itMap = itMode->second.find(input);
        if (itMap != itMode->second.end()) {
            output = itMap->second.to;
            isRecursive = itMap->second.recursive;
            return true;
        }
    }

    // Check generic mapping if mode-specific not found
    if (mode != MappingMode::All) {
        itMode = mappings.find(MappingMode::All);
        if (itMode != mappings.end()) {
            auto itMap = itMode->second.find(input);
            if (itMap != itMode->second.end()) {
                output = itMap->second.to;
                isRecursive = itMap->second.recursive;
                return true;
            }
        }
    }

    return false;
}

std::vector<Mapping> MappingManager::getMappings(MappingMode mode) const {
    std::vector<Mapping> result;
    auto itMode = mappings.find(mode);
    if (itMode != mappings.end()) {
        for (auto const& [from, mapping] : itMode->second) {
            result.push_back(mapping);
        }
    }
    return result;
}
