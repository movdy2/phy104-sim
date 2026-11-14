#include "EquationText.h"

#include <cctype>
#include <set>
#include <stdexcept>
#include <vector>

// استخدام using في ملف .cpp للراحة (مسموح)
using std::string;
using std::vector;
using std::set;
using std::size_t;
using std::runtime_error;
using std::isalpha;
using std::isdigit;
using std::isspace;
using std::isalnum;

namespace phy104::EquationText {
namespace {

// قوائم الدوال والحروف اليونانية المعروفة
const set<string> kFunctions = {"sin", "cos", "tan", "sec", "csc", "cot", "asin", "acos", "atan", "sqrt", "abs", "ln", "log"};
const set<string> kGreek = {"alpha", "beta", "gamma", "delta", "epsilon", "theta", "lambda", "mu", "pi", "phi", "rho",
                            "sigma", "tau", "omega", "nu", "eta", "kappa", "psi", "Delta", "Omega", "Sigma", "Phi", "Theta", "Lambda"};

bool isAlpha(char c) { return isalpha(static_cast<unsigned char>(c)) != 0; }
bool isDigit(char c) { return isdigit(static_cast<unsigned char>(c)) != 0; }

// ============================================================
// LaTeX -> تعبير رياضي
// المحول ينتج قائمة من العناصر (Items). العناصر المتجاورة
// التي تنتهي/تبدأ كمعاملات يتم ربطها بعلامة ضرب ضمنية '*'.
// ============================================================
struct Item { string text; bool operandEnd = false; bool operandStart = false; };

class FromLatex {
public:
    explicit FromLatex(const string& s) : m_s(s) {}

    string run() {
        const string out = sequence(false);
        if (m_i < m_s.size()) throw runtime_error("Unexpected '}' in the LaTeX");
        return out;
    }

private:
    const string& m_s;
    size_t m_i = 0;

