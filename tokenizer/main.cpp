#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <map>
#include "../common/include/tokenizer.h"

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        return "";
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    return content;
}

void writeTokens(const std::vector<std::string>& tokens, 
                 const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось создать файл " << filename << std::endl;
        return;
    }
    
    for (const auto& token : tokens) {
        file << token << "\n";
    }
    
    file.close();
}

void writeTokensJSON(const std::vector<std::string>& tokens,
                    const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось создать файл " << filename << std::endl;
        return;
    }
    
    file << "[\n";
    for (size_t i = 0; i < tokens.size(); ++i) {
        file << "  \"" << tokens[i] << "\"";
        if (i < tokens.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    file << "]\n";
    
    file.close();
}

struct TokenStats {
    size_t total_tokens = 0;
    size_t total_length = 0;
    size_t min_length = SIZE_MAX;
    size_t max_length = 0;
    double average_length = 0.0;
    size_t unique_tokens = 0;
    std::map<std::string, size_t> token_frequencies;
    
    void addToken(const std::string& token) {
        total_tokens++;
        size_t len = token.length();
        total_length += len;
        
        if (len < min_length) {
            min_length = len;
        }
        if (len > max_length) {
            max_length = len;
        }
        
        token_frequencies[token]++;
    }
    
    void finalize() {
        if (total_tokens > 0) {
            average_length = static_cast<double>(total_length) / total_tokens;
        }
        unique_tokens = token_frequencies.size();
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <входной_файл> [выходной_файл] [--json]" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_file = (argc > 2) ? argv[2] : "tokens.txt";
    bool json_format = (argc > 3 && std::string(argv[3]) == "--json");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    std::string text = readFile(input_file);
    
    if (text.empty()) {
        std::cerr << "Ошибка: файл пуст или не удалось прочитать" << std::endl;
        return 1;
    }
    
    Tokenizer tokenizer;
    TokenStats stats;
    
    std::istringstream text_stream(text);
    std::string line;
    std::vector<std::string> all_tokens;
    
    while (std::getline(text_stream, line)) {
        if (line.empty()) continue;
        
        std::vector<std::string> tokens = tokenizer.tokenize(line);
        for (const auto& token : tokens) {
            stats.addToken(token);
            all_tokens.push_back(token);
        }
    }
    
    stats.finalize();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    double duration_sec = duration / 1000.0;
    
    if (json_format) {
        writeTokensJSON(all_tokens, output_file);
    } else {
        writeTokens(all_tokens, output_file);
    }
    
    size_t text_size_bytes = text.length();
    double text_size_kb = text_size_bytes / 1024.0;
    double kb_per_sec = (duration_sec > 0) ? (text_size_kb / duration_sec) : 0;
    double tokens_per_sec = (duration_sec > 0) ? (stats.total_tokens / duration_sec) : 0;
    
    std::cout << "Статистика токенизации:" << std::endl;
    std::cout << "  Количество токенов: " << stats.total_tokens << std::endl;
    std::cout << "  Уникальных токенов: " << stats.unique_tokens << std::endl;
    std::cout << "  Время выполнения: " << duration_sec << " сек" << std::endl;
    std::cout << "  Скорость: " << tokens_per_sec << " токенов/сек" << std::endl;
    std::cout << "  Скорость: " << kb_per_sec << " кб/сек" << std::endl;
    std::cout << "  Размер файла: " << text_size_kb << " кб" << std::endl;
    
    std::string stats_file = output_file + ".stats.json";
    std::ofstream stats_out(stats_file);
    if (stats_out.is_open()) {
        stats_out << "{\n";
        stats_out << "  \"total_tokens\": " << stats.total_tokens << ",\n";
        stats_out << "  \"unique_tokens\": " << stats.unique_tokens << ",\n";
        stats_out << "  \"execution_time_seconds\": " << duration_sec << "\n";
        stats_out << "}\n";
        stats_out.close();
    }
    
    return 0;
}
