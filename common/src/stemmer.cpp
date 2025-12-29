#include "stemmer.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <map>

const std::vector<std::string> RussianStemmer::VOWELS = {
    "а", "е", "и", "о", "у", "ы", "э", "ю", "я", "ё"
};

const std::vector<std::string> RussianStemmer::PERFECTIVE_GERUND_1 = {
    "вшись", "вши", "в"
};

const std::vector<std::string> RussianStemmer::PERFECTIVE_GERUND_2 = {
    "ившись", "ывшись", "ивши", "ывши", "ив", "ыв"
};

const std::vector<std::string> RussianStemmer::ADJECTIVE = {
    "ими", "ыми", "его", "ого", "ему", "ому",
    "ее", "ие", "ые", "ое", "ей", "ий", "ый", "ой",
    "ем", "им", "ым", "ом", "их", "ых", "ую", "юю",
    "ая", "яя", "ою", "ею"
};

const std::vector<std::string> RussianStemmer::PARTICIPLE_1 = {
    "ющ", "вш", "ем", "нн", "щ"
};

const std::vector<std::string> RussianStemmer::PARTICIPLE_2 = {
    "ующ", "ивш", "ывш"
};

const std::vector<std::string> RussianStemmer::REFLEXIVE = {
    "ся", "сь"
};

const std::vector<std::string> RussianStemmer::VERB_1 = {
    "ете", "йте", "нно", "ешь", "ла", "на", "ли",
    "ло", "но", "ет", "ют", "ны", "ть", "й", "л", "н", "ем"
};

const std::vector<std::string> RussianStemmer::VERB_2 = {
    "ейте", "уйте", "ила", "ыла", "ена", "ите", "или", "ыли",
    "ило", "ыло", "ено", "ует", "уют", "ены", "ить", "ыть", "ишь",
    "ей", "уй", "ил", "ыл", "им", "ым", "ен", "ят", "ит", "ыт",
    "ую", "ю"
};

const std::vector<std::string> RussianStemmer::NOUN = {
    "иями", "ями", "ами", "ией", "ием", "иям", "иях",
    "ев", "ов", "ие", "ье", "еи", "ии", "ей", "ой", "ий",
    "ям", "ем", "ам", "ом", "ах", "ях", "ию", "ью", "ия", "ья",
    "а", "е", "и", "й", "о", "у", "ы", "ь", "ю", "я"
};

const std::vector<std::string> RussianStemmer::SUPERLATIVE = {
    "ейше", "ейш"
};

const std::vector<std::string> RussianStemmer::DERIVATIONAL = {
    "ость", "ост"
};

size_t RussianStemmer::utf8CharLen(unsigned char c) const {
    if ((c & 0x80) == 0) return 1;      
    if ((c & 0xE0) == 0xC0) return 2;   
    if ((c & 0xF0) == 0xE0) return 3;   
    if ((c & 0xF8) == 0xF0) return 4;   
    return 1;
}

std::string RussianStemmer::utf8CharAt(const std::string& str, size_t bytePos) const {
    if (bytePos >= str.length()) return "";
    size_t len = utf8CharLen(static_cast<unsigned char>(str[bytePos]));
    if (bytePos + len > str.length()) return "";
    return str.substr(bytePos, len);
}

size_t RussianStemmer::utf8Len(const std::string& str) const {
    size_t count = 0;
    for (size_t i = 0; i < str.length(); ) {
        i += utf8CharLen(static_cast<unsigned char>(str[i]));
        count++;
    }
    return count;
}

bool RussianStemmer::isVowel(const std::string& ch) const {
    for (const auto& v : VOWELS) {
        if (ch == v) return true;
    }
    return false;
}

size_t RussianStemmer::findRV(const std::string& word) const {
    for (size_t i = 0; i < word.length(); ) {
        std::string ch = utf8CharAt(word, i);
        size_t charLen = ch.length();
        if (isVowel(ch)) {
            return i + charLen;
        }
        i += charLen;
    }
    return word.length();
}

size_t RussianStemmer::findR1(const std::string& word) const {
    bool foundVowel = false;
    for (size_t i = 0; i < word.length(); ) {
        std::string ch = utf8CharAt(word, i);
        size_t charLen = ch.length();
        
        if (isVowel(ch)) {
            foundVowel = true;
        } else if (foundVowel) {
            return i + charLen;
        }
        i += charLen;
    }
    return word.length();
}

size_t RussianStemmer::findR2(const std::string& word, size_t r1) const {
    if (r1 >= word.length()) return word.length();
    
    std::string r1Part = word.substr(r1);
    size_t r2InR1 = findR1(r1Part);
    return r1 + r2InR1;
}

bool RussianStemmer::endsWith(const std::string& word, const std::string& suffix) const {
    if (suffix.length() > word.length()) return false;
    return word.compare(word.length() - suffix.length(), suffix.length(), suffix) == 0;
}

