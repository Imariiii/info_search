#ifndef RUSSIAN_STEMMER_H
#define RUSSIAN_STEMMER_H

#include <string>
#include <vector>

class RussianStemmer {
private:
    static const std::vector<std::string> VOWELS;
    
    static const std::vector<std::string> PERFECTIVE_GERUND_1;  
    static const std::vector<std::string> PERFECTIVE_GERUND_2;
    static const std::vector<std::string> ADJECTIVE;
    static const std::vector<std::string> PARTICIPLE_1;  
    static const std::vector<std::string> PARTICIPLE_2;
    static const std::vector<std::string> REFLEXIVE;
    static const std::vector<std::string> VERB_1;  
    static const std::vector<std::string> VERB_2;
    static const std::vector<std::string> NOUN;
    static const std::vector<std::string> SUPERLATIVE;
    static const std::vector<std::string> DERIVATIONAL;
    
    bool isVowel(const std::string& ch) const;
    size_t findRV(const std::string& word) const;
    size_t findR1(const std::string& word) const;
    size_t findR2(const std::string& word, size_t r1) const;
    
    bool endsWith(const std::string& word, const std::string& suffix) const;
    bool endsWithPreceded(const std::string& word, const std::string& suffix, 
                          const std::string& preceding) const;
    
    std::string removeSuffix(const std::string& word, const std::string& suffix) const;
    
    size_t utf8CharLen(unsigned char c) const;
    std::string utf8CharAt(const std::string& str, size_t bytePos) const;
    size_t utf8Len(const std::string& str) const;
    
public:
    RussianStemmer() = default;
    std::string stem(const std::string& word) const;
};

#endif 
