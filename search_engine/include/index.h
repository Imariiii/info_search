#ifndef INDEX_H
#define INDEX_H

#include <string>
#include <utility>
#include "../../common/include/custom_containers.h"

struct TermEntry {
    std::string term;
    int docFreq;
    long long offset;
};

class InvertedIndex {
private:
    Custom::Vector<TermEntry> dictionary;
    Custom::Vector<Custom::Vector<int>> postings;

public:
    InvertedIndex();
    
    void build(Custom::Vector<std::pair<std::string, int>>& pairs);
    
    bool save(const std::string& filename) const;
    bool load(const std::string& filename);

    Custom::Vector<int> getPostings(const std::string& term) const;

    size_t getDictionarySize() const;
    size_t getPostingsCount() const;

    void printStats() const;
};

#endif
