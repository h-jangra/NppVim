#include "../include/ConfigManager.h"
#include "../plugin/Notepad_plus_msgs.h"
#include "../include/Utils.h"
#include "../include/NppVim.h"
#include <fstream>
#include <shlobj.h>
#include <iostream>

extern NppData nppData;

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

std::string ConfigManager::getPluginsConfigDir() {
    TCHAR configDir[MAX_PATH];
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);

    std::string path;
#ifdef UNICODE
    int len = WideCharToMultiByte(CP_UTF8, 0, configDir, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        path.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, configDir, -1, &path[0], len, NULL, NULL);
        path.pop_back();
    }
#else
    path = configDir;
#endif
    return path;
}

std::string ConfigManager::getConfigPath() {
    return getPluginsConfigDir() + "\\NppVim\\config.ini";
}

void ConfigManager::loadConfig() {
    std::string path = getConfigPath();
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;

        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            start = key.find_first_not_of(" \t");
            end = key.find_last_not_of(" \t");
            if (start != std::string::npos) key = key.substr(start, end - start + 1);

            start = value.find_first_not_of(" \t");
            end = value.find_last_not_of(" \t");
            if (start != std::string::npos) value = value.substr(start, end - start + 1);
            
            if (key == "enabled" || key == "vim_enabled") enabled = (value == "1" || value == "true");
            else if (key == "show_status_bar") showStatusBar = (value == "1" || value == "true");
            else if (key == "debug_logging") debugLogging = (value == "1" || value == "true");
            else if (key == "rc_file") rcFilePath = value;
            else if (key == "escape_key") g_config.escapeKey = value;
            else if (key == "escape_timeout") {
                try {
                    int timeout = std::stoi(value);
                    if (timeout >= 100 && timeout <= 1000) g_config.escapeTimeout = timeout;
                } catch (...) {}
            }
            else if (key == "custom_escape") g_config.customEscape = value;
            else if (key == "override_ctrl_d") g_config.overrideCtrlD = (value == "1" || value == "true");
            else if (key == "override_ctrl_u") g_config.overrideCtrlU = (value == "1" || value == "true");
            else if (key == "override_ctrl_r") g_config.overrideCtrlR = (value == "1" || value == "true");
            else if (key == "override_ctrl_f") g_config.overrideCtrlF = (value == "1" || value == "true");
            else if (key == "override_ctrl_b") g_config.overrideCtrlB = (value == "1" || value == "true");
            else if (key == "override_ctrl_o") g_config.overrideCtrlO = (value == "1" || value == "true");
            else if (key == "override_ctrl_i") g_config.overrideCtrlI = (value == "1" || value == "true");
            else if (key == "override_ctrl_v") g_config.overrideCtrlV = (value == "1" || value == "true");
            else if (key == "override_ctrl_a") g_config.overrideCtrlA = (value == "1" || value == "true");
            else if (key == "override_ctrl_x") g_config.overrideCtrlX = (value == "1" || value == "true");
            else if (key == "x_store_clipboard") g_config.xStoreClipboard = (value == "1" || value == "true");
            else if (key == "d_store_clipboard") g_config.dStoreClipboard = (value == "1" || value == "true");
            else if (key == "c_store_clipboard") g_config.cStoreClipboard = (value == "1" || value == "true");
            else if (key == "keyboard_layout_switching" || key == "keylayout") g_config.enableKeyboardLayoutSwitching = (value == "1" || value == "true");
            else if (key == "normallayout") g_config.normallayout = value;
            else if (key == "insertlayout") g_config.insertlayout = value;
        }
    }
}

