#include <windows.h>
#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#pragma comment(lib, "user32.lib")

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

        std::vector<std::string> hotKeyComponents = SplitString(hotKey, '+');
        std::vector<int> newKeyCodes;

        if (!AreAllKeysValid(hotKeyComponents)) {
            exit(1);
        }

        for (const auto& component : SplitString(newKeyName, '+')) {
            int keyCode = keyDictionary[component]; 
            if (keyCode > 0) {
                newKeyCodes.push_back(keyCode);
            }
            else {
                std::cerr << "Error: Invalid key component \"" << component << "\"." << std::endl;
                exit(1);
            }
        }

        hotKeys[hotKey] = newKeyCodes; 

        std::cout << "Mapping hotkey: " << hotKey << " to new key codes: ";
        for (const int& code : newKeyCodes) {
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
    int value;


    while (std::getline(file, line)) {
        std::istringstream iss(line);


        if (line.find('\'') != std::string::npos) {
            size_t start = line.find('\'') + 1;
            size_t end = line.find('\'', start);
            std::string key = line.substr(start, end - start);

            if (line.find("0x") != std::string::npos) {
                size_t start = line.find("0x") + 2;
                std::stringstream ss(line.substr(start));
                ss >> std::hex >> value;


                keyDictionary[key] = value;
            }
        }
    }

    file.close();
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
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* keyboard = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN) {
            int vkCode = keyboard->vkCode;
            std::cout << "Key pressed: " << vkCode << std::endl;

            std::string currentHotKey = GetHotkeyString(vkCode);
            std::cout << "Constructed hotkey: " << currentHotKey << std::endl;

            auto it = hotKeys.find(currentHotKey);
            if (it != hotKeys.end()) {
                const std::vector<int>& newKeyCodes = it->second;

                std::cout << "Hotkey matched: " << currentHotKey << std::endl;

                PressKeyCombination(newKeyCodes); 

                return 1; 
            }
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