#pragma once
#include <string>
#include <vector>
#include <windows.h>

class ConfigManager {
public:
    static ConfigManager& getInstance();

    void loadConfig();
    void saveConfig();
    
    std::string getRcPath();
    std::string getConfigPath();
    
    void ensureDefaultFiles();
    
    // Settings accessors
    bool isEnabled() const { return enabled; }
    void setEnabled(bool v) { enabled = v; }
    
    bool isDebugLogging() const { return debugLogging; }
    bool isShowStatusBar() const { return showStatusBar; }
    
    std::string getRcFilePathOverride() const { return rcFilePath; }

    void editRc();
    void editIni();

private:
    ConfigManager() = default;
    
    bool enabled = true;
    bool showStatusBar = true;
    bool debugLogging = false;
    std::string rcFilePath = "";

    std::string getPluginsConfigDir();
};
