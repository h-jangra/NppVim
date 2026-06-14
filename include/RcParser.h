#pragma once
#include <string>
#include <vector>
#include <windows.h>

class RcParser {
public:
    static RcParser& getInstance();

    bool parseFile(const std::string& path, HWND hwndEdit = nullptr);
    bool executeLine(const std::string& line, HWND hwndEdit = nullptr);

private:
    RcParser() = default;
    
    std::string trim(const std::string& s);
    bool isComment(const std::string& line);
    
    void handleSet(const std::string& args);
    void handleMapping(const std::string& cmd, const std::string& args);
    void handleUnmapping(const std::string& cmd, const std::string& args);
    void handleSource(const std::string& path, HWND hwndEdit);
    void handleCommandDefinition(const std::string& args);
};
