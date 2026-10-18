#include "Parser.h"

#include <cmath>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek(size_t off) const {
    size_t idx = i_ + off;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

bool Parser::check(TokenKind k) const { return peek().kind == k; }

bool Parser::match(TokenKind k) {
    if (!check(k)) return false;
    i_++;
    return true;
}

bool Parser::matchAny(std::initializer_list<TokenKind> kinds) {
    for (auto k : kinds) {
        if (check(k)) {
            i_++;
            return true;
        }
    }
    return false;
}

Token Parser::consume() {
    Token t = peek();
    if (t.kind != TokenKind::Eof) i_++;
    return t;
}

void Parser::fail(const std::string& msg) {
    errors_.push_back({peek().line, msg});
    if (error_.empty()) {
        error_ = msg;
        errorLine_ = peek().line;
    }
}

void Parser::synchronize() {
    if (check(TokenKind::Eof)) return;
    int line = peek().line;
    while (!check(TokenKind::Eof) && peek().line <= line) consume();
}

void Parser::fillProgram(Program& p) {
    p.errors = errors_;
    if (!errors_.empty()) {
        p.error = errors_[0].message;
        p.errorLine = errors_[0].line;
    } else {
        p.error = error_;
        p.errorLine = errorLine_;
        if (!error_.empty()) p.errors.push_back({errorLine_, error_});
    }
}

Program Parser::parse() {
    Program p;
    while (!check(TokenKind::Eof) && error_.empty()) {
        if (match(TokenKind::Semicolon)) continue;
        auto s = statement();
        if (s) p.statements.push_back(std::move(s));
        match(TokenKind::Semicolon);
        if (!error_.empty()) break;
    }
    fillProgram(p);
    return p;
}

Program Parser::parseAll() {
    Program p;
    while (!check(TokenKind::Eof)) {
        const size_t before = errors_.size();
        if (match(TokenKind::Semicolon)) continue;
        auto s = statement();
        if (errors_.size() > before) {
            synchronize();
            error_.clear();
            continue;
        }
        if (s) p.statements.push_back(std::move(s));
        match(TokenKind::Semicolon);
    }
    fillProgram(p);
    return p;
}

