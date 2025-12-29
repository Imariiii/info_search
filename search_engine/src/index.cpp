#include "index.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdint>

InvertedIndex::InvertedIndex() {}

template <typename T>
void quickSort(T* arr, long long low, long long high) {
    if (low >= high) return;

    struct Range { long long low, high; };
    Custom::Stack<Range> stack;
    stack.push({low, high});

    while (!stack.empty()) {
        Range range = stack.top();
        stack.pop();

        long long l = range.low;
        long long h = range.high;

        if (l >= h) continue;

        long long mid = l + (h - l) / 2;
        if (arr[mid] < arr[l]) std::swap(arr[mid], arr[l]);
        if (arr[h] < arr[l]) std::swap(arr[h], arr[l]);
        if (arr[h] < arr[mid]) std::swap(arr[h], arr[mid]);
        
        T pivot = arr[mid];
        
        long long i = l - 1;
        long long j = h + 1;
        while (true) {
            do { i++; } while (arr[i] < pivot);
            do { j--; } while (pivot < arr[j]);
            if (i >= j) break;
            std::swap(arr[i], arr[j]);
        }

        if (l < j) stack.push({l, j});
        if (j + 1 < h) stack.push({j + 1, h});
    }
}

void InvertedIndex::build(Custom::Vector<std::pair<std::string, int>>& pairs) {
    if (pairs.empty()) return;

    if (pairs.size() > 1) {
        quickSort(pairs.data_ptr(), 0, (long long)pairs.size() - 1);
    }

    dictionary.clear();
    postings.clear();

    std::string currentTerm = pairs[0].first;
    Custom::Vector<int> currentPostingList;
    currentPostingList.push_back(pairs[0].second);

    for (size_t i = 1; i < pairs.size(); ++i) {
        if (pairs[i].first == currentTerm) {
            if (pairs[i].second != currentPostingList.back()) {
                currentPostingList.push_back(pairs[i].second);
            }
        } else {
            TermEntry entry;
            entry.term = std::move(currentTerm);
            entry.docFreq = currentPostingList.size();
            entry.offset = postings.size();
            dictionary.push_back(std::move(entry));
            postings.push_back(std::move(currentPostingList));

            currentTerm = pairs[i].first;
            currentPostingList.clear();
            currentPostingList.push_back(pairs[i].second);
        }
    }

    TermEntry entry;
    entry.term = currentTerm;
    entry.docFreq = currentPostingList.size();
    entry.offset = postings.size();
    dictionary.push_back(std::move(entry));
    postings.push_back(std::move(currentPostingList));
}

size_t InvertedIndex::getDictionarySize() const {
    return dictionary.size();
}

size_t InvertedIndex::getPostingsCount() const {
    size_t count = 0;
    for (const auto& list : postings) {
        count += list.size();
    }
    return count;
}

bool InvertedIndex::save(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) return false;

    const char magic[] = "IDX1";
    out.write(magic, 4);

    uint32_t dictSize = dictionary.size();
    out.write(reinterpret_cast<const char*>(&dictSize), sizeof(dictSize));

    for (const auto& entry : dictionary) {
        uint32_t termLen = entry.term.length();
        out.write(reinterpret_cast<const char*>(&termLen), sizeof(termLen));
        out.write(entry.term.c_str(), termLen);
        uint32_t docFreq = entry.docFreq;
        out.write(reinterpret_cast<const char*>(&docFreq), sizeof(docFreq));

        const auto& list = postings[entry.offset];
        if (list.size() != docFreq) {
             std::cerr << "Error: Posting list size mismatch for term " << entry.term << std::endl;
             return false;
        }
        if (docFreq > 0) {
            out.write(reinterpret_cast<const char*>(list.data_ptr()), docFreq * sizeof(int));
        }
    }
    
    return true;
}

bool InvertedIndex::load(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) return false;

    dictionary.clear();
    postings.clear();

    char magic[5] = {0};
    in.read(magic, 4);
    if (std::string(magic) != "IDX1") return false;

    uint32_t dictSize = 0;
    in.read(reinterpret_cast<char*>(&dictSize), sizeof(dictSize));
    
    dictionary.reserve(dictSize);
    postings.reserve(dictSize);

    for (size_t i = 0; i < dictSize; ++i) {
        TermEntry entry;
        
        uint32_t termLen = 0;
        in.read(reinterpret_cast<char*>(&termLen), sizeof(termLen));
        
        std::string term(termLen, '\0');
        in.read(&term[0], termLen);
        entry.term = std::move(term);
        
        uint32_t docFreq = 0;
        in.read(reinterpret_cast<char*>(&docFreq), sizeof(docFreq));
        entry.docFreq = docFreq;

        Custom::Vector<int> list(docFreq);
        if (docFreq > 0) {
            in.read(reinterpret_cast<char*>(list.data_ptr()), docFreq * sizeof(int));
        }
        
        entry.offset = postings.size();
        postings.push_back(std::move(list));
        dictionary.push_back(std::move(entry));
    }

    return true;
}

Custom::Vector<int> InvertedIndex::getPostings(const std::string& term) const {
    if (dictionary.empty()) return {};

    int low = 0;
    int high = (int)dictionary.size() - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (dictionary[mid].term == term) {
            return postings[dictionary[mid].offset];
        }
        if (dictionary[mid].term < term) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    
    return {};
}

void InvertedIndex::printStats() const {
    std::cout << "Index Statistics:" << std::endl;
    std::cout << "  Dictionary Size: " << getDictionarySize() << " terms" << std::endl;
    std::cout << "  Total Postings: " << getPostingsCount() << std::endl;
}
