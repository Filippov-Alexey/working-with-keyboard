#include <windows.h>
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <string>
HHOOK keyboardHook;
std::map<int, std::string> keyDictionary;

struct Hotkey {
    std::string combination; 
    bool isBlocked;
};
std::vector<Hotkey> hotkeysToBlock;
std::unordered_set<int> currentlyPressedKeys;     
std::unordered_set<std::string> currentlyPressedKeysHex; 
int i,k;
bool isBlocked = false;
bool isKey = false;
bool isCombinedBlocked = false; 
bool IsHotkeyBlocked(const std::string& combination) {
    return std::any_of(hotkeysToBlock.begin(), hotkeysToBlock.end(),
                       [&combination](const Hotkey& hotkey) {
                           return hotkey.combination == combination;
                       });
}

std::string VKCodeToHex(int vkCode) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << vkCode;
    return ss.str();
}
template<typename T>
std::string SetToString(const std::unordered_set<T>& set) {
    std::ostringstream oss;
    for (const auto& element : set) {
        oss << element << " "; 
    }
    return oss.str();
}


std::vector<std::string> GetCurrentCombinationarray() {
    std::vector<std::string> combinations;
    
    for (int vkCode : currentlyPressedKeys) {
        std::string keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "Unknown";
        combinations.push_back(keyName);
    }
    
    return combinations; 
}
std::string GetCurrentCombination() {
    std::string combination;
    std::vector<std::string> currentKeys = GetCurrentCombinationarray();
    for (const std::string& keyName : currentKeys) {
        if (!combination.empty()) {
            combination += "+";
        }
        combination += keyName;
    }
    return combination;
}
int HexToVKCode(const std::string& hex) {
    int vkCode;
    std::stringstream ss;
    ss << std::hex << hex.substr(2);
    ss >> vkCode;
    return vkCode;
}

void HandleKeyPress(int vkCode, bool isKeyDown, bool isKeyUp) {
    std::string status = "";
    std::string keyName = "";

    if (isKeyDown) {
        if (currentlyPressedKeys.find(vkCode) == currentlyPressedKeys.end()) {
            currentlyPressedKeys.insert(vkCode);
            std::string hexCode = VKCodeToHex(vkCode);
            currentlyPressedKeysHex.insert(hexCode);
            k += 1;
            status = "Down";
            keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "";
        }
    } else if (isKeyUp) {
        if (currentlyPressedKeys.find(vkCode) != currentlyPressedKeys.end()) {
            currentlyPressedKeys.erase(vkCode);
            std::string hexCode = VKCodeToHex(vkCode);
            currentlyPressedKeysHex.erase(hexCode);
            k -= 1;
            status = "Up";
            keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "";
        }
    }else {
        if (currentlyPressedKeys.find(vkCode) == currentlyPressedKeys.end()) {
            currentlyPressedKeys.insert(vkCode);
            std::string hexCode = VKCodeToHex(vkCode);
            currentlyPressedKeysHex.insert(hexCode);
            k+=1;
            status="Down";
            std::string keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "";
        }
    }
    std::string myString = std::to_string(k); 
    std::cout << "k=" << k << " key=" << keyName << " status=" << status << std::endl;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* keyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int vkCode = keyboard->vkCode;
        bool isKeyDown = (wParam == WM_KEYDOWN);
        bool isKeyUp = (wParam == WM_KEYUP);

        HandleKeyPress(vkCode, isKeyDown, isKeyUp);
        
        std::string currentCombination = GetCurrentCombination();
        std::string keyName = keyDictionary.count(vkCode) ? keyDictionary[vkCode] : "";
        
        if (currentCombination.empty()) {
            currentCombination = keyName;
        }

        bool isBlocked = IsHotkeyBlocked(currentCombination);
        std::string combinedKey = keyName + "+" + currentCombination;
        bool isCombinedBlocked = IsHotkeyBlocked(combinedKey);

        std::stringstream hexStream;
        hexStream << std::hex << std::uppercase << vkCode;
        std::cout << "\n\n{\"key_code\": \"0x" << hexStream.str() << "\""
                  << "\n\"current_combination\": \"" << currentCombination << "\","
                  << "\n\"combinationkey\": \"" << combinedKey << "\","
                  << "\n\"statusB\": \"" << (isBlocked ? "Blocked" : "Not blocked") << "\","
                  << "\n\"statusCB\": \"" << (isCombinedBlocked ? "Blocked" : "Not blocked") << "\","
                  << "\n\"event\": \"" << (isKeyDown ? "keydown" : "keyup") << "\"\n\n}" 
                  << std::endl;

        if (isBlocked || (isCombinedBlocked && !currentlyPressedKeys.empty())) {
            std::cout << "block " << currentCombination << std::endl;
            return 1;
        }
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
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string keyName = line.substr(1, pos - 2);
            std::string hexCode = line.substr(pos + 1);
            try {
                int keyCode = std::stoi(hexCode, nullptr, 16);
                keyDictionary[keyCode] = keyName;
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error parsing key code: " << hexCode << std::endl;
            }
        }
    }
    file.close();
}


void GeneratePermutations(const std::vector<std::string>& keys, std::vector<std::string>& results, int start) {
    if (start == keys.size() - 1) {
        std::string permutation;
        for (const auto& key : keys) {
            if (!permutation.empty()) {
                permutation += "+";
            }
            permutation += key; 
        }
        results.push_back(permutation);
        return;
    }

    for (int i = start; i < keys.size(); ++i) {
        std::vector<std::string> tempKeys = keys; 
        std::swap(tempKeys[start], tempKeys[i]); 
        GeneratePermutations(tempKeys, results, start + 1);
    }
}

void ParseArguments(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string hotkeyCombination = argv[i];

        std::istringstream ss(hotkeyCombination);
        std::vector<std::string> keys;
        std::string token;

        while (std::getline(ss, token, '+')) {
            keys.push_back(token); 
        }

        hotkeysToBlock.push_back({ hotkeyCombination, true });

        std::vector<std::string> permutations;
        GeneratePermutations(keys, permutations, 0);

        for (const auto& perm : permutations) {
            if (perm != hotkeyCombination) { 
                hotkeysToBlock.push_back({ perm, true });
            }
        }
    }

    for (const auto& hotkey : hotkeysToBlock) {
        std::cout << "Blocked hotkey: " << hotkey.combination << std::endl;
    }
}

void SetHook() {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
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
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Unhook();
    return 0;
}
