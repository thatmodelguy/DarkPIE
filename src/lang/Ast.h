#pragma once

#include "Value.h"

#include <memory>
#include <string>
#include <vector>

enum class AssignOp { Equal, ColonEqual, Arrow, PlusEqual, MinusEqual, StarEqual, SlashEqual };

struct Expr {
    virtual ~Expr() = default;
    int line = 1;
    int column = 1;
    int id = 0;
};

struct Stmt {
    virtual ~Stmt() = default;
    int line = 1;
    int column = 1;
    int id = 0;
};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct LiteralExpr : Expr {
    Value value;
};

struct NameExpr : Expr {
    std::string name;
};

struct MemberExpr : Expr {
    ExprPtr object;
    std::string name;
};

struct UnaryExpr : Expr {
    std::string op;
    ExprPtr inner;
};

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
};

struct CallExpr : Expr {
    ExprPtr object;
    std::string name;
    std::vector<ExprPtr> args;
};

struct ArrayExpr : Expr {
    std::vector<ExprPtr> elements;
};

struct AssignStmt : Stmt {
    ExprPtr target;
    ExprPtr value;
    AssignOp op = AssignOp::Equal;
};

struct ExprStmt : Stmt {
    ExprPtr expr;
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
};

struct IfStmt : Stmt {
    ExprPtr condition;
    std::vector<StmtPtr> thenBody;
    std::vector<StmtPtr> elseBody;
};

struct WaitStmt : Stmt {
    ExprPtr duration;
    std::string unit;
};

struct IncrementStmt : Stmt {
    ExprPtr target;
    int delta = 1;
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct ForStmt : Stmt {
    StmtPtr init;
    ExprPtr variable;
    ExprPtr from;
    ExprPtr to;
    ExprPtr step;
    ExprPtr condition;
    StmtPtr increment;
    bool downto = false;
    std::vector<StmtPtr> body;
};

struct BetweenExpr : Expr {
    ExprPtr low;
    ExprPtr mid;
    ExprPtr high;
    std::string opLo;
    std::string opHi;
};

struct ParseError {
    int line = 1;
    int column = 1;
    int length = 1;
    std::string message;
};

struct Program {
    std::vector<StmtPtr> statements;
    std::string error;
    int errorLine = 0;
    std::vector<ParseError> errors;
};
