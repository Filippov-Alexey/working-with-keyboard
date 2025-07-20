#include <windows.h>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>

HHOOK keyboardHook;
std::map<std::string, int> keyDictionary;
std::map<int, int> keyRemap;
std::map<std::string, std::vector<int>> hotKeys;

std::string ToLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return lowerStr;
}

std::vector<std::string> SplitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}


std::string GetHotkeyString(int vkCode) {
    std::string hotKeyStr;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        hotKeyStr += "ctrl+";
    }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        hotKeyStr += "shift+";
    }
    if (GetAsyncKeyState(VK_MENU) & 0x8000) {
        hotKeyStr += "alt+";
    }
    if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) {
        hotKeyStr += "win+";
    }
    for (const auto& entry : keyDictionary) {
        if (entry.second == vkCode) {
            hotKeyStr += entry.first;
            break;
        }
    }

    return hotKeyStr;
}


bool AreAllKeysValid(const std::vector<std::string>& components) {
    for (const std::string& component : components) {
        auto it = keyDictionary.find(component);
        if (it == keyDictionary.end()) {
            std::cerr << "Error: Key \"" << component << "\" is not valid!" << std::endl;
            return false;
        }
    }
    return true;
}
void ParseArguments(int argc, char* argv[]) {
    if ((argc - 1) % 2 != 0) {
        std::cerr << "Error: Number of arguments must be even!" << std::endl;
        exit(1);
    }

    std::cout << "Starting to parse arguments..." << std::endl;
    for (int i = 1; i < argc; i += 2) {
        std::string hotKey = ToLower(argv[i]);
        std::string newKeyName = ToLower(argv[i + 1]);

        std::cout << "Processing hotkey \"" << hotKey << "\" and mapping it to key name \"" << newKeyName << "\"." << std::endl;

        std::vector<int> newKeyCodes;
        for (const auto& component : SplitString(newKeyName, '+')) {
            auto it = keyDictionary.find(component);
            if (it != keyDictionary.end()) {
                newKeyCodes.push_back(it->second);
            } else {
                std::cerr << "Error: Invalid key component \"" << component << "\"." << std::endl;
                exit(1);
            }
        }
        hotKeys.emplace(std::move(hotKey), std::move(newKeyCodes)); // Используем emplace
        std::cout << "Mapping hotkey: " << hotKey << " to new key codes: ";
        for (const auto& code : hotKeys[hotKey]) {
            std::cout << code << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "Argument parsing completed." << std::endl;
}

void ParseKeyCodeFile(const std::string& fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "Could not open the file: " << fileName << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line.find('\'') == std::string::npos) continue; 

        size_t start = line.find('\'') + 1;
        size_t end = line.find('\'', start);
        std::string key = line.substr(start, end - start);
        
        if (line.find("0x") != std::string::npos) {
            size_t hexStart = line.find("0x") + 2;
            int value;
            std::stringstream ss(line.substr(hexStart));
            ss >> std::hex >> value;
            keyDictionary.emplace(std::move(key), value); 
        }
    }
}
void PressKeyCombination(const std::vector<int>& keyCodes) {
    for (int keyCode : keyCodes) {
        INPUT inputDown = { 0 };
        inputDown.type = INPUT_KEYBOARD;
        inputDown.ki.wVk = keyCode;
        SendInput(1, &inputDown, sizeof(INPUT));
        std::cout << "Pressed key: " << keyCode << std::endl;
    }

    for (auto it = keyCodes.rbegin(); it != keyCodes.rend(); ++it) {
        INPUT inputUp = { 0 };
        inputUp.type = INPUT_KEYBOARD;
        inputUp.ki.wVk = *it;
        inputUp.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &inputUp, sizeof(INPUT));
        std::cout << "Released key: " << *it << std::endl;
    }
}
void PressKey(int keyCode) {
    INPUT inputDown = { 0 };
    inputDown.type = INPUT_KEYBOARD;
    inputDown.ki.wVk = keyCode;

    SendInput(1, &inputDown, sizeof(INPUT));
    std::cout << "Pressed key: " << keyCode << std::endl;

    INPUT inputUp = { 0 };
    inputUp.type = INPUT_KEYBOARD;
    inputUp.ki.wVk = keyCode;
    inputUp.ki.dwFlags = KEYEVENTF_KEYUP; 

    SendInput(1, &inputUp, sizeof(INPUT));
    std::cout << "Released key: " << keyCode << std::endl;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int vkCode = keyboard->vkCode;
        std::cout << "Key pressed: " << vkCode << std::endl;

        std::string currentHotKey = GetHotkeyString(vkCode);
        std::cout << "Constructed hotkey: " << currentHotKey << std::endl;

        auto it = hotKeys.find(currentHotKey);
        if (it != hotKeys.end()) {
            PressKeyCombination(it->second);
            return 1; // Блокируем событие, если нашли горячую клавишу
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
 
void SetHook() {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!keyboardHook) {
        std::cerr << "Failed to install hook!" << std::endl;
        exit(1);
    }
}


void Unhook() {
    UnhookWindowsHookEx(keyboardHook);
}

int main(int argc, char* argv[]) {
    ParseKeyCodeFile("key_code.txt");

    if (argc > 1) {
        ParseArguments(argc, argv);
    }

    SetHook();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Unhook();
    return 0;
}