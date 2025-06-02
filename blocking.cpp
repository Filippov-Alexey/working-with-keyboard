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

struct KeyInfo {
    int key_code;
    std::string key_name;
    std::string status;
};

std::vector<KeyInfo> keyInfoList;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) { // Проверка на корректность nCode
        KBDLLHOOKSTRUCT* keyboard = (KBDLLHOOKSTRUCT*)lParam;
        int vkCode = keyboard->vkCode;

        std::stringstream hexStream;
        hexStream << std::hex << vkCode;

        bool isKeyDown = (wParam == WM_KEYDOWN);
        bool isBlocked = keysToBlock.count(vkCode) && keysToBlock[vkCode];

        std::string keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "Unknown";
        std::string status = isBlocked ? "Blocked" : "Not blocked";

        // Вывод данных в формате JSON
        std::cout << "{\"key_code\": \"0x" << std::uppercase << hexStream.str()
            << "\", \"key_name\": \"" << keyName << "\", \"status\": \"" << status << "\", \"event\": \""
            << (isKeyDown ? "keydown" : (wParam == WM_KEYUP ? "keyup" : "unknown")) << "\"}" << std::endl;

        // Блокировка только событий WM_KEYDOWN, которые помечены как заблокированные.
        return isBlocked && isKeyDown ? 1 : CallNextHookEx(keyboardHook, nCode, wParam, lParam);
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
        line.erase(remove(line.begin(), line.end(), ' '), line.end());
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string keyName = line.substr(1, pos - 2); // Извлечение названия клавиши, обработка кавычек
            std::string hexCode = line.substr(pos + 1);
            try {
                int keyCode = std::stoi(hexCode, nullptr, 16);
                keyDictionary[keyCode] = keyName;
            }
            catch (const std::invalid_argument& e) {
                std::cerr << "Error parsing key code: " << hexCode << std::endl;
            }
        }
    }
    file.close();
}

void ParseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string keyName = argv[i];
        for (const auto& pair : keyDictionary) { // Правильный синтаксис для итерации по элементам
            int keyCode = pair.first; // Клавиша
            std::string name = pair.second; // Название
            if (name == keyName) {
                keysToBlock[keyCode] = true;
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
