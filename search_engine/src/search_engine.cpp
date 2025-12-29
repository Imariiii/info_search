#include "search_engine.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include "../../common/include/custom_containers.h"

SearchEngine::SearchEngine(std::shared_ptr<InvertedIndex> idx) : index(idx) {}

Custom::Vector<int> SearchEngine::search(const std::string& query, bool useStemming) {
    try {
        auto tokens = tokenizeQuery(query, useStemming);
        auto rpn = shuntingYard(tokens);
        auto result = executeRPN(rpn);
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Query Error: " << e.what() << std::endl;
        return {};
    }
}

Custom::Vector<QueryToken> SearchEngine::tokenizeQuery(const std::string& query, bool useStemming) {
    Custom::Vector<QueryToken> rawTokens;
    std::string current;
    
    auto addWord = [&](const std::string& word) {
        if (word.empty()) return;
        if (word == "AND") {
            rawTokens.push_back({TokenType::AND, "AND", 2});
        } else if (word == "OR") {
            rawTokens.push_back({TokenType::OR, "OR", 1});
        } else if (word == "NOT") {
            rawTokens.push_back({TokenType::NOT, "NOT", 3});
        } else if (word == "(") {
            rawTokens.push_back({TokenType::LPAREN, "(", 0});
        } else if (word == ")") {
            rawTokens.push_back({TokenType::RPAREN, ")", 0});
        } else {
            std::vector<std::string> subTokens = tokenizer.tokenize(word);
            for (const auto& t : subTokens) {
                std::string term = t;
                if (useStemming) {
                    term = stemmer.stem(t);
                }
                rawTokens.push_back({TokenType::TERM, term, 0});
            }
        }
    };

    for (size_t i = 0; i < query.length(); ++i) {
        char c = query[i];
        if (isspace(static_cast<unsigned char>(c))) {
            addWord(current);
            current.clear();
        } else if (c == '(' || c == ')') {
            addWord(current);
            current.clear();
            std::string paren(1, c);
            addWord(paren);
        } else {
            current += c;
        }
    }
    addWord(current);

    Custom::Vector<QueryToken> finalTokens;
    if (rawTokens.empty()) return finalTokens;

    finalTokens.push_back(rawTokens[0]);
    for (size_t i = 1; i < rawTokens.size(); ++i) {
        TokenType prev = rawTokens[i-1].type;
        TokenType curr = rawTokens[i].type;

        bool needsAnd = false;
        if (prev == TokenType::TERM && curr == TokenType::TERM) needsAnd = true;
        if (prev == TokenType::RPAREN && curr == TokenType::TERM) needsAnd = true;
        if (prev == TokenType::TERM && curr == TokenType::LPAREN) needsAnd = true;
        if (prev == TokenType::RPAREN && curr == TokenType::LPAREN) needsAnd = true;
        if (prev == TokenType::TERM && curr == TokenType::NOT) needsAnd = true;

        if (needsAnd) {
            finalTokens.push_back({TokenType::AND, "AND", 2});
        }
        finalTokens.push_back(rawTokens[i]);
    }

    return finalTokens;
}

Custom::Vector<QueryToken> SearchEngine::shuntingYard(const Custom::Vector<QueryToken>& tokens) {
    Custom::Vector<QueryToken> queue;
    Custom::Stack<QueryToken> stack;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        switch (token.type) {
            case TokenType::TERM:
                queue.push_back(token);
                break;
            case TokenType::AND:
            case TokenType::OR:
            case TokenType::NOT:
                while (!stack.empty() && stack.top().type != TokenType::LPAREN && 
                       stack.top().precedence >= token.precedence) {
                    queue.push_back(stack.top());
                    stack.pop();
                }
                stack.push(token);
                break;
            case TokenType::LPAREN:
                stack.push(token);
                break;
            case TokenType::RPAREN:
                while (!stack.empty() && stack.top().type != TokenType::LPAREN) {
                    queue.push_back(stack.top());
                    stack.pop();
                }
                if (!stack.empty()) stack.pop();
                break;
        }
    }
    while (!stack.empty()) {
        queue.push_back(stack.top());
        stack.pop();
    }
    return queue;
}

