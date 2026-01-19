
#include <thread>
#include <chrono>
#include <vector>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <fstream>
#include <unordered_set>
#include <algorithm>

HHOOK keyboardHook;
bool isBlocked = false;

std::map<int, std::string> keyDictionary;
std::unordered_set<int> currentlyPressedKeys;     
struct Hotkey {
    std::string combination; 
    bool isBlocked;
};
std::vector<Hotkey> hotkeysToBlock;
std::string pressedKeys;

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

void ProcessKeyModifier(std::string& keyName, const char prefix, std::vector<std::string>& modifierKeys) {
    if (keyName.front() == prefix) {
        keyName.erase(0, 1); 
        if (std::find(modifierKeys.begin(), modifierKeys.end(), keyName) == modifierKeys.end()) {
            modifierKeys.push_back(keyName);
        }
    }
}

void SeparateModifiersAndNonModifiers(const std::string& keyname, const std::unordered_set<int>& vk_key, 
                                       std::vector<int>& vk, 
                                       std::unordered_set<int>& modifiers, 
                                       std::unordered_set<int>& nonModifiers) {
    bool keyFound = false;

    for (int key : vk_key) {
        std::string keyName = keyDictionary.count(key) ? keyDictionary.at(key) : "";
        
        if (!keyName.empty()) {
            if ((keyName.front() != '=') && (keyName.front() != '_') && (keyName.front() != '-')) {
                nonModifiers.insert(key);
            } else {
                modifiers.insert(key);
            }
        }
        
        if (keyName == keyname) {
            keyFound = true; 
        }
    }

    if (!keyFound) {
        int keyCode = std::find_if(keyDictionary.begin(), keyDictionary.end(),
        [&keyname](const auto& pair) { return pair.second == keyname; })->first;
        nonModifiers.insert(keyCode);
    }
    vk.insert(vk.end(), modifiers.begin(), modifiers.end());
    vk.insert(vk.end(), nonModifiers.begin(), nonModifiers.end());
}
void TrimOptions(std::vector<std::string>& options, const std::string& keyName) {
    options.erase(
        std::remove_if(options.begin(), options.end(),
                       [&keyName](const std::string& s) { return s.find(keyName) == std::string::npos; }),
        options.end()
    );
}

void AddOptions(const std::vector<std::string>& modifiers, const std::string& keyName, std::vector<std::string>& options) {
    for (const auto& modifier : modifiers) {
        options.push_back(modifier + "+" + keyName);
    }
    TrimOptions(options, keyName);
}

void AddComplexOptions(const std::vector<std::string>& firstModifier, const std::vector<std::string>& secondModifier, const std::string& keyName, std::vector<std::string>& options) {
    for (const auto& first : firstModifier) {
        for (const auto& second : secondModifier) {
            options.push_back(first + "+" + second + "+" + keyName);
        }
    }
    TrimOptions(options, keyName);
}

void AddComplexOptions1(const std::vector<std::string>& firstModifier, const std::vector<std::string>& secondModifier, const std::vector<std::string>& thirdModifier, const std::string& keyName, std::vector<std::string>& options) {
    for (const auto& first : firstModifier) {
        for (const auto& second : secondModifier) {
            for (const auto& third : thirdModifier) {
                options.push_back(first + "+" + second + "+" + third + "+" + keyName);
            }
        }
    }
    TrimOptions(options, keyName);
}
template<typename ContainerType>
void replace_all(ContainerType& container, typename ContainerType::value_type old_val, typename ContainerType::value_type new_val) {
    for(auto it = container.begin(); it != container.end(); ++it) {
        if(*it == old_val)
            *it = new_val;
    }
}

void ProcessKeyName(std::string& keyName,
                    std::unordered_set<int>& vk_key,
                    std::vector<std::string>& pressShift,
                    std::vector<std::string>& pressCtrl,
                    std::vector<std::string>& pressAlt,
                    std::string& pressedKeys,
                    std::vector<std::string>& options) {
    
    std::vector<int> vk; 
    std::unordered_set<int> modifiers; 
    std::unordered_set<int> nonModifiers;

    SeparateModifiersAndNonModifiers(keyName, vk_key, vk, modifiers, nonModifiers);

    const int left_alt = 0x12;
    const int alt = 0xa4;
    const int right_alt = 0xa5;
    if(std::find(vk.begin(), vk.end(), left_alt) != vk.end() &&
       std::find(vk.begin(), vk.end(), right_alt) != vk.end()) {
        replace_all(vk, left_alt, alt);
    }
    for (int key : vk) {
        std::string keyName = keyDictionary.count(key) ? keyDictionary.at(key) : "";
        
        if (!keyName.empty()) {
            ProcessKeyModifier(keyName, '=', pressShift);
            ProcessKeyModifier(keyName, '_', pressCtrl);
            ProcessKeyModifier(keyName, '-', pressAlt);
            
            bool hasShift = !pressShift.empty();
            bool hasCtrl = !pressCtrl.empty();
            bool hasAlt = !pressAlt.empty();
            
            if (hasShift && !hasCtrl && !hasAlt) {
                AddOptions(pressShift, keyName, options);
            } else if (!hasShift && hasCtrl && !hasAlt) {
                AddOptions(pressCtrl, keyName, options);
            } else if (!hasShift && !hasCtrl && hasAlt) {
                AddOptions(pressAlt, keyName, options);
            } else if (hasShift && hasCtrl && !hasAlt) {
                AddComplexOptions(pressShift, pressCtrl, keyName, options);
            } else if (hasShift && !hasCtrl && hasAlt) {
                AddComplexOptions(pressShift, pressAlt, keyName, options);
            } else if (!hasShift && hasCtrl && hasAlt) {
                AddComplexOptions(pressCtrl, pressAlt, keyName, options);
            } else if (hasShift && hasCtrl && hasAlt) {
                AddComplexOptions1(pressShift, pressCtrl, pressAlt, keyName, options);
            } else {
                if (pressedKeys!=keyName)
                options.push_back(keyName);
            }
        }
    }
}
void PrintKeyInfo(const std::string& hexCode, const std::string& keyName, 
                  const std::string& pressedKeys, 
                  const std::string& status, 
                  const std::vector<std::string>& options,
                  const std::string& block, const std::string& isInjected) {

    std::string optionsStr;
    for (const auto& option : options) {
        optionsStr += "\""+option+"\"" + ", ";
    }

    if (!optionsStr.empty()) {
        optionsStr = optionsStr.substr(0, optionsStr.size() - 2); 
    }

    std::cout 
              << "{\"key_code\": \"" << hexCode << "\", "
              << "\"key_name\": \"" << keyName << "\", "
              << "\"status\": \"" << status << "\", "
              << "\"pressed_keys\": \"" << pressedKeys << "\", "
              << "\"option\": [" << optionsStr << "], "
              << "\"blocked\": \"" << block << "\", "
              << "\"isInjected\": \""<< isInjected<<"\"}" 
              << std::endl;
}


