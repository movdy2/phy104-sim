#include "ExpressionEvaluator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace phy104 {
namespace {

const char* const kFunctions[] = {"sin", "cos", "tan", "sec", "csc", "cot", "asin", "acos", "atan", "sqrt", "abs", "ln", "log",
                                  "sinh", "cosh", "tanh", "exp", "floor", "ceil", "round", "sgn"};

// Grammar:
//   expr    = term   { ('+'|'-') term }
//   term    = unary  { ('*'|'/') unary }
//   unary   = '-' unary | power
//   power   = primary [ '^' unary ]
//   primary = number | name | name '(' expr ')' | '(' expr ')'
class Parser {
public:
    Parser(const std::string& text, const VariableMap& vars, AngleUnit unit)
        : m_text(text), m_vars(vars), m_unit(unit) {}

    double parse() {
        const double value = parseExpression();
        skipSpaces();
        if (m_pos != m_text.size())
            throw std::runtime_error(std::string("Unexpected character '") + m_text[m_pos] + "'");
        return value;
    }

private:
    const std::string& m_text;
    const VariableMap& m_vars;
    AngleUnit m_unit;
    std::size_t m_pos = 0;

    void skipSpaces() {
        while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
    }
    bool accept(char c) {
        skipSpaces();
        if (m_pos < m_text.size() && m_text[m_pos] == c) { ++m_pos; return true; }
        return false;
    }

    double parseExpression() {
        double value = parseTerm();
        for (;;) {
            if (accept('+')) value += parseTerm();
            else if (accept('-')) value -= parseTerm();
            else return value;
        }
    }
    double parseTerm() {
        double value = parseUnary();
        for (;;) {
            if (accept('*')) value *= parseUnary();
            else if (accept('/')) {
                const double d = parseUnary();
                if (d == 0.0) throw std::runtime_error("Division by zero");
                value /= d;
            } else return value;
        }
    }
    double parseUnary() {
        if (accept('-')) return -parseUnary();
        if (accept('+')) return parseUnary();
        return parsePower();
    }
    double parsePower() {
        const double base = parsePrimary();
        if (accept('^')) return std::pow(base, parseUnary());
        return base;
    }
    double parsePrimary() {
        skipSpaces();
        if (m_pos >= m_text.size()) throw std::runtime_error("Unexpected end of expression");
        const char c = m_text[m_pos];

        if (c == '(') {
            ++m_pos;
            const double v = parseExpression();
            if (!accept(')')) throw std::runtime_error("Missing ')'");
            return v;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            char* end = nullptr;
            const double v = std::strtod(m_text.c_str() + m_pos, &end);
            m_pos = static_cast<std::size_t>(end - m_text.c_str());
            return v;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const std::size_t start = m_pos;
            while (m_pos < m_text.size() &&
                   (std::isalnum(static_cast<unsigned char>(m_text[m_pos])) || m_text[m_pos] == '_'))
                ++m_pos;
            const std::string name = m_text.substr(start, m_pos - start);

            if (accept('(')) {                     // function call
                const double arg = parseExpression();
                if (!accept(')')) throw std::runtime_error("Missing ')' after " + name);
                return callFunction(name, arg);
            }
            return lookup(name);
        }
        throw std::runtime_error(std::string("Unexpected character '") + c + "'");
    }

    double lookup(const std::string& name) const {
        const auto it = m_vars.find(name);                 // user variables win
        if (it != m_vars.end()) return it->second;
        if (name == "pi") return std::numbers::pi;
        if (name == "e")  return std::numbers::e;
        if (name == "nm") return 1e-9;
        if (name == "um") return 1e-6;
        if (name == "mm") return 1e-3;
        if (name == "cm") return 1e-2;
        if (name == "km") return 1e3;
        throw std::runtime_error("Unknown variable '" + name + "'");
    }

    // ---- trigonometry in degrees: exact for multiples of 90 (sin(180) = 0, not 1.2e-16) ----
    static double sinDeg(double d) {
        double r = std::fmod(d, 360.0);
        if (r < 0) r += 360.0;
        if (r == 0.0 || r == 180.0) return 0.0;
        if (r == 90.0)  return 1.0;
        if (r == 270.0) return -1.0;
        return std::sin(r * std::numbers::pi / 180.0);
    }
    static double cosDeg(double d) { return sinDeg(d + 90.0); }
    static double tanDeg(double d) {
        double r = std::fmod(d, 180.0);
        if (r < 0) r += 180.0;
        if (r == 0.0) return 0.0;
        if (r == 90.0) return std::numeric_limits<double>::quiet_NaN();
        return std::tan(r * std::numbers::pi / 180.0);
    }
    static double toDeg(double rad) { return rad * 180.0 / std::numbers::pi; }

