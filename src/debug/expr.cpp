#include "debug/expr.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

namespace gen::debug {

namespace {

constexpr std::array<std::string_view, kRegZ80Count> kZ80Names{
    "a",  "f",  "b",  "c",  "d",  "e",  "h",  "l",   "i",   "r",   "af",
    "bc", "de", "hl", "ix", "iy", "sp", "pc", "af'", "bc'", "de'", "hl'"};

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::optional<int> registerId(std::string_view name, CpuId cpu) {
    const std::string n = lower(name);
    if (cpu == CpuId::Z80) {
        for (std::size_t i = 0; i < kZ80Names.size(); ++i) {
            if (n == kZ80Names[i]) {
                return kRegZ80 + static_cast<int>(i);
            }
        }
        return std::nullopt;
    }
    if (n.size() == 2 && (n[0] == 'd' || n[0] == 'a') && n[1] >= '0' && n[1] <= '7') {
        return (n[0] == 'd' ? 0 : 8) + (n[1] - '0');
    }
    if (n == "sp") {
        return 15;
    }
    if (n == "pc") {
        return kRegPc;
    }
    if (n == "sr") {
        return kRegSr;
    }
    if (n == "ccr") {
        return kRegCcr;
    }
    if (n == "usp") {
        return kRegUsp;
    }
    if (n == "ssp") {
        return kRegSsp;
    }
    return std::nullopt;
}

}  // namespace

class Parser {
public:
    Parser(std::string_view text, CpuId cpu, Expr& out) : text_(text), cpu_(cpu), out_(out) {}

    bool parse(std::string& error) {
        if (!logicalOr() || !expectEnd()) {
            error = error_ + " (at column " + std::to_string(errorAt_ + 1) + ")";
            return false;
        }
        return true;
    }

private:
    using Op = Expr::Op;

    void emit(Op op, std::int64_t imm = 0) {
        out_.code_.push_back({op, imm});
        switch (op) {
            case Op::Const:
            case Op::Reg:
            case Op::Var:
                ++depth_;
                out_.maxDepth_ = std::max(out_.maxDepth_, depth_);
                break;
            case Op::Read:
            case Op::Mask:
            case Op::Neg:
            case Op::Not:
            case Op::BitNot:
                break;
            default:
                --depth_;  // binary
                break;
        }
    }

    bool fail(const std::string& message) {
        if (error_.empty()) {
            error_ = message;
            errorAt_ = pos_;
        }
        return false;
    }

    void skipSpace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool accept(std::string_view token) {
        skipSpace();
        if (text_.substr(pos_).starts_with(token)) {
            // Do not split `<<` into `<`, `&&` into `&`, `==` into `=`, ...
            const std::size_t next = pos_ + token.size();
            if (token.size() == 1 && next < text_.size()) {
                const char a = token[0];
                const char b = text_[next];
                if ((a == '<' || a == '>') && (b == a || b == '=')) {
                    return false;
                }
                if ((a == '&' || a == '|') && b == a) {
                    return false;
                }
                if ((a == '!' || a == '=') && b == '=') {
                    return false;
                }
            }
            pos_ = next;
            return true;
        }
        return false;
    }

    bool expectEnd() {
        skipSpace();
        return pos_ == text_.size() || fail("unexpected '" + std::string(1, text_[pos_]) + "'");
    }