    bool atEnd() const { return m_i >= m_s.size(); }
    void skipSpaces() { while (!atEnd() && isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i; }

    static string join(const vector<Item>& items) {
        string out;
        for (size_t k = 0; k < items.size(); ++k) {
            if (k > 0 && items[k - 1].operandEnd && items[k].operandStart) out += "*";
            out += items[k].text;
        }
        return out;
    }

    string group() {                                   // {...}
        skipSpaces();
        if (atEnd()) throw runtime_error("Missing argument in the LaTeX");
        if (m_s[m_i] != '{') return atom().text;            // وسيطة رمز واحد: x^2 (لا يدعم \frac12)
        ++m_i;
        const string inner = sequence(true);
        if (atEnd() || m_s[m_i] != '}') throw runtime_error("Missing '}' in the LaTeX");
        ++m_i;
        return inner;
    }

    string commandName() {
        size_t start = m_i;
        while (!atEnd() && isAlpha(m_s[m_i])) ++m_i;
        if (m_i == start && !atEnd()) ++m_i;
        return m_s.substr(start, m_i - start);
    }

    // معامل واحد: رقم، معرف (مع رمز سفلي / علامة اقتباس)، \command، مجموعة، أو قوس.
    Item atom() {
        skipSpaces();
        if (atEnd()) throw runtime_error("Unexpected end of the LaTeX");
        const char c = m_s[m_i];
        if (isDigit(c) || c == '.') {
            size_t start = m_i;
            while (!atEnd() && (isDigit(m_s[m_i]) || m_s[m_i] == '.')) ++m_i;
            return {m_s.substr(start, m_i - start), true, true};
        }
        if (isAlpha(c)) { ++m_i; return identifier(string(1, c)); }
        if (c == '{') return {"(" + group() + ")", true, true};
        if (c == '\\') { ++m_i; return command(); }
        throw runtime_error(string("Unexpected '") + c + "' in the LaTeX");
    }

    Item identifier(string name) {                     // بعد الحروف الأساسية: _sub وعلامات الاقتباس
        for (;;) {
            if (!atEnd() && m_s[m_i] == '_') {
                ++m_i;
                string sub = group();
                string flat;
                for (char ch : sub) if (isalnum(static_cast<unsigned char>(ch))) flat += ch;
                name += "_" + flat;
            } else if (!atEnd() && m_s[m_i] == '\'') {
                ++m_i;
                name += "p";                                // s' -> sp
            } else break;
        }
        return {name, true, true};
    }

    Item command() {
        const string name = commandName();
        if (name == "frac" || name == "dfrac") {
            const string a = group(), b = group();
            return {"((" + a + ")/(" + b + "))", true, true};
        }
        if (name == "sqrt") {
            skipSpaces();
            if (!atEnd() && m_s[m_i] == '[') {                       // \sqrt[n]{x} = x^(1/n)
                ++m_i;
                string n;
                while (!atEnd() && m_s[m_i] != ']') n += m_s[m_i++];
                if (atEnd()) throw runtime_error("Missing ']' in the LaTeX");
                ++m_i;
                return {"((" + group() + ")^(1/" + n + "))", true, true};
            }
            return {"sqrt(" + group() + ")", true, true};
        }
        if (name == "left" || name == "right") {
            skipSpaces();
            if (atEnd()) throw runtime_error("Missing bracket after \\" + name);
            const char b = m_s[m_i++];
            const bool open = name == "left";
            if (b == '(' || b == '[') return {"(", !open, open};
            if (b == ')' || b == ']') return {")", true, false};
            if (b == '.') return {"", false, false};
            throw runtime_error("Unsupported bracket after \\" + name);
        }
        if (name == "cdot" || name == "times") return {"*", false, false};
        if (name == "," || name == ";" || name == " " || name == "!" || name == "quad" || name == "qquad") return {"", false, false};
        if (name == "pi") return {"pi", true, true};
        if (name == "circ") return {"", false, false};
        if (kGreek.count(name)) return identifier(name);
        if (name == "arcsin" || name == "arccos" || name == "arctan") return {"a" + name.substr(3), false, true};
        if (kFunctions.count(name)) return {name, false, true};
        throw runtime_error("Unsupported LaTeX command \\" + name);
    }

    string sequence(bool untilBrace) {
        vector<Item> items;
        for (;;) {
            skipSpaces();
            if (atEnd()) break;
            if (m_s[m_i] == '}') { if (untilBrace) break; throw runtime_error("Unexpected '}' in the LaTeX"); }
            const char c = m_s[m_i];
            if (c == '+' || c == '-' || c == '=' || c == '/' || c == '*') { items.push_back({string(1, c), false, false}); ++m_i; continue; }
            if (c == '(' || c == '[') { items.push_back({"(", false, true}); ++m_i; continue; }
            if (c == ')' || c == ']') { items.push_back({")", true, false}); ++m_i; continue; }
            if (c == '^') {                                                   // الأس ينطبق على المعامل السابق
                ++m_i;
                if (items.empty()) throw runtime_error("'^' needs something before it");
                const string e = group();
                items.push_back({"^(" + e + ")", true, false});
                continue;
            }
            if (c == '_') throw runtime_error("Subscript '_' must follow a letter");
            Item a = atom();
            // دالة مع أس: \sin^2\theta -> (sin(theta))^(2)
            if (kFunctions.count(a.text) && !a.operandEnd) {
                skipSpaces();
                string power;
                if (!atEnd() && m_s[m_i] == '^') { ++m_i; power = group(); }
                skipSpaces();
                string arg;
                if (!atEnd() && (m_s[m_i] == '(' || (m_s[m_i] == '\\' && m_s.compare(m_i, 5, "\\left") == 0))) {
                    // وسيطة بين أقواس: اترك الحلقة الرئيسية تقرأها، ثم لفها.
                    size_t depth = 0, begin = m_i;
                    vector<Item> inner;
                    do {
                        skipSpaces();
                        if (m_s.compare(m_i, 5, "\\left") == 0) { m_i += 5; skipSpaces(); }
                        if (m_s.compare(m_i, 6, "\\right") == 0) { m_i += 6; skipSpaces(); }
                        const char ch = m_s[m_i];
                        if (ch == '(') ++depth;
                        if (ch == ')') --depth;
                        ++m_i;
                    } while (depth > 0 && !atEnd());
                    if (depth != 0) throw runtime_error("Unbalanced brackets in the LaTeX");
                    string raw = m_s.substr(begin, m_i - begin);
                    string cleaned;
                    for (size_t k = 0; k < raw.size();) {
                        if (raw.compare(k, 5, "\\left") == 0) k += 5;
                        else if (raw.compare(k, 6, "\\right") == 0) k += 6;
                        else cleaned += raw[k++];
                    }
                    FromLatex sub(cleaned);
                    arg = sub.run();                                          // يشمل الأقواس
                } else {
                    Item operand = atom();
                    arg = "(" + operand.text + ")";
                }
                const string call = a.text + arg;
                items.push_back({power.empty() ? call : "(" + call + ")^(" + power + ")", true, true});
                continue;
            }
            items.push_back(a);
        }
        return join(items);
    }
};

// ============================================================
// تعبير رياضي -> LaTeX
// ============================================================
struct Piece { string latex; bool simple = false; };         // simple: رمز / رقم واحد (لا يحتاج أقواس)

class ToLatex {
public:
    explicit ToLatex(const string& s) : m_s(s) {}

