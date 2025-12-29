#include "tokenizer.h"
#include <algorithm>
#include <cctype>

Tokenizer::Tokenizer() {
    word_pattern = std::regex(
        R"(\S+)",
        std::regex_constants::ECMAScript | std::regex_constants::optimize
    );
}

std::string Tokenizer::cleanToken(const std::string& token) {
    if (token.empty()) return "";
    
    std::string cleaned = token;
    size_t start = 0;
    size_t end = cleaned.length();
    
    while (start < end && 
            (static_cast<unsigned char>(cleaned[start]) < 128 && 
            std::ispunct(static_cast<unsigned char>(cleaned[start])) && 
            cleaned[start] != '-')) {
        start++;
    }
    
    while (end > start && 
            (static_cast<unsigned char>(cleaned[end-1]) < 128 && 
            std::ispunct(static_cast<unsigned char>(cleaned[end-1])) && 
            cleaned[end-1] != '-')) {
        end--;
    }
    
    return cleaned.substr(start, end - start);
}

std::string Tokenizer::toLower(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ) {
        unsigned char uc = static_cast<unsigned char>(str[i]);   
        if (uc < 128) {
            if (uc >= 'A' && uc <= 'Z') {
                result += static_cast<char>(uc + ('a' - 'A'));
            } else {
                result += str[i];
            }
            ++i;
        } 
        else if ((uc & 0xE0) == 0xC0 && i + 1 < str.length()) {
            unsigned char uc2 = static_cast<unsigned char>(str[i + 1]);
            if ((uc2 & 0xC0) == 0x80) {
                unsigned int codepoint = ((uc & 0x1F) << 6) | (uc2 & 0x3F);     
                if (codepoint >= 0x0410 && codepoint <= 0x042F) {
                    codepoint += 0x20; 
                    result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }      
                else if (codepoint == 0x0401) {
                    codepoint = 0x0451;
                    result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                    result += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                else {
                    result += str[i];
                    result += str[i + 1];
                }
                i += 2;
            } else {
                result += str[i];
                ++i;
            }
        }
        else if ((uc & 0xF0) == 0xE0 && i + 2 < str.length()) {
            result += str[i];
            result += str[i + 1];
            result += str[i + 2];
            i += 3;
        }
        else if ((uc & 0xF8) == 0xF0 && i + 3 < str.length()) {
            result += str[i];
            result += str[i + 1];
            result += str[i + 2];
            result += str[i + 3];
            i += 4;
        }
        else {
            result += str[i];
            ++i;
        }
    }
    return result;
}

std::vector<std::string> Tokenizer::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    if (text.empty()) return tokens;
    
    std::sregex_iterator iter(text.begin(), text.end(), word_pattern);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::string token = iter->str();
        token = cleanToken(token);
        token = toLower(token);
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}