    template <typename Next>
    bool binary(Next next, std::initializer_list<std::pair<std::string_view, Op>> ops) {
        if (!(this->*next)()) {
            return false;
        }
        for (;;) {
            bool matched = false;
            for (const auto& [token, op] : ops) {
                if (accept(token)) {
                    if (!(this->*next)()) {
                        return false;
                    }
                    emit(op);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return true;
            }
        }
    }

    bool logicalOr() { return binary(&Parser::logicalAnd, {{"||", Op::LogOr}}); }
    bool logicalAnd() { return binary(&Parser::bitOr, {{"&&", Op::LogAnd}}); }
    bool bitOr() { return binary(&Parser::bitXor, {{"|", Op::Or}}); }
    bool bitXor() { return binary(&Parser::bitAnd, {{"^", Op::Xor}}); }
    bool bitAnd() { return binary(&Parser::equality, {{"&", Op::And}}); }
    bool equality() { return binary(&Parser::relational, {{"==", Op::Eq}, {"!=", Op::Ne}}); }
    bool relational() {
        return binary(&Parser::shift, {{"<=", Op::Le}, {">=", Op::Ge}, {"<", Op::Lt}, {">", Op::Gt}});
    }
    bool shift() { return binary(&Parser::additive, {{"<<", Op::Shl}, {">>", Op::Shr}}); }
    bool additive() { return binary(&Parser::multiplicative, {{"+", Op::Add}, {"-", Op::Sub}}); }
    bool multiplicative() { return binary(&Parser::unary, {{"*", Op::Mul}, {"/", Op::Div}, {"%", Op::Mod}}); }

    bool unary() {
        if (accept("-")) {
            if (!unary()) {
                return false;
            }
            emit(Op::Neg);
            return true;
        }
        if (accept("!")) {
            if (!unary()) {
                return false;
            }
            emit(Op::Not);
            return true;
        }
        if (accept("~")) {
            if (!unary()) {
                return false;
            }
            emit(Op::BitNot);
            return true;
        }
        if (accept("+")) {
            return unary();
        }
        return postfix();
    }

    // A `.b` / `.w` / `.l` size suffix after a primary.
    bool postfix() {
        if (!primary()) {
            return false;
        }
        if (pos_ + 1 < text_.size() && text_[pos_] == '.') {
            const char s = static_cast<char>(std::tolower(static_cast<unsigned char>(text_[pos_ + 1])));
            const bool endOfWord =
                pos_ + 2 >= text_.size() || std::isalnum(static_cast<unsigned char>(text_[pos_ + 2])) == 0;
            if (endOfWord && (s == 'b' || s == 'w' || s == 'l')) {
                pos_ += 2;
                emit(Op::Mask, s == 'b' ? 8 : s == 'w' ? 16 : 32);
            }
        }
        return true;
    }

    bool number() {
        int base = 10;
        if (text_[pos_] == '$') {
            base = 16;
            ++pos_;
        } else if (text_[pos_] == '%') {
            base = 2;
            ++pos_;
        } else if (text_.substr(pos_).starts_with("0x") || text_.substr(pos_).starts_with("0X")) {
            base = 16;
            pos_ += 2;
        }
        std::uint64_t value = 0;
        int digits = 0;
        while (pos_ < text_.size()) {
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(text_[pos_])));
            int d = -1;
            if (c >= '0' && c <= '9') {
                d = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                d = c - 'a' + 10;
            }
            if (d < 0 || d >= base) {
                break;
            }
            value = value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(d);
            ++digits;
            ++pos_;
        }
        if (digits == 0) {
            return fail("number expected");
        }
        emit(Op::Const, static_cast<std::int64_t>(value));
        return true;
    }

    bool primary() {
        skipSpace();
        if (pos_ >= text_.size()) {
            return fail("unexpected end of expression");
        }
        const char c = text_[pos_];
        if (c == '(') {
            ++pos_;
            if (!logicalOr()) {
                return false;
            }
            return accept(")") || fail("')' expected");
        }
        if (c == '$' || c == '%' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
            return number();
        }
        if (std::isalpha(static_cast<unsigned char>(c)) == 0 && c != '_') {
            return fail("unexpected '" + std::string(1, c) + "'");
        }
        const std::size_t start = pos_;
        while (pos_ < text_.size() &&
               (std::isalnum(static_cast<unsigned char>(text_[pos_])) != 0 || text_[pos_] == '_')) {
            ++pos_;
        }
        if (cpu_ == CpuId::Z80 && pos_ < text_.size() && text_[pos_] == '\'') {
            ++pos_;  // af' bc' de' hl'
        }
        const std::string word = lower(text_.substr(start, pos_ - start));
        if (word == "byte" || word == "word" || word == "long") {
            if (!accept("(")) {
                return fail("'(' expected after " + word);
            }
            if (!logicalOr()) {
                return false;
            }
            if (!accept(")")) {
                return fail("')' expected");
            }
            emit(Op::Read, word == "byte" ? 1 : word == "word" ? 2 : 4);
            return true;
        }
        if (word == "frame") {
            emit(Op::Var, kVarFrame);
            return true;
        }
        if (word == "line") {
            emit(Op::Var, kVarLine);
            return true;
        }
        if (word == "value") {
            emit(Op::Var, kVarValue);
            return true;
        }
        if (word == "addr") {
            emit(Op::Var, kVarAddr);
            return true;
        }
        if (word == "hits") {
            emit(Op::Var, kVarHits);
            return true;
        }
        if (const auto id = registerId(word, cpu_)) {
            emit(Op::Reg, *id);
            return true;
        }
        pos_ = start;
        return fail("unknown name '" + word + "'");
    }

    std::string_view text_;
    CpuId cpu_;
    Expr& out_;
    std::size_t pos_ = 0;
    int depth_ = 0;
    std::string error_;
    std::size_t errorAt_ = 0;
};

std::optional<Expr> Expr::compile(std::string_view text, CpuId cpu, std::string& error) {
    Expr e;
    e.text_ = std::string(text);
    Parser p(text, cpu, e);
    if (!p.parse(error)) {
        return std::nullopt;
    }
    return e;
}