    string run() {
        string out = expr();
        skip();
        if (m_i < m_s.size() && m_s[m_i] == '=') {
            ++m_i;
            out += " = " + expr();
            skip();
        }
        if (m_i < m_s.size()) throw runtime_error(string("Unexpected '") + m_s[m_i] + "'");
        return out;
    }

private:
    const string& m_s;
    size_t m_i = 0;

    void skip() { while (m_i < m_s.size() && isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i; }
    bool accept(char c) { skip(); if (m_i < m_s.size() && m_s[m_i] == c) { ++m_i; return true; } return false; }

    string expr() {
        string out = term().latex;
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

    static string identifierLatex(const string& id) {
        auto greek = [](const string& s) { return kGreek.count(s) ? "\\" + s : s; };
        // theta1 -> \theta_{1}, n1 -> n_{1}, n_a -> n_{a}, lambda0 -> \lambda_{0}, Emax -> E_{max}
        for (const auto& g : kGreek) {
            if (id.compare(0, g.size(), g) == 0) {
                string rest = id.substr(g.size());
                if (!rest.empty() && rest[0] == '_') rest.erase(0, 1);
                return "\\" + g + (rest.empty() ? "" : "_{" + rest + "}");
            }
        }
        const auto us = id.find('_');
        if (us != string::npos) return greek(id.substr(0, us)) + "_{" + id.substr(us + 1) + "}";
        if (id.size() > 1) return id.substr(0, 1) + "_{" + id.substr(1) + "}";
        return id;
    }

    Piece primary() {
        skip();
        if (m_i >= m_s.size()) throw runtime_error("Unexpected end of the expression");
        const char c = m_s[m_i];
        if (c == '(') {
            ++m_i;
            string inner = expr();
            if (!accept(')')) throw runtime_error("Missing ')'");
            return {"\\left(" + inner + "\\right)", false};
        }
        if (isDigit(c) || c == '.') {
            size_t start = m_i;
            while (m_i < m_s.size() && (isDigit(m_s[m_i]) || m_s[m_i] == '.')) ++m_i;
            return {m_s.substr(start, m_i - start), true};
        }
        if (isAlpha(c) || c == '_') {
            size_t start = m_i;
            while (m_i < m_s.size() && (isalnum(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == '_')) ++m_i;
            string name = m_s.substr(start, m_i - start);
            skip();
            if (m_i < m_s.size() && m_s[m_i] == '(' && kFunctions.count(name)) {
                ++m_i;
                string arg = expr();
                if (!accept(')')) throw runtime_error("Missing ')' after " + name);
                if (name == "sqrt") return {"\\sqrt{" + arg + "}", false};
                if (name == "abs") return {"\\left|" + arg + "\\right|", false};
                const string cmd = (name[0] == 'a' && name.size() == 4) ? "\\arc" + name.substr(1) : "\\" + name;
                return {cmd + "\\left(" + arg + "\\right)", false};
            }
            if (name == "pi") return {"\\pi", true};
            if (name == "e") return {"e", true};
            return {identifierLatex(name), true};
        }
        throw runtime_error(string("Unexpected '") + c + "'");
    }
};

} // namespace

string latexToExpression(const string& latex) {
    if (latex.find_first_not_of(" \t\n") == string::npos) throw runtime_error("Empty equation");
    return FromLatex(latex).run();
}

string expressionToLatex(const string& expression) {
    if (expression.find_first_not_of(" \t\n") == string::npos) throw runtime_error("Empty equation");
    return ToLatex(expression).run();
}

} // namespace phy104::EquationText