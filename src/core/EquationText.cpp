#include "EquationText.h"

#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace phy104::EquationText {
namespace {

const std::set<std::string> kFunctions = {"sin", "cos", "tan", "sec", "csc", "cot", "asin", "acos", "atan", "sqrt", "abs", "ln", "log",
                                           "sinh", "cosh", "tanh", "exp", "floor", "ceil", "round", "sgn"};
const std::set<std::string> kGreek = {"alpha", "beta", "gamma", "delta", "epsilon", "theta", "lambda", "mu", "pi", "phi", "rho",
                                       "sigma", "tau", "omega", "nu", "eta", "kappa", "psi", "zeta", "xi", "chi", "Delta", "Omega", "Sigma", "Phi", "Theta", "Lambda", "Gamma", "Xi", "Psi", "Pi"};

bool isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

// =============================================================================== LaTeX -> expression
// The converter produces a list of items; adjacent operands are joined with an implicit '*'.
struct Item { std::string text; bool operandEnd = false; bool operandStart = false; };

class FromLatex {
public:
    explicit FromLatex(const std::string& s) : m_s(s) {}

    std::string run() {
        const std::string out = sequence(false);
        if (m_i < m_s.size()) throw std::runtime_error("Unexpected '}' in the LaTeX");
        return out;
    }

private:
    const std::string& m_s;
    std::size_t m_i = 0;