std::int64_t Expr::eval(const EvalContext& ctx) const {
    if (code_.empty()) {
        return 1;
    }
    std::array<std::int64_t, 64> fixed{};
    std::vector<std::int64_t> heap;
    std::int64_t* stack = fixed.data();
    if (static_cast<std::size_t>(maxDepth_) > fixed.size()) {
        heap.resize(static_cast<std::size_t>(maxDepth_));
        stack = heap.data();
    }
    int sp = 0;
    for (const Instr& in : code_) {
        switch (in.op) {
            case Op::Const:
                stack[sp++] = in.imm;
                continue;
            case Op::Reg:
                stack[sp++] = ctx.reg(static_cast<int>(in.imm));
                continue;
            case Op::Var:
                stack[sp++] = ctx.variable(static_cast<int>(in.imm));
                continue;
            case Op::Read:
                stack[sp - 1] = ctx.read(static_cast<std::uint32_t>(stack[sp - 1]), static_cast<int>(in.imm));
                continue;
            case Op::Mask:
                stack[sp - 1] &= static_cast<std::int64_t>((std::uint64_t{1} << in.imm) - 1);
                continue;
            case Op::Neg:
                stack[sp - 1] = -stack[sp - 1];
                continue;
            case Op::Not:
                stack[sp - 1] = stack[sp - 1] == 0 ? 1 : 0;
                continue;
            case Op::BitNot:
                stack[sp - 1] = ~stack[sp - 1];
                continue;
            default:
                break;
        }
        const std::int64_t b = stack[--sp];
        std::int64_t& a = stack[sp - 1];
        switch (in.op) {
            case Op::Mul:
                a *= b;
                break;
            case Op::Div:
                a = b == 0 ? 0 : a / b;
                break;
            case Op::Mod:
                a = b == 0 ? 0 : a % b;
                break;
            case Op::Add:
                a += b;
                break;
            case Op::Sub:
                a -= b;
                break;
            case Op::Shl:
                a = b < 0 || b > 63 ? 0 : static_cast<std::int64_t>(static_cast<std::uint64_t>(a) << b);
                break;
            case Op::Shr:
                a = b < 0 || b > 63 ? 0 : static_cast<std::int64_t>(static_cast<std::uint64_t>(a) >> b);
                break;
            case Op::Lt:
                a = a < b ? 1 : 0;
                break;
            case Op::Le:
                a = a <= b ? 1 : 0;
                break;
            case Op::Gt:
                a = a > b ? 1 : 0;
                break;
            case Op::Ge:
                a = a >= b ? 1 : 0;
                break;
            case Op::Eq:
                a = a == b ? 1 : 0;
                break;
            case Op::Ne:
                a = a != b ? 1 : 0;
                break;
            case Op::And:
                a &= b;
                break;
            case Op::Xor:
                a ^= b;
                break;
            case Op::Or:
                a |= b;
                break;
            case Op::LogAnd:
                a = a != 0 && b != 0 ? 1 : 0;
                break;
            case Op::LogOr:
                a = a != 0 || b != 0 ? 1 : 0;
                break;
            default:
                break;
        }
    }
    return sp > 0 ? stack[sp - 1] : 0;
}

std::optional<Format> Format::compile(std::string_view text, CpuId cpu, std::string& error) {
    Format f;
    f.text_ = std::string(text);
    std::string literal;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '}' && i + 1 < text.size() && text[i + 1] == '}') {
            literal += '}';
            ++i;
            continue;
        }
        if (c != '{') {
            literal += c;
            continue;
        }
        if (i + 1 < text.size() && text[i + 1] == '{') {
            literal += '{';
            ++i;
            continue;
        }
        const std::size_t close = text.find('}', i);
        if (close == std::string_view::npos) {
            error = "'}' missing after column " + std::to_string(i + 1);
            return std::nullopt;
        }
        std::string_view field = text.substr(i + 1, close - i - 1);
        Part part;
        part.literal = std::move(literal);
        literal.clear();
        if (const std::size_t colon = field.rfind(':'); colon != std::string_view::npos) {
            const std::string_view spec = field.substr(colon + 1);
            if (spec.empty() || (spec[0] != 'x' && spec[0] != 'X' && spec[0] != 'd' && spec[0] != 'c')) {
                error = "unknown format '" + std::string(spec) + "' (use x, X, d or c)";
                return std::nullopt;
            }
            part.style = spec[0];
            for (const char d : spec.substr(1)) {
                if (d < '0' || d > '9') {
                    error = "bad digit count in '" + std::string(spec) + "'";
                    return std::nullopt;
                }
                part.digits = part.digits * 10 + (d - '0');
            }
            part.digits = std::min(part.digits, 16);
            field = field.substr(0, colon);
        }
        std::string exprError;
        part.expr = Expr::compile(field, cpu, exprError);
        if (!part.expr) {
            error = "in {" + std::string(field) + "}: " + exprError;
            return std::nullopt;
        }
        f.parts_.push_back(std::move(part));
        i = close;
    }
    if (!literal.empty()) {
        Part tail;
        tail.literal = std::move(literal);
        f.parts_.push_back(std::move(tail));
    }
    return f;
}

std::string Format::render(const EvalContext& ctx) const {
    std::string out;
    for (const Part& p : parts_) {
        out += p.literal;
        if (!p.expr) {
            continue;
        }
        const std::int64_t v = p.expr->eval(ctx);
        char buf[32];
        switch (p.style) {
            case 'x':
            case 'X':
                std::snprintf(buf, sizeof buf, p.style == 'x' ? "%0*llx" : "%0*llX", p.digits,
                              static_cast<unsigned long long>(v));
                break;
            case 'c':
                buf[0] = v >= 0x20 && v < 0x7F ? static_cast<char>(v) : '.';
                buf[1] = 0;
                break;
            default:
                std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
                break;
        }
        out += buf;
    }
    return out;
}

}  // namespace gen::debug