void HandleKeyPress(int vkCode) {
    for (int i = 0; i < 256; ++i) {
        SHORT state = GetAsyncKeyState(i);
        
        std::string keyName = keyDictionary.count(i) ? keyDictionary[i] : std::to_string(i);
        
        bool isKeyDown = (state & 0x8000) != 0;
        bool isKeyUp = (state & 0x8000) == 0 && currentlyPressedKeys.find(i) != currentlyPressedKeys.end();

        if (isKeyDown) {
            if (currentlyPressedKeys.find(i) == currentlyPressedKeys.end()) {
                currentlyPressedKeys.insert(i);
            }
        }

        if (isKeyUp) {
            if (currentlyPressedKeys.find(i) != currentlyPressedKeys.end()){
                currentlyPressedKeys.erase(i);
            }
        }
    }
}

void ProcessOptions(std::vector<std::string>& options, const std::string& pressedKeys) {
    if (!pressedKeys.empty()) {
        options.push_back(pressedKeys);
    }
    if (options.empty()) return;

    auto itHasPlus = std::find_if(options.begin(), options.end(),
                                  [](const std::string& s){ return s.find('+') != std::string::npos; });

    if (itHasPlus != options.end()) {
        std::vector<std::string> filtered;
        for (const auto& s : options) {
            if (s.find('+') != std::string::npos) filtered.push_back(s);
        }
        options.swap(filtered);
        return;
    }

    std::sort(options.begin(), options.end()); 
    std::vector<std::string> permutations;
    do {
        std::string combined;
        for (size_t i = 0; i < options.size(); ++i) {
            if (i > 0) combined += "+";
            combined += options[i];
        }
        permutations.push_back(std::move(combined));
    } while (std::next_permutation(options.begin(), options.end()));

    options.swap(permutations);
}
LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam) {

    if (nCode != HC_ACTION) return CallNextHookEx(nullptr, nCode, wParam, lParam);
    KBDLLHOOKSTRUCT* pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;
    KBDLLHOOKSTRUCT* ks = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    if (!ks) return CallNextHookEx(nullptr, nCode, wParam, lParam);
    bool isInjected = pKeyStruct->flags & LLKHF_INJECTED;

    const int vk = static_cast<int>(ks->vkCode);
    const bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    static thread_local std::vector<std::string> pressShift;
    static thread_local std::vector<std::string> pressCtrl;
    static thread_local std::vector<std::string> pressAlt;
    static thread_local std::vector<std::string> options;
    pressShift.clear(); pressCtrl.clear(); pressAlt.clear(); options.clear();

    auto it = keyDictionary.find(vk);
    std::string keyName = (it != keyDictionary.end()) ? it->second : std::string();
    ProcessKeyModifier(keyName, '=', pressShift);
    ProcessKeyModifier(keyName, '_', pressCtrl);
    ProcessKeyModifier(keyName, '-', pressAlt);


    HandleKeyPress(vk);

    ProcessKeyName(keyName, currentlyPressedKeys, pressShift, pressCtrl, pressAlt, pressedKeys, options);
    ProcessOptions(options, pressedKeys);

    if (IsHotkeyBlocked(keyName)) {
        pressedKeys = keyName;
        options.clear();
        options.push_back(keyName);
    } else {
        for (const auto& opt : options) {
            if (IsHotkeyBlocked(opt)) { pressedKeys = opt; break; }
        }
    }

    if (!pressedKeys.empty() && isKeyUp) {
        if (keyName == pressedKeys) {
            pressedKeys.clear();
        } else {
            for (const auto& opt : options) {
                if (opt == pressedKeys) { pressedKeys.clear(); break; }
            }
        }
    }

    isBlocked = !pressedKeys.empty();

    PrintKeyInfo(VKCodeToHex(vk), keyName, pressedKeys, (isKeyDown ? "Down" : (isKeyUp ? "Up" : "None")), options, (isBlocked?"Blocked":"No blocked"), (isInjected?"Programm":"Physical"));

    if (isBlocked&&!isInjected) return 1;
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
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
int main(int argc, char* argv[]) {
    ParseArguments(argc, argv);
    ParseKeyCodeFile("key_code.txt");

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, NULL, 0);
    if (!keyboardHook) {
        std::cerr << "Failed to install hook!" << std::endl;
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);
    return 0;
}
