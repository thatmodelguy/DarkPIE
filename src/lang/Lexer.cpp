#include "Lexer.h"

#include <cctype>
#include <unordered_map>

namespace {
std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
}

Lexer::Lexer(std::string source) : src_(std::move(source)) {}

char Lexer::peek(size_t off) const {
    if (i_ + off >= src_.size()) return '\0';
    return src_[i_ + off];
}

char Lexer::advance() {
    char c = src_[i_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

Token Lexer::make(TokenKind kind, std::string text, int line, int col) const {
    Token t;
    t.kind = kind;
    t.text = std::move(text);
    t.line = line;
    t.column = col;
    return t;
}

bool Lexer::skipWhitespaceAndComments() {
    bool skipped = false;
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            skipped = true;
            advance();
            continue;
        }
        if (c == '\n') {
            skipped = true;
            advance();
            atLineStart_ = true;
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            skipped = true;
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            skipped = true;
            advance();
            advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (!atEnd()) {
                advance();
                advance();
            }
            continue;
        }
        if (c == '%' && atLineStart_) {
            skipped = true;
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }
        break;
    }
    return skipped;
}

Token Lexer::identifierOrKeyword() {
    int line = line_;
    int col = column_;
    std::string text;
    while (!atEnd()) {
        unsigned char c = static_cast<unsigned char>(peek());
        if (std::isalnum(c) || c == '_') {
            text.push_back(advance());
        } else {
            break;
        }
    }
    std::string lower = toLower(text);
    if (lower == "isn" && peek() == '\'' && (peek(1) == 't' || peek(1) == 'T')) {
        advance();
        advance();
        return make(TokenKind::NotEqual, "isn't", line, col);
    }
    if (lower == "rem") {
        while (!atEnd() && peek() != '\n') advance();
        skipWhitespaceAndComments();
        if (atEnd()) return make(TokenKind::Eof, "", line_, column_);
        return tokenizeOne();
    }
    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"if", TokenKind::If},
        {"then", TokenKind::Then},
        {"else", TokenKind::Else},
        {"elseif", TokenKind::ElseIf},
        {"elsif", TokenKind::ElseIf},
        {"end", TokenKind::EndKw},
        {"endif", TokenKind::EndIf},
        {"and", TokenKind::And},
        {"but", TokenKind::And},
        {"or", TokenKind::Or},
        {"xor", TokenKind::Xor},
        {"not", TokenKind::Not},
        {"mod", TokenKind::Mod},
        {"div", TokenKind::Div},
        {"shl", TokenKind::ShlOp},
        {"shr", TokenKind::ShrOp},
        {"times", TokenKind::Star},
        {"in", TokenKind::In},
        {"var", TokenKind::Var},
        {"wait", TokenKind::Wait},
        {"begin", TokenKind::Begin},
        {"while", TokenKind::While},
        {"do", TokenKind::Do},
        {"for", TokenKind::For},
        {"to", TokenKind::To},
        {"downto", TokenKind::DownTo},
        {"step", TokenKind::Step},
        {"neither", TokenKind::Neither},
        {"nor", TokenKind::Nor},
        {"true", TokenKind::True},
        {"false", TokenKind::False},
    };
    auto it = keywords.find(lower);
    if (it != keywords.end()) {
        // "Key.End" must be a property, not the end-of-block keyword.
        if (lastKind_ == TokenKind::Dot) return make(TokenKind::Identifier, lower, line, col);
        return make(it->second, lower, line, col);
    }
    Token t = make(TokenKind::Identifier, lower, line, col);
    return t;
}

Token Lexer::number() {
    int line = line_;
    int col = column_;
    std::string text;
    while (std::isdigit(static_cast<unsigned char>(peek()))) text.push_back(advance());
    if (peek() == '.') {
        text.push_back(advance());
        while (std::isdigit(static_cast<unsigned char>(peek()))) text.push_back(advance());
    }
    Token t = make(TokenKind::Number, text, line, col);
    t.number = std::stod(text);
    return t;
}

Token Lexer::stringLiteral(char quote) {
    int line = line_;
    int col = column_;
    advance();
    std::string text;
    while (!atEnd() && peek() != quote) {
        if (peek() == '\\') {
            advance();
            if (atEnd()) break;
            char e = advance();
            if (e == 'n') text.push_back('\n');
            else if (e == 't') text.push_back('\t');
            else text.push_back(e);
        } else {
            text.push_back(advance());
        }
    }
    if (peek() == quote) advance();
    return make(TokenKind::String, text, line, col);
}