    double callFunction(const std::string& f, double x) const {
        const bool deg = m_unit == AngleUnit::Degrees;
        if (f == "sin")  return deg ? sinDeg(x) : std::sin(x);
        if (f == "cos")  return deg ? cosDeg(x) : std::cos(x);
        if (f == "tan")  return deg ? tanDeg(x) : std::tan(x);
        if (f == "sec")  return 1.0 / (deg ? cosDeg(x) : std::cos(x));
        if (f == "csc")  return 1.0 / (deg ? sinDeg(x) : std::sin(x));
        if (f == "cot")  return deg ? 1.0 / tanDeg(x) : 1.0 / std::tan(x);
        if (f == "asin") return deg ? toDeg(std::asin(x)) : std::asin(x);   // NaN if |x| > 1
        if (f == "acos") return deg ? toDeg(std::acos(x)) : std::acos(x);
        if (f == "atan") return deg ? toDeg(std::atan(x)) : std::atan(x);
        if (f == "sqrt") return std::sqrt(x);              // NaN if x < 0
        if (f == "abs")  return std::fabs(x);
        if (f == "ln")   return std::log(x);
        if (f == "log")  return std::log10(x);
        if (f == "sinh") return std::sinh(x);
        if (f == "cosh") return std::cosh(x);
        if (f == "tanh") return std::tanh(x);
        if (f == "exp")  return std::exp(x);
        if (f == "floor") return std::floor(x);
        if (f == "ceil") return std::ceil(x);
        if (f == "round") return std::round(x);
        if (f == "sgn")  return x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0);
        throw std::runtime_error("Unknown function '" + f + "'");
    }
};

} // namespace

double ExpressionEvaluator::evaluate(const std::string& expression, const VariableMap& variables,
                                     AngleUnit angleUnit) {
    const std::string clean = normalize(expression);
    if (clean.find_first_not_of(" \t") == std::string::npos)
        throw std::runtime_error("Empty expression");
    Parser parser(clean, variables, angleUnit);
    return parser.parse();
}

bool ExpressionEvaluator::isFunctionName(const std::string& name) {
    return std::any_of(std::begin(kFunctions), std::end(kFunctions),
                       [&](const char* f) { return name == f; });
}

std::vector<std::string> ExpressionEvaluator::identifiers(const std::string& expression) {
    std::vector<std::string> result;
    std::size_t i = 0;
    while (i < expression.size()) {
        const unsigned char c = static_cast<unsigned char>(expression[i]);
        if (std::isalpha(c) || c == '_') {
            std::size_t j = i;
            while (j < expression.size() &&
                   (std::isalnum(static_cast<unsigned char>(expression[j])) || expression[j] == '_'))
                ++j;
            const std::string name = expression.substr(i, j - i);
            if (!isFunctionName(name) && name != "pi" &&
                std::find(result.begin(), result.end(), name) == result.end())
                result.push_back(name);
            i = j;
        } else if (std::isdigit(c) || c == '.') {          // skip numbers like 3e8 / 0.5
            std::size_t j = i;
            while (j < expression.size() &&
                   (std::isalnum(static_cast<unsigned char>(expression[j])) || expression[j] == '.'))
                ++j;
            i = j;
        } else {
            ++i;
        }
    }
    return result;
}

std::string ExpressionEvaluator::normalize(std::string text) {
    const std::pair<const char*, const char*> table[] = {
        {"×", "*"}, {"÷", "/"}, {"−", "-"}, {"π", "pi"}, {"[", "("}, {"]", ")"}, {"{", "("}, {"}", ")"}};
    for (const auto& [from, to] : table) {
        const std::string f = from;
        std::size_t pos = 0;
        while ((pos = text.find(f, pos)) != std::string::npos) {
            text.replace(pos, f.size(), to);
            pos += std::strlen(to);
        }
    }
    // "√(16)" -> "sqrt(16)"   and   "√16" -> "sqrt(16)"
    const std::string root = "√";
    std::size_t pos = 0;
    while ((pos = text.find(root, pos)) != std::string::npos) {
        std::size_t after = pos + root.size();
        if (after < text.size() && text[after] == '(') {
            text.replace(pos, root.size(), "sqrt");
        } else {
            std::size_t end = after;
            while (end < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '.' || text[end] == '_'))
                ++end;
            text.replace(pos, end - pos, "sqrt(" + text.substr(after, end - after) + ")");
        }
        pos += 4;
    }
    return text;
}

} // namespace phy104