void ConfigManager::saveConfig() {
    std::string path = getConfigPath();
    std::string dir = path.substr(0, path.find_last_of('\\'));
    CreateDirectoryA(dir.c_str(), NULL);

    std::ofstream file(path);
    if (file.is_open()) {
        file << "[General]\n";
        file << "enabled=" << (enabled ? "1" : "0") << "\n";
        file << "show_status_bar=" << (showStatusBar ? "1" : "0") << "\n";
        file << "debug_logging=" << (debugLogging ? "1" : "0") << "\n";
        file << "rc_file=" << (rcFilePath.empty() ? getRcPath() : rcFilePath) << "\n";
        
        file << "\n# NppVim Configuration File\n";
        file << "# Escape key options: esc, jj, jk, kj\n";
        file << "escape_key=" << g_config.escapeKey << "\n";
        file << "# Timeout for two-key escape sequences (100-1000ms)\n";
        file << "escape_timeout=" << g_config.escapeTimeout << "\n";
        file << "custom_escape=" << g_config.customEscape << "\n";
        file << "override_ctrl_d=" << (g_config.overrideCtrlD ? "1" : "0") << "\n";
        file << "override_ctrl_u=" << (g_config.overrideCtrlU ? "1" : "0") << "\n";
        file << "override_ctrl_r=" << (g_config.overrideCtrlR ? "1" : "0") << "\n";
        file << "override_ctrl_f=" << (g_config.overrideCtrlF ? "1" : "0") << "\n";
        file << "override_ctrl_b=" << (g_config.overrideCtrlB ? "1" : "0") << "\n";
        file << "override_ctrl_o=" << (g_config.overrideCtrlO ? "1" : "0") << "\n";
        file << "override_ctrl_i=" << (g_config.overrideCtrlI ? "1" : "0") << "\n";
        file << "override_ctrl_v=" << (g_config.overrideCtrlV ? "1" : "0") << "\n";
        file << "override_ctrl_a=" << (g_config.overrideCtrlA ? "1" : "0") << "\n";
        file << "override_ctrl_x=" << (g_config.overrideCtrlX ? "1" : "0") << "\n";
        file << "x_store_clipboard=" << (g_config.xStoreClipboard ? "1" : "0") << "\n";
        file << "d_store_clipboard=" << (g_config.dStoreClipboard ? "1" : "0") << "\n";
        file << "c_store_clipboard=" << (g_config.cStoreClipboard ? "1" : "0") << "\n";
        file << "keyboard_layout_switching=" << (g_config.enableKeyboardLayoutSwitching ? "1" : "0") << "\n";
        file << "normallayout=" << g_config.normallayout << "\n";
        file << "insertlayout=" << g_config.insertlayout << "\n";
    }
}

std::string ConfigManager::getRcPath() {
    if (!rcFilePath.empty()) return rcFilePath;

    // 1. Check %USERPROFILE%\.nppvimrc
    char userProfile[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userProfile))) {
        std::string path = std::string(userProfile) + "\\.nppvimrc";
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
    }

    // 2. Check %APPDATA%\Notepad++\plugins\Config\NppVim\nppvim.rc
    std::string defaultPath = getPluginsConfigDir() + "\\NppVim\\nppvim.rc";
    return defaultPath;
}

void ConfigManager::ensureDefaultFiles() {
    std::string configPath = getConfigPath();
    std::string dir = configPath.substr(0, configPath.find_last_of('\\'));
    CreateDirectoryA(dir.c_str(), NULL);

    if (GetFileAttributesA(configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        saveConfig();
    }

    std::string rcPath = getRcPath();
    if (GetFileAttributesA(rcPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::ofstream file(rcPath);
        if (file.is_open()) {
            file << "\" NppVim Configuration\n";
            file << "set relativenumber\n";
            file << "set hlsearch\n\n";
            file << "nnoremap H ^\n";
            file << "nnoremap L $\n\n";
            file << "inoremap jj <Esc>\n";
        }
    }
}

void ConfigManager::editRc() {
    std::string path = getRcPath();
    std::wstring wpath(path.begin(), path.end());
    ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)wpath.c_str());
}

void ConfigManager::editIni() {
    std::string path = getConfigPath();
    std::wstring wpath(path.begin(), path.end());
    ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)wpath.c_str());
}