StmtPtr Parser::statement() {
    // GlovePIE ignores extra endif / end if / end at statement start.
    if (match(TokenKind::EndIf)) return nullptr;
    if (check(TokenKind::EndKw) && peek(1).kind != TokenKind::Equal &&
        peek(1).kind != TokenKind::ColonEqual && peek(1).kind != TokenKind::PlusEqual &&
        peek(1).kind != TokenKind::MinusEqual) {
        consume();
        match(TokenKind::If);
        match(TokenKind::While);
        match(TokenKind::For);
        return nullptr;
    }
    if (check(TokenKind::If)) return ifStatement();
    if (check(TokenKind::While)) return whileStatement();
    if (check(TokenKind::For)) return forStatement();
    if (check(TokenKind::Wait)) return waitStatement();
    if (match(TokenKind::Begin)) {
        auto block = std::make_unique<BlockStmt>();
        block->line = peek().line;
        block->id = nextId_++;
        while (!check(TokenKind::EndKw) && !check(TokenKind::Eof) && error_.empty()) {
            if (match(TokenKind::Semicolon)) continue;
            auto s = statement();
            if (s) block->statements.push_back(std::move(s));
        }
        match(TokenKind::EndKw);
        return block;
    }
    if (check(TokenKind::LBrace)) {
        auto block = std::make_unique<BlockStmt>();
        block->line = peek().line;
        block->id = nextId_++;
        block->statements = braceBlock();
        return block;
    }

    int line = peek().line;
    if (check(TokenKind::Identifier)) {
        std::string n = peek().text;
        auto isZeroArg = [&] {
            return n == "hidepie" || n == "showpie" || n == "exitpie" || n == "exitprogram" ||
                   n == "beepasterisk" || n == "beepexclamation" || n == "beephand" || n == "beepquestion" ||
                   n == "beepdefault" || n == "controlpanel" || n == "controlpanelkeyboard" ||
                   n == "controlpanelmouse" || n == "controlpanelmidi" || n == "controlpanelspeech" ||
                   n == "controlpanelppjoy" || n == "controlpaneljoystick" || n == "flashpiewindow" ||
                   n == "minimizepie" || n == "maximizepie" || n == "restorepie" || n == "unminimizepie" ||
                   n == "unmaximizepie";
        };
        auto isOneArg = [&] {
            return n == "press" || n == "release" || n == "type" || n == "say" || n == "swallow" ||
                   n == "beep" || n == "execute" || n == "debugprint" || n == "display" || n == "playsound" ||
                   n == "sendosc" || n == "broadcastosc" || n == "wiimotepoke" || n == "wiimotesend" ||
                   n == "typeunicode" || n == "addcode";
        };
        bool nextParen = peek(1).kind == TokenKind::LParen;
        bool nextAssign = peek(1).kind == TokenKind::Equal || peek(1).kind == TokenKind::ColonEqual ||
                          peek(1).kind == TokenKind::PlusEqual;
        if (!nextParen && !nextAssign && isZeroArg()) {
            consume();
            auto call = std::make_unique<CallExpr>();
            call->name = n;
            call->line = line;
            call->id = nextId_++;
            auto stmt = std::make_unique<ExprStmt>();
            stmt->line = line;
            stmt->id = nextId_++;
            stmt->expr = std::move(call);
            return stmt;
        }
        if (!nextParen && !nextAssign && isOneArg() && peek(1).kind != TokenKind::Eof && peek(1).line == line) {
            consume();
            auto call = std::make_unique<CallExpr>();
            call->name = n;
            call->line = line;
            call->id = nextId_++;
            call->args.push_back(expression());
            auto stmt = std::make_unique<ExprStmt>();
            stmt->line = line;
            stmt->id = nextId_++;
            stmt->expr = std::move(call);
            return stmt;
        }
    }
    if (check(TokenKind::EndKw) &&
        (peek(1).kind == TokenKind::Equal || peek(1).kind == TokenKind::ColonEqual ||
         peek(1).kind == TokenKind::PlusEqual || peek(1).kind == TokenKind::MinusEqual)) {
        consume();
        auto name = std::make_unique<NameExpr>();
        name->name = "end";
        name->line = line;
        auto stmt = std::make_unique<AssignStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->op = AssignOp::Equal;
        TokenKind k = peek().kind;
        consume();
        if (k == TokenKind::ColonEqual) stmt->op = AssignOp::ColonEqual;
        else if (k == TokenKind::PlusEqual) stmt->op = AssignOp::PlusEqual;
        else if (k == TokenKind::MinusEqual) stmt->op = AssignOp::MinusEqual;
        stmt->target = std::move(name);
        stmt->value = expression();
        return stmt;
    }
    if (match(TokenKind::PlusPlus) || match(TokenKind::MinusMinus)) {
        int d = tokens_[i_ - 1].kind == TokenKind::MinusMinus ? -1 : 1;
        auto stmt = std::make_unique<IncrementStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->delta = d;
        stmt->target = expression();
        return stmt;
    }
    ExprPtr expr = orExpr(false);
    if (!expr) return nullptr;
    if (match(TokenKind::PlusPlus) || match(TokenKind::MinusMinus)) {
        int d = tokens_[i_ - 1].kind == TokenKind::MinusMinus ? -1 : 1;
        auto stmt = std::make_unique<IncrementStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->delta = d;
        stmt->target = std::move(expr);
        return stmt;
    }

    if (match(TokenKind::PlusEqual) || match(TokenKind::MinusEqual) || match(TokenKind::StarEqual) ||
        match(TokenKind::SlashEqual)) {
        TokenKind k = tokens_[i_ - 1].kind;
        auto stmt = std::make_unique<AssignStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        if (k == TokenKind::PlusEqual) stmt->op = AssignOp::PlusEqual;
        else if (k == TokenKind::MinusEqual) stmt->op = AssignOp::MinusEqual;
        else if (k == TokenKind::StarEqual) stmt->op = AssignOp::StarEqual;
        else stmt->op = AssignOp::SlashEqual;
        stmt->target = std::move(expr);
        stmt->value = expression();
        return stmt;
    }

    if (match(TokenKind::Equal)) {
        auto stmt = std::make_unique<AssignStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->op = AssignOp::Equal;
        stmt->target = std::move(expr);
        stmt->value = expression();
        return stmt;
    }

    if (match(TokenKind::ColonEqual)) {
        auto stmt = std::make_unique<AssignStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->op = AssignOp::ColonEqual;
        stmt->target = std::move(expr);
        stmt->value = expression();
        return stmt;
    }
    if (match(TokenKind::Arrow)) {
        auto stmt = std::make_unique<AssignStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->op = AssignOp::Arrow;
        stmt->value = std::move(expr);
        stmt->target = expression();
        return stmt;
    }

    if (dynamic_cast<CallExpr*>(expr.get())) {
        auto stmt = std::make_unique<ExprStmt>();
        stmt->line = line;
        stmt->id = nextId_++;
        stmt->expr = std::move(expr);
        return stmt;
    }
    fail("unexpected text; expected an assignment, if, wait, or function call");
    return nullptr;
}

