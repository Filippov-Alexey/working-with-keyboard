#include <windows.h>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <algorithm>
#include <vector>

HHOOK keyboardHook;
std::map<int, std::string> keyDictionary;
std::map<int, bool> keysToBlock;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int vkCode = keyboard->vkCode;

        std::stringstream hexStream;
        hexStream << std::hex << std::uppercase << vkCode;

        bool isKeyDown = (wParam == WM_KEYDOWN);
        bool isBlocked = keysToBlock[vkCode];

        std::string keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "Unknown";
        std::string status = isBlocked ? "Blocked" : "Not blocked";

        std::cout << "{\"key_code\": \"0x" << hexStream.str()
                  << "\", \"key_name\": \"" << keyName 
                  << "\", \"status\": \"" << status 
                  << "\", \"event\": \"" << (isKeyDown ? "keydown" : "keyup") << "\"}" << std::endl;

        return (isBlocked && isKeyDown) ? 1 : CallNextHookEx(keyboardHook, nCode, wParam, lParam);
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

void ParseKeyCodeFile(const std::string& fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "Error opening key code file: " << fileName << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string keyName = line.substr(1, pos - 2); 
            std::string hexCode = line.substr(pos + 1);
            try {
                int keyCode = std::stoi(hexCode, nullptr, 16);
                keyDictionary[keyCode] = keyName;
            } catch (const std::invalid_argument&) {
                std::cerr << "Error parsing key code: " << hexCode << std::endl;
            }
        }
    }
}

void ParseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string keyName = argv[i];
        for (const auto& pair : keyDictionary) {
            if (pair.second == keyName) {
                keysToBlock[pair.first] = true;
                break;
            }
        }
    }
}

void SetHook() {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    if (!keyboardHook) {
        std::cerr << "Failed to install hook: " << GetLastError() << std::endl;
        exit(1);
    }
}

void Unhook() {
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
    }
}

int main(int argc, char* argv[]) {
    ParseKeyCodeFile("key_code.txt");
    ParseArguments(argc, argv);
    SetHook();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Unhook();
    return 0;
}
