#pragma once

#include "Ast.h"
#include "Token.h"

#include <string>
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();
    Program parseAll();

private:
    std::vector<Token> tokens_;
    size_t i_ = 0;
    int nextId_ = 1;
    std::string error_;
    int errorLine_ = 0;
    std::vector<ParseError> errors_;

    const Token& peek(size_t off = 0) const;
    bool check(TokenKind k) const;
    bool match(TokenKind k);
    bool matchAny(std::initializer_list<TokenKind> kinds);
    Token consume();
    void fail(const std::string& msg);
    void synchronize();
    void fillProgram(Program& p);

    StmtPtr statement();
    StmtPtr ifStatement();
    StmtPtr waitStatement();
    StmtPtr whileStatement();
    StmtPtr forStatement();
    std::vector<StmtPtr> loopBody(int headerLine);
    std::vector<StmtPtr> braceBlock();
    ExprPtr expression();
    ExprPtr orExpr(bool equalIsComparison);
    ExprPtr andExpr(bool equalIsComparison);
    ExprPtr equality(bool equalIsComparison);
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unitary();
    ExprPtr unary();
    ExprPtr postfix();
    ExprPtr primary();
    ExprPtr applyUnits(ExprPtr inner);
    ExprPtr finishCall(std::string name, int line);
};