static Custom::Vector<int> intersection(const Custom::Vector<int>& a, const Custom::Vector<int>& b) {
    Custom::Vector<int> res;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) i++;
        else if (a[i] > b[j]) j++;
        else { res.push_back(a[i]); i++; j++; }
    }
    return res;
}

static Custom::Vector<int> union_set(const Custom::Vector<int>& a, const Custom::Vector<int>& b) {
    Custom::Vector<int> res;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) { res.push_back(a[i]); i++; }
        else if (a[i] > b[j]) { res.push_back(b[j]); j++; }
        else { res.push_back(a[i]); i++; j++; }
    }
    while (i < a.size()) res.push_back(a[i++]);
    while (j < b.size()) res.push_back(b[j++]);
    return res;
}

static Custom::Vector<int> difference(const Custom::Vector<int>& a, const Custom::Vector<int>& b) {
    Custom::Vector<int> res;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] < b[j]) { res.push_back(a[i]); i++; }
        else if (a[i] > b[j]) { j++; }
        else { i++; j++; }
    }
    while (i < a.size()) res.push_back(a[i++]);
    return res;
}

Custom::Vector<int> SearchEngine::executeRPN(const Custom::Vector<QueryToken>& rpn) {
    if (rpn.empty()) return {};

    Custom::Stack<SearchResult> results;

    for (size_t i = 0; i < rpn.size(); ++i) {
        const auto& token = rpn[i];
        if (token.type == TokenType::TERM) {
            results.push({index->getPostings(token.value), false});
        } else if (token.type == TokenType::NOT) {
             if (results.empty()) continue;
             SearchResult res = results.top(); results.pop();
             res.isNegated = !res.isNegated;
             results.push(res);
        } else {
            if (results.size() < 2) continue;
            SearchResult b = results.top(); results.pop();
            SearchResult a = results.top(); results.pop();
            
            SearchResult out;

            if (token.type == TokenType::AND) {
                if (!a.isNegated && !b.isNegated) {
                    out.docs = intersection(a.docs, b.docs);
                    out.isNegated = false;
                }
                else if (!a.isNegated && b.isNegated) {
                    out.docs = difference(a.docs, b.docs);
                    out.isNegated = false;
                }
                else if (a.isNegated && !b.isNegated) {
                    out.docs = difference(b.docs, a.docs);
                    out.isNegated = false;
                }
                else {
                    out.docs = union_set(a.docs, b.docs);
                    out.isNegated = true;
                }
            } else if (token.type == TokenType::OR) {
                if (!a.isNegated && !b.isNegated) {
                    out.docs = union_set(a.docs, b.docs);
                    out.isNegated = false;
                }
                else if (!a.isNegated && b.isNegated) {
                    out.docs = difference(b.docs, a.docs);
                    out.isNegated = true;
                }
                else if (a.isNegated && !b.isNegated) {
                    out.docs = difference(a.docs, b.docs);
                    out.isNegated = true;
                }
                else {
                    out.docs = intersection(a.docs, b.docs);
                    out.isNegated = true;
                }
            }
            results.push(std::move(out));
        }
    }
    
    if (results.empty()) return {};
    
    SearchResult finalRes = results.top();
    if (finalRes.isNegated) {
        std::cerr << "Warning: Query result is purely negative (NOT ...). Returning empty." << std::endl;
        return {};
    }
    return finalRes.docs;
}

Custom::Vector<int> SearchEngine::mergeAnd(const Custom::Vector<int>& a, const Custom::Vector<int>& b) { return {}; }
Custom::Vector<int> SearchEngine::mergeOr(const Custom::Vector<int>& a, const Custom::Vector<int>& b) { return {}; }
Custom::Vector<int> SearchEngine::mergeNot(const Custom::Vector<int>& a, const Custom::Vector<int>& b) { return {}; }