StmtPtr Parser::ifStatement() {
    auto stmt = std::make_unique<IfStmt>();
    stmt->line = peek().line;
    stmt->id = nextId_++;
    consume(); // if
    stmt->condition = expression();
    const int ifLine = stmt->line;
    bool hadThen = match(TokenKind::Then);
    int thenLine = hadThen ? tokens_[i_ - 1].line : ifLine;
    const bool oneLine = (peek().line == ifLine || (hadThen && peek().line == thenLine)) &&
                         peek().kind != TokenKind::Eof && peek().kind != TokenKind::Else &&
                         peek().kind != TokenKind::ElseIf && peek().kind != TokenKind::EndKw &&
                         peek().kind != TokenKind::EndIf && peek().kind != TokenKind::LBrace;
    if (oneLine) {
        auto s = statement();
        if (s) stmt->thenBody.push_back(std::move(s));
        if (match(TokenKind::Else) && peek().line == thenLine) {
            auto e = statement();
            if (e) stmt->elseBody.push_back(std::move(e));
        }
        if (match(TokenKind::EndIf)) return stmt;
        if (match(TokenKind::EndKw)) match(TokenKind::If);
        return stmt;
    }
    if (check(TokenKind::LBrace)) {
        stmt->thenBody = braceBlock();
        if (match(TokenKind::Else)) {
            if (check(TokenKind::LBrace)) stmt->elseBody = braceBlock();
            else if (check(TokenKind::If)) stmt->elseBody.push_back(ifStatement());
            else stmt->elseBody.push_back(statement());
        }
        return stmt;
    }
    auto atIfEnd = [&] {
        return check(TokenKind::Else) || check(TokenKind::ElseIf) || check(TokenKind::EndKw) ||
               check(TokenKind::EndIf) || check(TokenKind::Eof);
    };
    while (!atIfEnd() && error_.empty()) {
        if (match(TokenKind::Semicolon)) continue;
        auto s = statement();
        if (s) stmt->thenBody.push_back(std::move(s));
    }
    if (match(TokenKind::ElseIf)) {
        auto nested = std::make_unique<IfStmt>();
        nested->line = peek().line;
        nested->id = nextId_++;
        nested->condition = expression();
        match(TokenKind::Then);
        while (!atIfEnd() && error_.empty()) {
            if (match(TokenKind::Semicolon)) continue;
            auto s = statement();
            if (s) nested->thenBody.push_back(std::move(s));
        }
        if (match(TokenKind::Else)) {
            while (!check(TokenKind::EndKw) && !check(TokenKind::EndIf) && !check(TokenKind::Eof) &&
                   !check(TokenKind::If) && error_.empty()) {
                if (match(TokenKind::Semicolon)) continue;
                auto s = statement();
                if (s) nested->elseBody.push_back(std::move(s));
            }
        }
        stmt->elseBody.push_back(std::move(nested));
    } else if (match(TokenKind::Else)) {
        while (!check(TokenKind::EndKw) && !check(TokenKind::EndIf) && !check(TokenKind::Eof) &&
               !check(TokenKind::If) && error_.empty()) {
            if (match(TokenKind::Semicolon)) continue;
            auto s = statement();
            if (s) stmt->elseBody.push_back(std::move(s));
        }
    }
    if (match(TokenKind::EndIf)) return stmt;
    if (match(TokenKind::EndKw)) {
        match(TokenKind::If); // end if
    }
    return stmt;
}