bool RussianStemmer::endsWithPreceded(const std::string& word, const std::string& suffix,
                                       const std::string& preceding) const {
    if (!endsWith(word, suffix)) return false;
    
    size_t pos = word.length() - suffix.length();
    if (pos < preceding.length()) return false;
    
    std::string precChar = "";
    for (size_t i = 0; i < pos; ) {
        size_t charLen = utf8CharLen(static_cast<unsigned char>(word[i]));
        if (i + charLen >= pos && i + charLen <= pos + preceding.length()) {
            precChar = word.substr(i, charLen);
        }
        if (i + charLen >= pos) break;
        i += charLen;
    }
    
    if (pos >= 2) {
        size_t checkPos = pos - 2;
        precChar = word.substr(checkPos, 2);
    }
    
    return (precChar == "а" || precChar == "я");
}

std::string RussianStemmer::removeSuffix(const std::string& word, const std::string& suffix) const {
    return word.substr(0, word.length() - suffix.length());
}

std::string RussianStemmer::stem(const std::string& word) const {
    if (word.empty()) return word;
    
    std::string result = word;
    size_t pos = 0;
    while ((pos = result.find("ё", pos)) != std::string::npos) {
        result.replace(pos, 2, "е");
        pos += 2;
    }
    
    size_t rv = findRV(result);
    size_t r1 = findR1(result);
    size_t r2 = findR2(result, r1);
    
    bool step1Done = false;
    
    for (const auto& suffix : PERFECTIVE_GERUND_2) {
        if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
            result = removeSuffix(result, suffix);
            step1Done = true;
            break;
        }
    }
    
    if (!step1Done) {
        for (const auto& suffix : PERFECTIVE_GERUND_1) {
            if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
                if (endsWithPreceded(result, suffix, "а") || 
                    endsWithPreceded(result, suffix, "я")) {
                    result = removeSuffix(result, suffix);
                    step1Done = true;
                    break;
                }
            }
        }
    }
    
    if (!step1Done) {
        for (const auto& suffix : REFLEXIVE) {
            if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
                result = removeSuffix(result, suffix);
                break;
            }
        }
        
        bool foundAdjectival = false;
        
        for (const auto& adj : ADJECTIVE) {
            if (endsWith(result, adj) && result.length() - adj.length() >= rv) {
                std::string withoutAdj = removeSuffix(result, adj);
                
                for (const auto& part : PARTICIPLE_2) {
                    if (endsWith(withoutAdj, part) && withoutAdj.length() - part.length() >= rv) {
                        result = removeSuffix(withoutAdj, part);
                        foundAdjectival = true;
                        break;
                    }
                }
                                
                if (!foundAdjectival) {
                    for (const auto& part : PARTICIPLE_1) {
                        if (endsWith(withoutAdj, part) && withoutAdj.length() - part.length() >= rv) {
                            if (endsWithPreceded(withoutAdj, part, "а") ||
                                endsWithPreceded(withoutAdj, part, "я")) {
                                result = removeSuffix(withoutAdj, part);
                                foundAdjectival = true;
                                break;
                            }
                        }
                    }
                }
                
                if (!foundAdjectival) {
                    result = withoutAdj;
                    foundAdjectival = true;
                }
                break;
            }
        }
        
        if (!foundAdjectival) {
            bool foundVerb = false;
            for (const auto& suffix : VERB_2) {
                if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
                    result = removeSuffix(result, suffix);
                    foundVerb = true;
                    break;
                }
            }
            
            if (!foundVerb) {
                for (const auto& suffix : VERB_1) {
                    if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
                        if (endsWithPreceded(result, suffix, "а") ||
                            endsWithPreceded(result, suffix, "я")) {
                            result = removeSuffix(result, suffix);
                            foundVerb = true;
                            break;
                        }
                    }
                }
            }
            
            if (!foundVerb) {
                for (const auto& suffix : NOUN) {
                    if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
                        result = removeSuffix(result, suffix);
                        break;
                    }
                }
            }
        }
    }
    
    if (endsWith(result, "и") && result.length() - 2 >= rv) {
        result = removeSuffix(result, "и");
    }
    
    for (const auto& suffix : DERIVATIONAL) {
        if (endsWith(result, suffix) && result.length() - suffix.length() >= r2) {
            result = removeSuffix(result, suffix);
            break;
        }
    }
    
    bool step4Done = false;
    
    for (const auto& suffix : SUPERLATIVE) {
        if (endsWith(result, suffix) && result.length() - suffix.length() >= rv) {
            result = removeSuffix(result, suffix);
            step4Done = true;
            break;
        }
    }
    
    if (endsWith(result, "нн")) {
        result = removeSuffix(result, "н");
    } else if (!step4Done && endsWith(result, "ь")) {
        result = removeSuffix(result, "ь");
    }
    
    return result;
}