    bool atEnd() const { return m_i >= m_s.size(); }
    void skipSpaces() { while (!atEnd() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i; }

    static std::string join(const std::vector<Item>& items) {
        std::string out;
        for (std::size_t k = 0; k < items.size(); ++k) {
            if (k > 0 && items[k - 1].operandEnd && items[k].operandStart) out += "*";
            out += items[k].text;
        }
        return out;
    }

    std::string group() {                                   // {...}
        skipSpaces();
        if (atEnd()) throw std::runtime_error("Missing argument in the LaTeX");
        if (m_s[m_i] != '{') return atom().text;            // single token argument: x^2, \frac12 is not supported
        ++m_i;
        const std::string inner = sequence(true);
        if (atEnd() || m_s[m_i] != '}') throw std::runtime_error("Missing '}' in the LaTeX");
        ++m_i;
        return inner;
    }

    std::string commandName() {
        std::size_t start = m_i;
        while (!atEnd() && isAlpha(m_s[m_i])) ++m_i;
        if (m_i == start && !atEnd()) ++m_i;
        return m_s.substr(start, m_i - start);
    }

    /// One operand: number, identifier (with subscript / prime), \command, group or bracket.
    Item atom() {
        skipSpaces();
        if (atEnd()) throw std::runtime_error("Unexpected end of the LaTeX");
        const char c = m_s[m_i];
        if (isDigit(c) || c == '.') {
            std::size_t start = m_i;
            while (!atEnd() && (isDigit(m_s[m_i]) || m_s[m_i] == '.')) ++m_i;
            return {m_s.substr(start, m_i - start), true, true};
        }
        if (isAlpha(c)) { ++m_i; return identifier(std::string(1, c)); }
        if (c == '{') {
            std::string grouped{"("};
            grouped += group();
            grouped += ')';
            return {std::move(grouped), true, true};
        }
        if (c == '\\') { ++m_i; return command(); }
        throw std::runtime_error(std::string("Unexpected '") + c + "' in the LaTeX");
    }

    Item identifier(std::string name) {                     // after the base letters: _sub and primes
        for (;;) {
            if (!atEnd() && m_s[m_i] == '_') {
                ++m_i;
                std::string sub = group();
                std::string flat;
                for (char ch : sub) if (std::isalnum(static_cast<unsigned char>(ch))) flat += ch;
                name += "_" + flat;
            } else if (!atEnd() && m_s[m_i] == '\'') {
                ++m_i;
                name += "p";                                // s' -> sp
            } else break;
        }
        return {name, true, true};
    }

    Item command() {
        const std::string name = commandName();
        if (name == "frac" || name == "dfrac") {
            const std::string a = group(), b = group();
            return {"((" + a + ")/(" + b + "))", true, true};
        }
        if (name == "sqrt") {
            skipSpaces();
            if (!atEnd() && m_s[m_i] == '[') {                       // \sqrt[n]{x} = x^(1/n)
                ++m_i;
                std::string n;
                while (!atEnd() && m_s[m_i] != ']') n += m_s[m_i++];
                if (atEnd()) throw std::runtime_error("Missing ']' in the LaTeX");
                ++m_i;
                return {"((" + group() + ")^(1/" + n + "))", true, true};
            }
            return {"sqrt(" + group() + ")", true, true};
        }
        if (name == "left" || name == "right") {
            skipSpaces();
            if (atEnd()) throw std::runtime_error("Missing bracket after \\" + name);
            const char b = m_s[m_i++];
            const bool open = name == "left";
            if (b == '(' || b == '[') return {"(", !open, open};
            if (b == ')' || b == ']') return {")", true, false};
            if (b == '.') return {"", false, false};
            throw std::runtime_error("Unsupported bracket after \\" + name);
        }
        if (name == "cdot" || name == "times") return {"*", false, false};
        if (name == "div") return {"/", false, false};
        if (name == "operatorname") {                                  // \operatorname{sinh} -> the function name
            const std::string fn = group();
            if (!kFunctions.count(fn)) throw std::runtime_error("Unsupported function " + fn);
            return {fn, false, true};
        }
        if (name == "," || name == ";" || name == " " || name == "!" || name == "quad" || name == "qquad") return {"", false, false};
        if (name == "pi") return {"pi", true, true};
        if (name == "circ") return {"", false, false};
        if (kGreek.count(name)) return identifier(name);
        if (name == "arcsin" || name == "arccos" || name == "arctan") return {"a" + name.substr(3), false, true};
        if (kFunctions.count(name)) return {name, false, true};
        throw std::runtime_error("Unsupported LaTeX command \\" + name);
    }

    std::string sequence(bool untilBrace) {
        std::vector<Item> items;
        for (;;) {
            skipSpaces();
            if (atEnd()) break;
            if (m_s[m_i] == '}') { if (untilBrace) break; throw std::runtime_error("Unexpected '}' in the LaTeX"); }
            const char c = m_s[m_i];
            if (c == '+' || c == '-' || c == '=' || c == '/' || c == '*') { items.push_back({std::string(1, c), false, false}); ++m_i; continue; }
            if (c == '(' || c == '[') { items.push_back({"(", false, true}); ++m_i; continue; }
            if (c == ')' || c == ']') { items.push_back({")", true, false}); ++m_i; continue; }
            if (c == '^') {                                                   // exponent applies to the previous operand
                ++m_i;
                if (items.empty()) throw std::runtime_error("'^' needs something before it");
                skipSpaces();
                if (m_s.compare(m_i, 8, "{\\circ}") == 0 || m_s.compare(m_i, 5, "\\circ") == 0) {   // 30^{\circ} = 30 degrees
                    m_i += m_s.compare(m_i, 8, "{\\circ}") == 0 ? 8 : 5;
                    items.push_back({"*(pi/180)", true, false});
                    continue;
                }
                const std::string e = group();
                items.push_back({"^(" + e + ")", true, false});
                continue;
            }
            if (c == '_') throw std::runtime_error("Subscript '_' must follow a letter");
            Item a = atom();
            // a function with an exponent: \sin^2\theta -> (sin(theta))^(2)
            if (kFunctions.count(a.text) && !a.operandEnd) {
                skipSpaces();
                std::string power;
                if (!atEnd() && m_s[m_i] == '^') { ++m_i; power = group(); }
                skipSpaces();
                std::string arg;
                if (!atEnd() && (m_s[m_i] == '(' || (m_s[m_i] == '\\' && m_s.compare(m_i, 5, "\\left") == 0))) {
                    // bracketed argument: let the main loop read it, wrap afterwards
                    std::size_t depth = 0, begin = m_i;
                    std::vector<Item> inner;
                    do {
                        skipSpaces();
                        if (m_s.compare(m_i, 5, "\\left") == 0) { m_i += 5; skipSpaces(); }
                        if (m_s.compare(m_i, 6, "\\right") == 0) { m_i += 6; skipSpaces(); }
                        const char ch = m_s[m_i];
                        if (ch == '(') ++depth;
                        if (ch == ')') --depth;
                        ++m_i;
                    } while (depth > 0 && !atEnd());
                    if (depth != 0) throw std::runtime_error("Unbalanced brackets in the LaTeX");
                    std::string raw = m_s.substr(begin, m_i - begin);
                    std::string cleaned;
                    for (std::size_t k = 0; k < raw.size();) {
                        if (raw.compare(k, 5, "\\left") == 0) k += 5;
                        else if (raw.compare(k, 6, "\\right") == 0) k += 6;
                        else cleaned += raw[k++];
                    }
                    FromLatex sub(cleaned);
                    arg = sub.run();                                          // includes the brackets
                } else {
                    Item operand = atom();
                    arg = "(" + operand.text + ")";
                }
                const std::string call = a.text + arg;
                items.push_back({power.empty() ? call : "(" + call + ")^(" + power + ")", true, true});
                continue;
            }
            items.push_back(a);
        }
        return join(items);
    }
};

// =============================================================================== expression -> LaTeX
struct Piece { std::string latex; bool simple = false; };         // simple: a single symbol / number (no brackets needed)

class ToLatex {
public:
    explicit ToLatex(const std::string& s) : m_s(s) {}