std::vector<StmtPtr> Parser::loopBody(int headerLine) {
    match(TokenKind::Do);
    match(TokenKind::Then);
    if (check(TokenKind::LBrace)) return braceBlock();
    if (match(TokenKind::Begin)) {
        std::vector<StmtPtr> body;
        while (!check(TokenKind::EndKw) && !check(TokenKind::Eof) && error_.empty()) {
            if (match(TokenKind::Semicolon)) continue;
            auto s = statement();
            if (s) body.push_back(std::move(s));
        }
        match(TokenKind::EndKw);
        match(TokenKind::While);
        match(TokenKind::For);
        return body;
    }
    if (peek().kind != TokenKind::Eof && peek().line == headerLine && peek().kind != TokenKind::EndKw) {
        std::vector<StmtPtr> body;
        auto s = statement();
        if (s) body.push_back(std::move(s));
        return body;
    }
    std::vector<StmtPtr> body;
    while (!check(TokenKind::EndKw) && !check(TokenKind::EndIf) && !check(TokenKind::Eof) &&
           !check(TokenKind::Else) && error_.empty()) {
        if (match(TokenKind::Semicolon)) continue;
        auto s = statement();
        if (s) body.push_back(std::move(s));
    }
    match(TokenKind::EndKw);
    match(TokenKind::While);
    match(TokenKind::For);
    return body;
}

StmtPtr Parser::whileStatement() {
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = peek().line;
    stmt->id = nextId_++;
    consume();
    bool parens = match(TokenKind::LParen);
    stmt->condition = expression();
    if (parens) match(TokenKind::RParen);
    stmt->body = loopBody(stmt->line);
    return stmt;
}

StmtPtr Parser::forStatement() {
    auto stmt = std::make_unique<ForStmt>();
    stmt->line = peek().line;
    stmt->id = nextId_++;
    consume();
    if (match(TokenKind::LParen)) {
        if (!check(TokenKind::Semicolon)) stmt->init = statement();
        match(TokenKind::Semicolon);
        if (!check(TokenKind::Semicolon)) stmt->condition = expression();
        match(TokenKind::Semicolon);
        if (!check(TokenKind::RParen)) {
            auto inc = statement();
            stmt->increment = std::move(inc);
        }
        match(TokenKind::RParen);
        stmt->body = loopBody(stmt->line);
        return stmt;
    }
    stmt->variable = orExpr(false);
    match(TokenKind::Equal);
    match(TokenKind::ColonEqual);
    stmt->from = expression();
    stmt->downto = match(TokenKind::DownTo);
    if (!stmt->downto) match(TokenKind::To);
    stmt->to = expression();
    if (match(TokenKind::Step)) stmt->step = expression();
    stmt->body = loopBody(stmt->line);
    return stmt;
}

