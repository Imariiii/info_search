#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>

#include "tokenizer.h"
#include "stemmer.h"
#include "index.h"
#include "search_engine.h"
#include "../../common/include/custom_containers.h"

namespace fs = std::filesystem;

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: \n"
                  << "  Build:  " << argv[0] << " build <input_source> <index_file> [--stemming]\n"
                  << "  Search: " << argv[0] << " search <index_file> [urls_file] [--stemming]" << std::endl;
        return 1;
    }

    std::string mode = argv[1];

    try {
        if (mode == "build") {
            if (argc < 4) {
                std::cerr << "Usage: " << argv[0] << " build <input_source> <index_file> [--stemming]" << std::endl;
                return 1;
            }
            std::string inputPath = argv[2];
            std::string indexPath = argv[3];
            bool useStemming = false;
            if (argc > 4 && std::string(argv[argc-1]) == "--stemming") useStemming = true;

            std::cout << "Building Boolean Index..." << std::endl;
            std::cout << "Input: " << inputPath << std::endl;
            std::cout << "Stemming: " << (useStemming ? "ON" : "OFF") << std::endl;

            Tokenizer tokenizer;
            RussianStemmer stemmer;
            Custom::Vector<std::pair<std::string, int>> allPairs;
            int docCount = 0;
            
            auto startTotal = std::chrono::high_resolution_clock::now();

            if (fs::exists(inputPath) && fs::is_directory(inputPath)) {
                for (const auto& entry : fs::directory_iterator(inputPath)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                        docCount++;
                        int docId = docCount; 
                        std::string content = readFile(entry.path().string());
                        std::vector<std::string> tokens = tokenizer.tokenize(content);
                        for (size_t i = 0; i < tokens.size(); ++i) {
                            std::string term = tokens[i];
                            if (useStemming) term = stemmer.stem(tokens[i]);
                            allPairs.push_back({term, docId});
                        }
                    }
                }
            } else if (fs::exists(inputPath) && fs::is_regular_file(inputPath)) {
                std::cout << "Processing single file line-by-line (each line = 1 doc)..." << std::endl;
                std::ifstream file(inputPath);
                if (file.is_open()) {
                    std::string line;
                    while (std::getline(file, line)) {
                        if (line.empty()) continue;
                        docCount++;
                        int docId = docCount;
                        std::vector<std::string> tokens = tokenizer.tokenize(line);
                        for (size_t i = 0; i < tokens.size(); ++i) {
                            std::string term = tokens[i];
                            if (useStemming) term = stemmer.stem(tokens[i]);
                            allPairs.push_back({term, docId});
                        }
                        if (docCount % 1000 == 0) std::cout << "\rRead " << docCount << " documents..." << std::flush;
                    }
                    std::cout << std::endl;
                }
            }

            std::cout << "Collected " << allPairs.size() << " pairs from " << docCount << " documents. Sorting and building..." << std::endl;

            InvertedIndex index;
            index.build(allPairs);
            
            if (index.save(indexPath)) {
                 std::cout << "Index saved to " << indexPath << std::endl;
            } else {
                 std::cerr << "Error saving index to " << indexPath << std::endl;
                 return 1;
            }

            auto endTotal = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> totalTime = endTotal - startTotal;
            std::cout << "Total Time: " << totalTime.count() << "s" << std::endl;
            index.printStats();

        } else if (mode == "search") {
            if (argc < 3) {
                std::cerr << "Usage: " << argv[0] << " search <index_file> [urls_file] [--stemming]" << std::endl;
                return 1;
            }
            std::string indexPath = argv[2];
            std::string urlsPath = "";
            bool useStemming = false;

            for (int i = 3; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--stemming") {
                    useStemming = true;
                } else {
                    urlsPath = arg;
                }
            }

            std::vector<std::string> docUrls;
            if (!urlsPath.empty()) {
                std::cout << "Loading URLs from " << urlsPath << "..." << std::endl;
                std::ifstream urlFile(urlsPath);
                if (urlFile.is_open()) {
                    std::string line;
                    while (std::getline(urlFile, line)) {
                        docUrls.push_back(line);
                    }
                    std::cout << "Loaded " << docUrls.size() << " URLs." << std::endl;
                } else {
                    std::cerr << "Warning: Could not open URLs file " << urlsPath << std::endl;
                }
            }

            std::cout << "Loading Index from " << indexPath << "..." << std::endl;
            auto index = std::make_shared<InvertedIndex>();
            
            auto startLoad = std::chrono::high_resolution_clock::now();
            if (!index->load(indexPath)) {
                std::cerr << "Error loading index!" << std::endl;
                return 1;
            }
            auto endLoad = std::chrono::high_resolution_clock::now();
            std::cout << "Index loaded in " << std::chrono::duration<double>(endLoad - startLoad).count() << "s." << std::endl;
            index->printStats();

            SearchEngine engine(index);
            std::cout << "\nEnter boolean queries (e.g., 'term1 AND term2'). Type 'exit' to quit." << std::endl;
            
            std::string line;
            while (true) {
                std::cout << "> ";
                if (!std::getline(std::cin, line) || line == "exit") break;
                if (line.empty()) continue;

                auto startQ = std::chrono::high_resolution_clock::now();
                Custom::Vector<int> results = engine.search(line, useStemming);
                auto endQ = std::chrono::high_resolution_clock::now();
                
                std::cout << "Found " << results.size() << " documents in " 
                          << std::chrono::duration<double, std::milli>(endQ - startQ).count() << "ms." << std::endl;
                
                if (!results.empty()) {
                    if (results.size() <= 10) {
                        std::cout << "Results (all " << results.size() << "):" << std::endl;
                        for (size_t i = 0; i < results.size(); ++i) {
                            int docId = results[i];
                            std::cout << "  [" << docId << "]";
                            if (!docUrls.empty() && docId > 0 && (size_t)docId <= docUrls.size()) {
                                std::cout << " " << docUrls[docId - 1];
                            }
                            std::cout << std::endl;
                        }
                    } else {
                        std::cout << "Results (first 5 and last 5):" << std::endl;
                        for (size_t i = 0; i < 5; ++i) {
                            int docId = results[i];
                            std::cout << "  [" << docId << "]";
                            if (!docUrls.empty() && docId > 0 && (size_t)docId <= docUrls.size()) {
                                std::cout << " " << docUrls[docId - 1];
                            }
                            std::cout << std::endl;
                        }
                        std::cout << "  ..." << std::endl;
                        for (size_t i = results.size() - 5; i < results.size(); ++i) {
                            int docId = results[i];
                            std::cout << "  [" << docId << "]";
                            if (!docUrls.empty() && docId > 0 && (size_t)docId <= docUrls.size()) {
                                std::cout << " " << docUrls[docId - 1];
                            }
                            std::cout << std::endl;
                        }
                    }
                }
            }
        } else {
            std::cerr << "Unknown mode: " << mode << std::endl;
            return 1;
        }
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory Error: Out of memory!" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