    std::string run() {
        std::string out = expr();
        skip();
        if (m_i < m_s.size() && m_s[m_i] == '=') {
            ++m_i;
            out += " = " + expr();
            skip();
        }
        if (m_i < m_s.size()) throw std::runtime_error(std::string("Unexpected '") + m_s[m_i] + "'");
        return out;
    }

private:
    const std::string& m_s;
    std::size_t m_i = 0;

    void skip() { while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i; }
    bool accept(char c) { skip(); if (m_i < m_s.size() && m_s[m_i] == c) { ++m_i; return true; } return false; }

    std::string expr() {
        std::string out = term().latex;
        for (;;) {
            if (accept('+')) out += " + " + term().latex;
            else if (accept('-')) out += " - " + term().latex;
            else return out;
        }
    }

    Piece term() {
        Piece acc = factor();
        for (;;) {
            if (accept('*')) {
                Piece f = factor();
                const bool numbers = isDigit(acc.latex.back()) && isDigit(f.latex.front());
                acc = {acc.latex + (numbers ? " \\times " : " ") + f.latex, false};
            } else if (accept('/')) {
                Piece f = factor();
                acc = {"\\frac{" + acc.latex + "}{" + f.latex + "}", false};
            } else return acc;
        }
    }

    Piece factor() {
        if (accept('-')) { Piece p = factor(); return {"-" + p.latex, false}; }
        if (accept('+')) return factor();
        return power();
    }

    Piece power() {
        Piece base = primary();
        if (accept('^')) {
            Piece e = factor();
            return {(base.simple ? base.latex : "\\left(" + base.latex + "\\right)") + "^{" + e.latex + "}", false};
        }
        return base;
    }

    static std::string identifierLatex(const std::string& id) {
        auto greek = [](const std::string& s) { return kGreek.count(s) ? "\\" + s : s; };
        // theta1 -> \theta_{1}, n1 -> n_{1}, n_a -> n_{a}, lambda0 -> \lambda_{0}, Emax -> E_{max}
        for (const auto& g : kGreek) {
            if (id.compare(0, g.size(), g) == 0) {
                std::string rest = id.substr(g.size());
                if (!rest.empty() && rest[0] == '_') rest.erase(0, 1);
                return "\\" + g + (rest.empty() ? "" : "_{" + rest + "}");
            }
        }
        const auto us = id.find('_');
        if (us != std::string::npos) return greek(id.substr(0, us)) + "_{" + id.substr(us + 1) + "}";
        if (id.size() > 1) return id.substr(0, 1) + "_{" + id.substr(1) + "}";
        return id;
    }

    Piece primary() {
        skip();
        if (m_i >= m_s.size()) throw std::runtime_error("Unexpected end of the expression");
        const char c = m_s[m_i];
        if (c == '(') {
            ++m_i;
            std::string inner = expr();
            if (!accept(')')) throw std::runtime_error("Missing ')'");
            return {"\\left(" + inner + "\\right)", false};
        }
        if (isDigit(c) || c == '.') {
            std::size_t start = m_i;
            while (m_i < m_s.size() && (isDigit(m_s[m_i]) || m_s[m_i] == '.')) ++m_i;
            return {m_s.substr(start, m_i - start), true};
        }
        if (isAlpha(c) || c == '_') {
            std::size_t start = m_i;
            while (m_i < m_s.size() && (std::isalnum(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == '_')) ++m_i;
            std::string name = m_s.substr(start, m_i - start);
            skip();
            if (m_i < m_s.size() && m_s[m_i] == '(' && kFunctions.count(name)) {
                ++m_i;
                std::string arg = expr();
                if (!accept(')')) throw std::runtime_error("Missing ')' after " + name);
                if (name == "sqrt") return {"\\sqrt{" + arg + "}", false};
                if (name == "abs") return {"\\left|" + arg + "\\right|", false};
                const std::string cmd = (name[0] == 'a' && name.size() == 4) ? "\\arc" + name.substr(1) : "\\" + name;
                return {cmd + "\\left(" + arg + "\\right)", false};
            }
            if (name == "pi") return {"\\pi", true};
            if (name == "e") return {"e", true};
            return {identifierLatex(name), true};
        }
        throw std::runtime_error(std::string("Unexpected '") + c + "'");
    }
};

} // namespace

std::string latexToExpression(const std::string& latex) {
    if (latex.find_first_not_of(" \t\n") == std::string::npos) throw std::runtime_error("Empty equation");
    return FromLatex(latex).run();
}

std::string expressionToLatex(const std::string& expression) {
    if (expression.find_first_not_of(" \t\n") == std::string::npos) throw std::runtime_error("Empty equation");
    return ToLatex(expression).run();
}

} // namespace phy104::EquationText
