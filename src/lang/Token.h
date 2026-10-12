#pragma once

#include <string>

enum class TokenKind {
    Eof,
    Identifier,
    Number,
    String,
    True,
    False,
    If,
    Then,
    Else,
    ElseIf,
    EndKw,
    EndIf,
    And,
    Or,
    Xor,
    Not,
    In,
    Mod,
    Percent,
    Div,
    Var,
    Wait,
    Begin,
    While,
    Do,
    For,
    To,
    DownTo,
    Step,
    Neither,
    Nor,
    ApproxEqual,
    Equal,       // =
    ColonEqual,  // :=
    Arrow,       // =>
    Plus,
    PlusPlus,
    Minus,
    MinusMinus,
    Star,
    Slash,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    NotEqual,    // <> or !=
    EqualEqual,
    Dot,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Pipe,
    Ampersand,
    Caret,
    StarStar,
    ShlOp,
    ShrOp,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    Comma,
    Semicolon,
    Unknown
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string text;
    double number = 0;
    int line = 1;
    int column = 1;
};

inline const char* tokenKindName(TokenKind k) {
    switch (k) {
    case TokenKind::Eof: return "end-of-file";
    case TokenKind::Identifier: return "identifier";
    case TokenKind::Number: return "number";
    case TokenKind::String: return "string";
    default: return "token";
    }
}
