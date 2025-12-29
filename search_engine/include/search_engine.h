#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include <memory>
#include <string>
#include "index.h"
#include "tokenizer.h"
#include "stemmer.h"
#include "../../common/include/custom_containers.h"

enum class TokenType {
    TERM,
    AND,
    OR,
    NOT,
    LPAREN,
    RPAREN
};

struct QueryToken {
    TokenType type;
    std::string value;
    int precedence;
};

struct SearchResult {
    Custom::Vector<int> docs;
    bool isNegated = false;
};

class SearchEngine {
public:
    SearchEngine(std::shared_ptr<InvertedIndex> idx);

    Custom::Vector<int> search(const std::string& query, bool useStemming);

private:
    std::shared_ptr<InvertedIndex> index;
    Tokenizer tokenizer;
    RussianStemmer stemmer;

    Custom::Vector<QueryToken> tokenizeQuery(const std::string& query, bool useStemming);

    Custom::Vector<QueryToken> shuntingYard(const Custom::Vector<QueryToken>& tokens);

    Custom::Vector<int> executeRPN(const Custom::Vector<QueryToken>& rpn);

    Custom::Vector<int> mergeAnd(const Custom::Vector<int>& a, const Custom::Vector<int>& b);
    Custom::Vector<int> mergeOr(const Custom::Vector<int>& a, const Custom::Vector<int>& b);
    Custom::Vector<int> mergeNot(const Custom::Vector<int>& a, const Custom::Vector<int>& b); // a AND NOT b
};

#endif
