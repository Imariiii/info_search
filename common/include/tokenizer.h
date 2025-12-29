#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <vector>
#include <regex>

class Tokenizer {
private:
    std::regex word_pattern;
    std::string cleanToken(const std::string& token);
    std::string toLower(const std::string& str);

public:
    Tokenizer();
    std::vector<std::string> tokenize(const std::string& text);
};

#endif
