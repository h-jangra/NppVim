#include "../include/RcParser.h"
#include "../include/OptionRegistry.h"
#include "../include/MappingManager.h"
#include "../include/ConfigManager.h"
#include "../include/Utils.h"
#include "../include/Keymap.h"
#include <fstream>
#include <sstream>
#include <algorithm>

RcParser& RcParser::getInstance() {
    static RcParser instance;
    return instance;
}

std::string RcParser::trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return s;
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, (last - first + 1));
}

bool RcParser::isComment(const std::string& line) {
    if (line.empty()) return true;
    return line[0] == '"' || line[0] == '#';
}

bool RcParser::parseFile(const std::string& path, HWND hwndEdit) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        executeLine(line, hwndEdit);
    }
    return true;
}

#include "../include/CommandMode.h"
extern CommandMode* g_commandMode;

bool RcParser::executeLine(const std::string& line, HWND hwndEdit) {
    std::string s = trim(line);
    if (isComment(s)) return true;

    std::stringstream ss(s);
    std::string cmd;
    ss >> cmd;

    std::string args;
    std::getline(ss, args);
    args = trim(args);

    if (cmd == "set") {
        handleSet(args);
    } else if (cmd == "map" || cmd == "nmap" || cmd == "imap" || cmd == "vmap" ||
               cmd == "noremap" || cmd == "nnoremap" || cmd == "inoremap" || cmd == "vnoremap") {
        handleMapping(cmd, args);
    } else if (cmd == "unmap" || cmd == "nunmap" || cmd == "iunmap" || cmd == "vunmap") {
        handleUnmapping(cmd, args);
    } else if (cmd == "source" || cmd == "so") {
        handleSource(args, hwndEdit);
    } else if (cmd == "command" || cmd == "com") {
        handleCommandDefinition(args);
    } else {
        // Try as a colon command
        if (g_commandMode) {
            std::string c = s;
            if (!c.empty() && c[0] == ':') c = c.substr(1);
            g_commandMode->handleColonCommand(hwndEdit, c);
            return true;
        }
        return false;
    }

    return true;
}

#include "../include/CommandMode.h"

void RcParser::handleCommandDefinition(const std::string& args) {
    std::stringstream ss(args);
    std::string alias, target;
    ss >> alias;
    std::getline(ss, target);
    target = trim(target);

    if (!alias.empty() && !target.empty()) {
        CommandMode::addUserCommand(alias, target);
    }
}

void RcParser::handleSet(const std::string& args) {
    OptionRegistry::getInstance().setOptionFromString(args);
}

void RcParser::handleMapping(const std::string& cmd, const std::string& args) {
    bool recursive = (cmd.find("nore") == std::string::npos);
    
    std::stringstream ss(args);
    std::string from, to;
    ss >> from;
    std::getline(ss, to);
    to = trim(to);

    if (from.empty() || to.empty()) return;

    std::string transFrom = Utils::translateKeyNotation(from);
    std::string transTo = Utils::translateKeyNotation(to);

    if (cmd == "map" || cmd == "noremap") {
        if (g_normalKeymap) g_normalKeymap->addMapping(transFrom, transTo, recursive);
        if (g_visualKeymap) g_visualKeymap->addMapping(transFrom, transTo, recursive);
        if (g_insertKeymap) g_insertKeymap->addMapping(transFrom, transTo, recursive);
    } else if (cmd == "nmap" || cmd == "nnoremap") {
        if (g_normalKeymap) g_normalKeymap->addMapping(transFrom, transTo, recursive);
    } else if (cmd == "imap" || cmd == "inoremap") {
        if (g_insertKeymap) g_insertKeymap->addMapping(transFrom, transTo, recursive);
    } else if (cmd == "vmap" || cmd == "vnoremap") {
        if (g_visualKeymap) g_visualKeymap->addMapping(transFrom, transTo, recursive);
    }
    
    // Also store in MappingManager for listing
    MappingMode mode = MappingMode::All;
    if (cmd[0] == 'n') mode = MappingMode::Normal;
    else if (cmd[0] == 'i') mode = MappingMode::Insert;
    else if (cmd[0] == 'v') mode = MappingMode::Visual;
    MappingManager::getInstance().addMapping(mode, from, to, recursive);
}

void RcParser::handleUnmapping(const std::string& cmd, const std::string& args) {
    std::string from = trim(args);
    if (from.empty()) return;

    std::string transFrom = Utils::translateKeyNotation(from);

    if (cmd == "unmap") {
        if (g_normalKeymap) g_normalKeymap->removeMapping(transFrom);
        if (g_visualKeymap) g_visualKeymap->removeMapping(transFrom);
        if (g_insertKeymap) g_insertKeymap->removeMapping(transFrom);
    } else if (cmd == "nunmap") {
        if (g_normalKeymap) g_normalKeymap->removeMapping(transFrom);
    } else if (cmd == "iunmap") {
        if (g_insertKeymap) g_insertKeymap->removeMapping(transFrom);
    } else if (cmd == "vunmap") {
        if (g_visualKeymap) g_visualKeymap->removeMapping(transFrom);
    }
    
    MappingMode mode = MappingMode::All;
    if (cmd[0] == 'n') mode = MappingMode::Normal;
    else if (cmd[0] == 'i') mode = MappingMode::Insert;
    else if (cmd[0] == 'v') mode = MappingMode::Visual;
    MappingManager::getInstance().removeMapping(mode, from);
}

void RcParser::handleSource(const std::string& path, HWND hwndEdit) {
    parseFile(path, hwndEdit);
}
