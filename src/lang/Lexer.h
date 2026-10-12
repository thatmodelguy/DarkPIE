#pragma once

#include "Token.h"

#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string src_;
    size_t i_ = 0;
    int line_ = 1;
    int column_ = 1;
    TokenKind lastKind_ = TokenKind::Eof;
    bool atLineStart_ = true;

    bool atEnd() const { return i_ >= src_.size(); }
    char peek(size_t off = 0) const;
    char advance();
    bool skipWhitespaceAndComments();
    Token make(TokenKind kind, std::string text, int line, int col) const;
    Token identifierOrKeyword();
    Token number();
    Token stringLiteral(char quote);
    Token tokenizeOne();
};