StmtPtr Parser::waitStatement() {
    auto stmt = std::make_unique<WaitStmt>();
    stmt->line = peek().line;
    stmt->id = nextId_++;
    consume();
    bool parens = match(TokenKind::LParen);
    stmt->duration = expression();
    if (parens) match(TokenKind::RParen);
    // "s" is seconds only on the same line as the time. The next line's "s=1"
    // is the S key, which is how the game scripts release movement.
    const int unitLine = stmt->duration ? stmt->duration->line : stmt->line;
    const bool unitIsAssignment =
        peek(1).kind == TokenKind::Equal || peek(1).kind == TokenKind::ColonEqual ||
        peek(1).kind == TokenKind::PlusEqual || peek(1).kind == TokenKind::MinusEqual;
    if (check(TokenKind::Identifier) && peek().line == unitLine && !unitIsAssignment) {
        const std::string& u = peek().text;
        if (u == "ms" || u == "millisecond" || u == "milliseconds" || u == "s" || u == "sec" ||
            u == "second" || u == "seconds" || u == "minute" || u == "minutes" || u == "hour" ||
            u == "hours" || u == "day" || u == "days") {
            stmt->unit = consume().text;
        }
    }
    return stmt;
}

std::vector<StmtPtr> Parser::braceBlock() {
    std::vector<StmtPtr> body;
    if (!match(TokenKind::LBrace)) {
        fail("expected '{'");
        return body;
    }
    while (!check(TokenKind::RBrace) && !check(TokenKind::Eof) && error_.empty()) {
        if (match(TokenKind::Semicolon)) continue;
        auto s = statement();
        if (s) body.push_back(std::move(s));
    }
    if (!match(TokenKind::RBrace)) fail("expected '}'");
    return body;
}

ExprPtr Parser::expression() { return orExpr(true); }

