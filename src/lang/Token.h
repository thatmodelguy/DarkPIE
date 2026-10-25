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
    case TokenKind::Eof: return "end of file";
    case TokenKind::Identifier: return "identifier";
    case TokenKind::Number: return "number";
    case TokenKind::String: return "string";
    case TokenKind::True: return "true";
    case TokenKind::False: return "false";
    case TokenKind::If: return "if";
    case TokenKind::Then: return "then";
    case TokenKind::Else: return "else";
    case TokenKind::ElseIf: return "elseif";
    case TokenKind::EndKw: return "end";
    case TokenKind::EndIf: return "endif";
    case TokenKind::And: return "and";
    case TokenKind::Or: return "or";
    case TokenKind::Xor: return "xor";
    case TokenKind::Not: return "not";
    case TokenKind::In: return "in";
    case TokenKind::Mod: return "mod";
    case TokenKind::Percent: return "%";
    case TokenKind::Div: return "div";
    case TokenKind::Var: return "var";
    case TokenKind::Wait: return "wait";
    case TokenKind::Begin: return "begin";
    case TokenKind::While: return "while";
    case TokenKind::Do: return "do";
    case TokenKind::For: return "for";
    case TokenKind::To: return "to";
    case TokenKind::DownTo: return "downto";
    case TokenKind::Step: return "step";
    case TokenKind::Neither: return "neither";
    case TokenKind::Nor: return "nor";
    case TokenKind::Equal: return "=";
    case TokenKind::ColonEqual: return ":=";
    case TokenKind::Arrow: return "=>";
    case TokenKind::Plus: return "+";
    case TokenKind::Minus: return "-";
    case TokenKind::Star: return "*";
    case TokenKind::Slash: return "/";
    case TokenKind::Less: return "<";
    case TokenKind::Greater: return ">";
    case TokenKind::LParen: return "(";
    case TokenKind::RParen: return ")";
    case TokenKind::LBrace: return "{";
    case TokenKind::RBrace: return "}";
    case TokenKind::LBracket: return "[";
    case TokenKind::RBracket: return "]";
    case TokenKind::Comma: return ",";
    case TokenKind::Semicolon: return ";";
    case TokenKind::Dot: return ".";
    case TokenKind::Unknown: return "ERROR!!";
    default: return "token";
    }
}

// GlovePIE-style "instead of …" text for the current scanner token.
inline std::string tokenDisplay(const Token& t) {
    if (t.kind == TokenKind::Eof) return "end of file";
    if (t.kind == TokenKind::String) return "\"" + t.text + "\"";
    if (!t.text.empty()) return "\"" + t.text + "\"";
    return std::string("\"") + tokenKindName(t.kind) + "\"";
}