Token Lexer::tokenizeOne() {
    bool skipped = skipWhitespaceAndComments();
    if (atEnd()) return make(TokenKind::Eof, "", line_, column_);
    atLineStart_ = false;
    int line = line_;
    int col = column_;
    char c = peek();
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return identifierOrKeyword();
    if (std::isdigit(static_cast<unsigned char>(c))) return number();
    // ".50" is a number after operators, but "Key.1" is member access (dot then 1).
    if (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))) &&
        lastKind_ != TokenKind::Identifier && lastKind_ != TokenKind::Var &&
        lastKind_ != TokenKind::True && lastKind_ != TokenKind::False &&
        lastKind_ != TokenKind::Number && lastKind_ != TokenKind::RParen) {
        return number();
    }
    if (c == '%') {
        advance();
        if (!skipped && lastKind_ == TokenKind::Number) return make(TokenKind::Percent, "%", line, col);
        return make(TokenKind::Mod, "%", line, col);
    }
    if (c == '"' || c == '`') return stringLiteral(c);
    if (c == '\'') {
        bool closed = false;
        for (size_t k = 1; i_ + k < src_.size() && src_[i_ + k] != '\n'; ++k) {
            if (src_[i_ + k] == '\'') {
                closed = true;
                break;
            }
        }
        if (closed) return stringLiteral('\'');
        while (!atEnd() && peek() != '\n') advance();
        return tokenizeOne();
    }

    if (c == '+' && peek(1) == '+') {
        advance();
        advance();
        return make(TokenKind::PlusPlus, "++", line, col);
    }
    if (c == '+' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::PlusEqual, "+=", line, col);
    }
    if (c == '-' && peek(1) == '-') {
        advance();
        advance();
        return make(TokenKind::MinusMinus, "--", line, col);
    }
    if (c == '-' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::MinusEqual, "-=", line, col);
    }
    if (c == '*' && peek(1) == '*') {
        advance();
        advance();
        return make(TokenKind::StarStar, "**", line, col);
    }
    if (c == '*' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::StarEqual, "*=", line, col);
    }
    if (c == '<' && peek(1) == '<') {
        advance();
        advance();
        return make(TokenKind::ShlOp, "<<", line, col);
    }
    if (c == '>' && peek(1) == '>') {
        advance();
        advance();
        return make(TokenKind::ShrOp, ">>", line, col);
    }
    if (c == '/' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::SlashEqual, "/=", line, col);
    }
    if (c == ':' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::ColonEqual, ":=", line, col);
    }
    if (c == '=' && peek(1) == '>') {
        advance();
        advance();
        return make(TokenKind::Arrow, "=>", line, col);
    }
    if (c == '=' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::EqualEqual, "==", line, col);
    }
    if (c == '<' && peek(1) == '>') {
        advance();
        advance();
        return make(TokenKind::NotEqual, "<>", line, col);
    }
    if (c == '!' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::NotEqual, "!=", line, col);
    }
    if (c == '~' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::ApproxEqual, "~=", line, col);
    }
    if (c == '<' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::LessEqual, "<=", line, col);
    }
    if (c == '>' && peek(1) == '=') {
        advance();
        advance();
        return make(TokenKind::GreaterEqual, ">=", line, col);
    }

    advance();
    switch (c) {
    case '=': return make(TokenKind::Equal, "=", line, col);
    case '+': return make(TokenKind::Plus, "+", line, col);
    case '-': return make(TokenKind::Minus, "-", line, col);
    case '*': return make(TokenKind::Star, "*", line, col);
    case '/': return make(TokenKind::Slash, "/", line, col);
    case '<': return make(TokenKind::Less, "<", line, col);
    case '>': return make(TokenKind::Greater, ">", line, col);
    case '.': return make(TokenKind::Dot, ".", line, col);
    case '(': return make(TokenKind::LParen, "(", line, col);
    case ')': return make(TokenKind::RParen, ")", line, col);
    case '{': return make(TokenKind::LBrace, "{", line, col);
    case '}': return make(TokenKind::RBrace, "}", line, col);
    case '[': return make(TokenKind::LBracket, "[", line, col);
    case ']': return make(TokenKind::RBracket, "]", line, col);
    case '|': return make(TokenKind::Pipe, "|", line, col);
    case '&': return make(TokenKind::Ampersand, "&", line, col);
    case '^': return make(TokenKind::Caret, "^", line, col);
    case ',': return make(TokenKind::Comma, ",", line, col);
    case ';': return make(TokenKind::Semicolon, ";", line, col);
    default: return make(TokenKind::Unknown, std::string(1, c), line, col);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    lastKind_ = TokenKind::Eof;
    atLineStart_ = true;
    while (true) {
        Token t = tokenizeOne();
        lastKind_ = t.kind;
        bool done = t.kind == TokenKind::Eof;
        tokens.push_back(std::move(t));
        if (done) break;
    }
    return tokens;
}
