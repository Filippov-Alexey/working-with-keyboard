#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <regex>
std::map<std::string, int> keyDictionary;

// Функция для нажатия комбинации клавиш
void PressKeyCombination(const std::vector<int>& keyCodes) {
    for (const int& keyCode : keyCodes) {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = keyCode;
        SendInput(1, &input, sizeof(INPUT));
        std::cout << "Pressed key: " << keyCode << std::endl;
    }

    for (auto it = keyCodes.rbegin(); it != keyCodes.rend(); ++it) {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = *it;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
        std::cout << "Released key: " << *it << std::endl;
    }
}

// Функция для загрузки кодов клавиш из файла
void LoadKeyCodes(const std::string& fileName) {
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

// Функция для обработки аргументов, переданных программе
std::vector<int> ParseArguments(int argc, char* argv[], const std::map<std::string, int>& keyDictionary) {
    std::vector<int> keyCodes;

    for (int i = 1; i < argc; ++i) {
        std::string hotKey = argv[i];
        std::string cleanedKey = hotKey;
        
        // Удаляем кавычки, если они существуют
        if (cleanedKey.front() == '\'' && cleanedKey.back() == '\'') {
            cleanedKey = cleanedKey.substr(1, cleanedKey.size() - 2);
        }

        std::vector<std::string> components;
        std::string delimiter = "+";
        size_t pos = 0;
        
        // Разделяем ключи по знаку +
        while ((pos = cleanedKey.find(delimiter)) != std::string::npos) {
            components.push_back(cleanedKey.substr(0, pos));
            cleanedKey.erase(0, pos + delimiter.length());
        }
        components.push_back(cleanedKey); // Последний элемент

        // Получаем коды клавиш из словаря
        for (const auto& component : components) {
            auto it = keyDictionary.find(component);
            if (it != keyDictionary.end()) {
                keyCodes.push_back(it->second);
            } else {
                std::cerr << "Error: Invalid key \"" << component << "\"." << std::endl;
                return {};
            }
        }
    }

    return keyCodes;
}

// Главная функция
int main(int argc, char* argv[]) {

    // Загружаем коды клавиш из файла
    LoadKeyCodes("key_code.txt");

    // Проверка, были ли переданы аргументы
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " '<key1+key2>' '<key3+key4>' ..." << std::endl;
        return 1;
    }

    // Получаем коды клавиш из аргументов
    std::vector<int> keyCodes = ParseArguments(argc, argv, keyDictionary);
    if (keyCodes.empty()) {
        return 1; // Если не удалось получить коды клавиш
    }

    // Имитация нажатия клавиш
    PressKeyCombination(keyCodes);

    std::cout << "Done!" << std::endl;

    return 0;
}
