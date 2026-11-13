#pragma once
#include <map>
#include <string>
#include <vector>

namespace phy104 {

using VariableMap = std::map<std::string, double>;

/// Unit used by sin/cos/tan/... (inputs) and asin/acos/atan (outputs).
enum class AngleUnit { Radians, Degrees };

/// Recursive-descent parser for math text such as "1+1+1+2-4/24" or "sqrt(abs(-16))+cos(0)".
/// Supports + - * / ^ ( ), numbers (3e8), variables, constants (pi, e) and the functions
/// sin cos tan sec csc cot asin acos atan sqrt abs ln log.
/// ANGLES: trigonometric functions use RADIANS by default (the formula solver converts degree
/// variables itself). The free calculator passes AngleUnit::Degrees for DEG mode.
/// Built-in length units: nm um mm cm km  ->  "550*nm" works.
/// Throws std::runtime_error with a readable message on bad input.
class ExpressionEvaluator {
public:
    static double evaluate(const std::string& expression, const VariableMap& variables = {},
                           AngleUnit angleUnit = AngleUnit::Radians);

    /// Variable names used in an expression, in order of first appearance
    /// (function names and "pi" are not variables).
    static std::vector<std::string> identifiers(const std::string& expression);

    static bool isFunctionName(const std::string& name);

    /// Converts keyboard symbols (× ÷ − π √ [ ] { }) into plain ASCII.
    static std::string normalize(std::string text);
};

} // namespace phy104
