#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include "../common/include/stemmer.h"

struct StemStats {
    size_t totalTokens = 0;
    size_t uniqueBefore = 0;
    size_t uniqueAfter = 0;
    double executionTime = 0.0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <входной_файл> [выходной_файл]" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputFile = (argc > 2) ? argv[2] : "stemmed_tokens.txt";
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::ifstream inFile(inputFile);
    if (!inFile.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << inputFile << std::endl;
        return 1;
    }
    
    std::ofstream outFile(outputFile);
    if (!outFile.is_open()) {
        std::cerr << "Ошибка: не удалось создать файл " << outputFile << std::endl;
        return 1;
    }
    
    RussianStemmer stemmer;
    StemStats stats;
    std::map<std::string, size_t> beforeFreq;
    std::map<std::string, size_t> afterFreq;
    
    std::string token;
    while (std::getline(inFile, token)) {
        if (token.empty()) continue;
        
        stats.totalTokens++;
        beforeFreq[token]++;
        
        std::string stemmed = stemmer.stem(token);
        afterFreq[stemmed]++;
        
        outFile << stemmed << "\n";
        
        if (stats.totalTokens % 1000000 == 0) {
            std::cerr << "\rОбработано: " << stats.totalTokens / 1000000 << "M токенов..." << std::flush;
        }
    }
    std::cerr << std::endl;
    
    inFile.close();
    outFile.close();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    stats.uniqueBefore = beforeFreq.size();
    stats.uniqueAfter = afterFreq.size();
    stats.executionTime = duration.count() / 1000.0;
    
    std::cout << "Статистика стемминга:" << std::endl;
    std::cout << "  Всего токенов: " << stats.totalTokens << std::endl;
    std::cout << "  Уникальных до: " << stats.uniqueBefore << std::endl;
    std::cout << "  Уникальных после: " << stats.uniqueAfter << std::endl;
    std::cout << "  Время выполнения: " << stats.executionTime << " сек" << std::endl;
    
    std::string statsFile = outputFile + ".stats.json";
    std::ofstream statsOut(statsFile);
    if (statsOut.is_open()) {
        statsOut << "{\n";
        statsOut << "  \"total_tokens\": " << stats.totalTokens << ",\n";
        statsOut << "  \"unique_before\": " << stats.uniqueBefore << ",\n";
        statsOut << "  \"unique_after\": " << stats.uniqueAfter << ",\n";
        statsOut << "  \"execution_time_seconds\": " << stats.executionTime << "\n";
        statsOut << "}\n";
        statsOut.close();
    }
    
    return 0;
}