ExprPtr Parser::orExpr(bool equalIsComparison) {
    if (match(TokenKind::Neither)) {
        auto bin = std::make_unique<BinaryExpr>();
        bin->op = "neither";
        bin->left = andExpr(equalIsComparison);
        match(TokenKind::Nor);
        bin->right = andExpr(equalIsComparison);
        bin->line = peek().line;
        return bin;
    }
    auto left = andExpr(equalIsComparison);
    while (match(TokenKind::Or) || match(TokenKind::Xor)) {
        Token op = tokens_[i_ - 1];
        auto bin = std::make_unique<BinaryExpr>();
        bin->op = op.kind == TokenKind::Xor ? "xor" : "or";
        bin->left = std::move(left);
        bin->right = andExpr(equalIsComparison);
        bin->line = peek().line;
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::andExpr(bool equalIsComparison) {
    auto left = equality(equalIsComparison);
    while (match(TokenKind::And)) {
        auto bin = std::make_unique<BinaryExpr>();
        bin->op = "and";
        bin->left = std::move(left);
        bin->right = equality(equalIsComparison);
        bin->line = peek().line;
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::equality(bool equalIsComparison) {
    auto left = comparison();
    while (true) {
        if (match(TokenKind::ApproxEqual)) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = "~=";
            bin->left = std::move(left);
            bin->right = comparison();
            left = std::move(bin);
            continue;
        }
        if (check(TokenKind::Identifier) && peek().text == "is" && peek(1).kind == TokenKind::Not) {
            consume();
            consume();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = "<>";
            bin->left = std::move(left);
            bin->right = comparison();
            left = std::move(bin);
            continue;
        }
        if (matchAny({TokenKind::EqualEqual, TokenKind::NotEqual}) ||
            (equalIsComparison && match(TokenKind::Equal))) {
            Token op = tokens_[i_ - 1];
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = (op.kind == TokenKind::NotEqual) ? "<>" : "=";
            bin->left = std::move(left);
            bin->right = comparison();
            left = std::move(bin);
            continue;
        }
        break;
    }
    return left;
}

ExprPtr Parser::comparison() {
    auto left = term();
    if (!matchAny({TokenKind::Less, TokenKind::Greater, TokenKind::LessEqual, TokenKind::GreaterEqual}))
        return left;
    Token op = tokens_[i_ - 1];
    auto mid = term();
    if (matchAny({TokenKind::Less, TokenKind::Greater, TokenKind::LessEqual, TokenKind::GreaterEqual})) {
        Token op2 = tokens_[i_ - 1];
        auto between = std::make_unique<BetweenExpr>();
        between->line = op.line;
        between->low = std::move(left);
        between->mid = std::move(mid);
        between->high = term();
        between->opLo = op.text;
        between->opHi = op2.text;
        return between;
    }
    auto bin = std::make_unique<BinaryExpr>();
    bin->op = op.text;
    bin->left = std::move(left);
    bin->right = std::move(mid);
    return bin;
}

ExprPtr Parser::term() {
    auto left = unitary();
    while (matchAny({TokenKind::Plus, TokenKind::Minus})) {
        Token op = tokens_[i_ - 1];
        auto bin = std::make_unique<BinaryExpr>();
        bin->op = op.text;
        bin->left = std::move(left);
        bin->right = unitary();
        left = std::move(bin);
    }
    return left;
}

namespace {
bool isUnitWord(const std::string& u) {
    return u == "pixels" || u == "pixel" || u == "px" || u == "inches" || u == "inch" || u == "cm" ||
           u == "centimetres" || u == "centimeters" || u == "metres" || u == "meters" || u == "mickeys" ||
           u == "mickey" || u == "counts" || u == "seconds" || u == "second" || u == "sec" || u == "ms" ||
           u == "milliseconds" || u == "millisecond" || u == "minutes" || u == "minute" || u == "hours" ||
           u == "hour" || u == "days" || u == "day" || u == "hz" || u == "per";
}

std::string canonUnitPart(const std::string& u) {
    if (u == "pixels" || u == "pixel" || u == "px") return "px";
    if (u == "inches" || u == "inch") return "in";
    if (u == "cm" || u == "centimetres" || u == "centimeters") return "cm";
    if (u == "metres" || u == "meters") return "m";
    if (u == "mickeys" || u == "mickey" || u == "counts") return "mickeys";
    if (u == "seconds" || u == "second" || u == "sec") return "s";
    if (u == "ms" || u == "milliseconds" || u == "millisecond") return "s";
    if (u == "minutes" || u == "minute") return "min";
    if (u == "hours" || u == "hour") return "h";
    if (u == "hz") return "hz";
    return u;
}
}

ExprPtr Parser::applyUnits(ExprPtr inner) {
    while (inner) {
        if (match(TokenKind::In)) {
            std::string spec;
            while (check(TokenKind::Identifier) && isUnitWord(peek().text)) {
                std::string w = consume().text;
                if (w == "per") {
                    if (check(TokenKind::Identifier)) {
                        std::string part = canonUnitPart(consume().text);
                        if (!spec.empty() && spec.back() != '/') spec += "/";
                        spec += part;
                    }
                } else {
                    std::string part = canonUnitPart(w);
                    if (spec.empty()) spec = part;
                    else {
                        spec += "/";
                        spec += part;
                    }
                }
            }
            auto u = std::make_unique<UnaryExpr>();
            u->op = "in:" + spec;
            u->inner = std::move(inner);
            u->line = peek().line;
            inner = std::move(u);
            continue;
        }
        if (check(TokenKind::Identifier) && isUnitWord(peek().text) && peek().text != "per") {
            std::string spec = canonUnitPart(consume().text);
            while (check(TokenKind::Identifier) && isUnitWord(peek().text)) {
                std::string w = consume().text;
                if (w == "per" && check(TokenKind::Identifier)) {
                    spec += "/";
                    spec += canonUnitPart(consume().text);
                } else {
                    spec += "/";
                    spec += canonUnitPart(w);
                }
            }
            auto u = std::make_unique<UnaryExpr>();
            u->op = "as:" + spec;
            u->inner = std::move(inner);
            u->line = peek().line;
            inner = std::move(u);
            continue;
        }
        break;
    }
    return inner;
}

ExprPtr Parser::unitary() { return applyUnits(factor()); }

ExprPtr Parser::factor() {
    auto left = unary();
    while (matchAny({TokenKind::Star, TokenKind::Slash, TokenKind::Mod, TokenKind::Div, TokenKind::Ampersand,
                     TokenKind::Caret, TokenKind::StarStar, TokenKind::ShlOp, TokenKind::ShrOp})) {
        Token op = tokens_[i_ - 1];
        if (op.kind == TokenKind::Mod && check(TokenKind::Identifier) && peek().text == "of") {
            consume();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = "%of";
            bin->line = op.line;
            bin->left = std::move(left);
            bin->right = unary();
            left = std::move(bin);
            continue;
        }
        auto bin = std::make_unique<BinaryExpr>();
        if (op.kind == TokenKind::Mod) bin->op = "mod";
        else if (op.kind == TokenKind::Div) bin->op = "div";
        else if (op.kind == TokenKind::Ampersand) bin->op = "&";
        else if (op.kind == TokenKind::Caret || op.kind == TokenKind::StarStar) bin->op = "^";
        else if (op.kind == TokenKind::ShlOp) bin->op = "shl";
        else if (op.kind == TokenKind::ShrOp) bin->op = "shr";
        else if (op.kind == TokenKind::Star) bin->op = "*";
        else if (op.kind == TokenKind::Slash) bin->op = "/";
        else bin->op = op.text;
        bin->left = std::move(left);
        bin->right = unary();
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::unary() {
    if (match(TokenKind::Caret) && (check(TokenKind::Identifier) || check(TokenKind::Number))) {
        Token key = consume();
        auto name = std::make_unique<NameExpr>();
        name->line = key.line;
        std::string id = key.text;
        if (key.kind == TokenKind::Number && std::floor(key.number) == key.number)
            id = std::to_string(static_cast<long long>(key.number));
        name->name = "ctrl+" + id;
        return name;
    }
    if (match(TokenKind::Not) || match(TokenKind::Minus)) {
        Token op = tokens_[i_ - 1];
        auto u = std::make_unique<UnaryExpr>();
        u->op = op.text;
        u->inner = unary();
        u->line = op.line;
        return u;
    }
    if (match(TokenKind::Pipe)) {
        int line = tokens_[i_ - 1].line;
        auto u = std::make_unique<UnaryExpr>();
        u->op = "abs";
        u->inner = expression();
        u->line = line;
        if (!match(TokenKind::Pipe)) fail("expected '|'");
        return u;
    }
    return postfix();
}

ExprPtr Parser::postfix() {
    auto expr = primary();
    while (match(TokenKind::Dot)) {
        if (!check(TokenKind::Identifier) && !check(TokenKind::Number) && !check(TokenKind::True) &&
            !check(TokenKind::False) && !check(TokenKind::Var) && !check(TokenKind::Begin) &&
            !check(TokenKind::Wait) && !check(TokenKind::EndKw) && !check(TokenKind::To) &&
            !check(TokenKind::Mod) && !check(TokenKind::Div)) {
            fail("expected property name");
            return expr;
        }
        Token nameTok = consume();
        if (nameTok.kind == TokenKind::Number) {
            if (std::floor(nameTok.number) == nameTok.number)
                nameTok.text = std::to_string(static_cast<long long>(nameTok.number));
        }
        if (match(TokenKind::LParen)) {
            auto call = finishCall(nameTok.text, nameTok.line);
            static_cast<CallExpr*>(call.get())->object = std::move(expr);
            expr = std::move(call);
        } else {
            auto mem = std::make_unique<MemberExpr>();
            mem->line = nameTok.line;
            mem->object = std::move(expr);
            mem->name = nameTok.text;
            expr = std::move(mem);
        }
    }
    if (match(TokenKind::Percent)) {
        if (check(TokenKind::Identifier) && peek().text == "of") {
            consume();
            auto bin = std::make_unique<BinaryExpr>();
            bin->op = "%of";
            bin->line = expr->line;
            bin->left = std::move(expr);
            bin->right = unary();
            expr = std::move(bin);
        } else {
            auto u = std::make_unique<UnaryExpr>();
            u->op = "%";
            u->line = expr->line;
            u->inner = std::move(expr);
            expr = std::move(u);
        }
    }
    return expr;
}

ExprPtr Parser::finishCall(std::string name, int line) {
    auto call = std::make_unique<CallExpr>();
    call->name = std::move(name);
    call->line = line;
    call->id = nextId_++;
    if (!match(TokenKind::RParen)) {
        do {
            call->args.push_back(expression());
        } while (match(TokenKind::Comma));
        if (!match(TokenKind::RParen)) fail("expected ')'");
    }
    return call;
}

ExprPtr Parser::primary() {
    int line = peek().line;
    if (match(TokenKind::True)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->line = line;
        lit->value = Value::boolean(true);
        return lit;
    }
    if (match(TokenKind::False)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->line = line;
        lit->value = Value::boolean(false);
        return lit;
    }
    if (check(TokenKind::Number)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->line = line;
        lit->id = nextId_++;
        double n = consume().number;
        if (check(TokenKind::Identifier)) {
            std::string unit = peek().text;
            auto isTimeOrHz = [](const std::string& u) {
                return u == "s" || u == "hz" || u == "khz" || u == "ms" || u == "millisecond" ||
                       u == "milliseconds" || u == "sec" || u == "second" || u == "seconds" ||
                       u == "minute" || u == "minutes" || u == "hour" || u == "hours" || u == "day" ||
                       u == "days";
            };
            // "s=0" is Key.S. "0.5s" / KeepDown(..., 0.5s) is seconds.
            bool sLooksLikeKey =
                (unit == "s" || unit == "sec" || unit == "second" || unit == "seconds") &&
                (i_ + 1 < tokens_.size()) &&
                (tokens_[i_ + 1].kind == TokenKind::Equal || tokens_[i_ + 1].kind == TokenKind::ColonEqual ||
                 tokens_[i_ + 1].kind == TokenKind::PlusEqual || tokens_[i_ + 1].kind == TokenKind::MinusEqual);
            if (isTimeOrHz(unit) && !sLooksLikeKey) {
                consume();
                std::string u = "s";
                if (unit == "ms" || unit == "millisecond" || unit == "milliseconds") n /= 1000.0;
                else if (unit == "minute" || unit == "minutes") n *= 60.0;
                else if (unit == "hour" || unit == "hours") n *= 3600.0;
                else if (unit == "day" || unit == "days") n *= 86400.0;
                else if (unit == "khz") {
                    n *= 1000.0;
                    u = "hz";
                } else if (unit == "hz")
                    u = "hz";
                lit->value = Value::number(n, u);
                return lit;
            }
        }
        lit->value = Value::number(n);
        return lit;
    }
    if (check(TokenKind::String)) {
        auto lit = std::make_unique<LiteralExpr>();
        lit->line = line;
        lit->value = Value::string(consume().text);
        return lit;
    }
    if (match(TokenKind::Var)) {
        auto name = std::make_unique<NameExpr>();
        name->name = "var";
        name->line = line;
        if (match(TokenKind::Dot) && check(TokenKind::Identifier)) {
            auto mem = std::make_unique<MemberExpr>();
            mem->line = line;
            mem->object = std::move(name);
            mem->name = consume().text;
            return mem;
        }
        return name;
    }
    if (check(TokenKind::Identifier)) {
        Token id = consume();
        if (match(TokenKind::LParen)) return finishCall(id.text, id.line);
        auto name = std::make_unique<NameExpr>();
        name->name = id.text;
        name->line = id.line;
        return name;
    }
    if (match(TokenKind::LParen)) {
        auto inner = expression();
        if (!match(TokenKind::RParen)) fail("expected ')'");
        return inner;
    }
    if (match(TokenKind::LBracket)) {
        auto arr = std::make_unique<ArrayExpr>();
        arr->line = line;
        arr->id = nextId_++;
        if (!check(TokenKind::RBracket)) {
            do {
                arr->elements.push_back(expression());
            } while (match(TokenKind::Comma));
        }
        if (!match(TokenKind::RBracket)) fail("expected ']'");
        return arr;
    }
    fail("unexpected token");
    return nullptr;
}
