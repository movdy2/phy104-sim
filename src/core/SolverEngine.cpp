#include "SolverEngine.h"
#include "EquationSolver.h"
#include "ExpressionEvaluator.h"
#include "LatexFormatter.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace phy104 {
namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}
bool isBlank(const std::string& s) { return s.find_first_not_of(" \t") == std::string::npos; }

bool contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    for (std::size_t pos = 0; (pos = text.find(from, pos)) != std::string::npos; pos += to.size())
        text.replace(pos, from.size(), to);
}

} // namespace

SolverEngine::SolverEngine(std::vector<PhysicsConstant> constants) {
    for (const auto& c : constants) {
        m_constants[c.id] = c.value;
        const bool ascii = std::all_of(c.symbol.begin(), c.symbol.end(),
                                       [](unsigned char ch) { return ch < 128; });
        if (!c.symbol.empty() && !ascii && c.symbol != "π")
            m_aliases.emplace_back(c.symbol, c.id);
    }
    // longest symbols first, so "ε₀" is replaced before "ε"
    std::sort(m_aliases.begin(), m_aliases.end(),
              [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
}

std::string SolverEngine::formatNumber(double value) {
    std::ostringstream out;
    out << std::setprecision(6) << value;
    return out.str();
}

std::string SolverEngine::substitute(const std::string& text,
                                     const std::map<std::string, std::string>& replacements) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (std::isalpha(c) || c == '_') {
            std::size_t j = i;
            while (j < text.size() && (std::isalnum(static_cast<unsigned char>(text[j])) || text[j] == '_')) ++j;
            const std::string name = text.substr(i, j - i);
            std::size_t k = j;
            while (k < text.size() && text[k] == ' ') ++k;
            const bool isFunction = k < text.size() && text[k] == '(';
            const auto it = replacements.find(name);
            out += (!isFunction && it != replacements.end()) ? it->second : name;
            i = j;
        } else if (std::isdigit(c) || c == '.') {              // copy numbers untouched (3e8)
            std::size_t j = i;
            while (j < text.size() && (std::isalnum(static_cast<unsigned char>(text[j])) || text[j] == '.')) ++j;
            out += text.substr(i, j - i);
            i = j;
        } else {
            out += text[i++];
        }
    }
    return out;
}

std::string SolverEngine::prepareInput(const std::string& text, const std::string& unit) const {
    std::string t = text;
    // "°" in a degree field is only decoration; elsewhere it means "× π/180".
    replaceAll(t, "°", unit == "deg" ? "" : "*(pi/180)");
    for (const auto& [symbol, id] : m_aliases) replaceAll(t, symbol, id);
    return t;
}

SolveResult SolverEngine::solve(const Formula& formula,
                                const std::map<std::string, std::string>& inputs) const {
    SolveResult result;
    result.formulaText = formula.equation;

    auto unitOf = [&](const Variable& v) {
        const auto it = formula.unitOverrides.find(v.id);
        return it != formula.unitOverrides.end() ? it->second : v.unit;
    };

    // 1) split "lhs = rhs"
    const auto eq = formula.equation.find('=');
    if (eq == std::string::npos || formula.equation.find('=', eq + 1) != std::string::npos) {
        result.error = "Formula '" + formula.id + "' must contain exactly one '='.";
        return result;
    }
    const std::string lhs = trim(formula.equation.substr(0, eq));
    const std::string rhs = trim(formula.equation.substr(eq + 1));

    // 2) find the single unknown
    std::vector<const Variable*> unknowns;
    for (const auto& var : formula.variables) {
        const auto it = inputs.find(var.id);
        if (it == inputs.end() || isBlank(it->second)) unknowns.push_back(&var);
    }
    if (unknowns.size() != 1) {
        result.error = unknowns.empty()
            ? "Leave exactly one field empty - that is the variable to calculate."
            : "Too many empty fields. Fill all variables except the one you want to find.";
        return result;
    }
    const Variable& unknown = *unknowns.front();
    const std::string unknownUnit = unitOf(unknown);
    result.unknownId = unknown.id;
    result.unknownDisplay = unknown.display;
    result.unit = unknownUnit;

    // 3) parse known values (user units) and convert to internal units (radians)
    VariableMap internal = m_constants;           // so equations may also use c, epsilon0 ... directly
    std::map<std::string, std::string> shown;     // text used in the "Substitute" step
    std::ostringstream knownText;
    bool usesDegrees = unknownUnit == "deg";
    for (const auto& var : formula.variables) {
        if (&var == &unknown) continue;
        const std::string unit = unitOf(var);
        double userValue = 0.0;
        try {
            userValue = ExpressionEvaluator::evaluate(prepareInput(inputs.at(var.id), unit), m_constants);
        } catch (const std::exception& e) {
            result.error = "Invalid value for " + var.display + ": " + e.what();
            return result;
        }
        internal[var.id] = (unit == "deg") ? userValue * kDegToRad : userValue;
        usesDegrees = usesDegrees || unit == "deg";

        std::string text = formatNumber(userValue);
        if (unit == "deg") text += "°";
        shown[var.id] = (userValue < 0) ? "(" + text + ")" : text;
        knownText << var.display << " = " << formatNumber(userValue);
        if (!unit.empty()) knownText << ' ' << unit;
        knownText << "\n";
    }

    // 4) direct substitution or numeric solution
    const auto rhsIds = ExpressionEvaluator::identifiers(rhs);
    const auto lhsIds = ExpressionEvaluator::identifiers(lhs);
    double internalValue = 0.0;
    std::vector<double> internalOthers;
    std::string method;

    auto evalSide = [&](const std::string& expr) -> bool {
        try {
            internalValue = ExpressionEvaluator::evaluate(expr, internal);
        } catch (const std::exception& e) {
            result.error = std::string("Calculation error: ") + e.what();
            return false;
        }
        if (!std::isfinite(internalValue)) {
            result.error = "The result is undefined for these inputs "
                           "(for example sin(angle) > 1 means total internal reflection).";
            result.physical = true;
            result.hint = formula.noSolutionHint;
            return false;
        }
        return true;
    };

    if (lhs == unknown.id && !contains(rhsIds, unknown.id)) {
        method = "direct";
        if (!evalSide(rhs)) return result;
    } else if (rhs == unknown.id && !contains(lhsIds, unknown.id)) {
        method = "direct";
        if (!evalSide(lhs)) return result;
    } else {
        method = "numeric";
        const SearchDomain domain = unknownUnit == "deg" ? SearchDomain::Angle
                                  : unknownUnit == "rad" ? SearchDomain::Phase
                                                         : SearchDomain::General;
        const RootResult root = EquationSolver::solve(lhs, rhs, unknown.id, internal, domain);
        if (!root.ok) {
            result.error = root.error;
            result.physical = true;
            result.hint = formula.noSolutionHint;
            return result;
        }
        internalValue = root.value;
        internalOthers = root.others;
    }

    // 5) back to user units
    const bool angleOut = unknownUnit == "deg";
    result.value = angleOut ? internalValue / kDegToRad : internalValue;
    for (double o : internalOthers) result.otherSolutions.push_back(angleOut ? o / kDegToRad : o);
    if (angleOut) {                                 // keep only physical angles 0..90 (drops 180° - θ mirror images)
        result.otherSolutions.erase(
            std::remove_if(result.otherSolutions.begin(), result.otherSolutions.end(),
                           [](double v) { return v < -1e-9 || v > 90.0 + 1e-9; }),
            result.otherSolutions.end());
    }

    // 6) steps
    const std::string unitText = unknownUnit.empty() ? "" : " " + unknownUnit;
    result.ok = true;
    result.steps.push_back("Formula: " + formula.equation);
    result.steps.push_back("Known values:\n" + knownText.str() +
                           (usesDegrees ? "(Angles are entered in degrees; the calculation converts them to radians.)" : ""));
    if (method == "direct") {
        const std::string expr = (lhs == unknown.id) ? rhs : lhs;
        result.steps.push_back("Solve for " + unknown.display + ": " + unknown.id + " = " + expr);
        result.steps.push_back("Substitute: " + unknown.id + " = " + substitute(expr, shown));
    } else {
        result.steps.push_back("Unknown: " + unknown.display + " (" + unknown.name + ")");
        result.steps.push_back("Substitute: " + substitute(formula.equation, shown));
        result.steps.push_back("The equation is solved numerically for " + unknown.id +
                               " (find the value that makes left side = right side).");
    }
    {   // verification, both sides with the solution
        VariableMap check = internal;
        check[unknown.id] = internalValue;
        try {
            const double l = ExpressionEvaluator::evaluate(lhs, check);
            const double r = ExpressionEvaluator::evaluate(rhs, check);
            result.checkSummary = "left side = " + formatNumber(l) + ", right side = " + formatNumber(r);
            result.steps.push_back("Check: " + result.checkSummary);
        } catch (const std::exception&) {}
    }
    result.answerLatex = LatexFormatter::answer(unknown.latex.empty() ? unknown.display : unknown.latex,
                                                result.value, unknownUnit);
    result.steps.push_back("Answer: " + unknown.display + " = " + formatNumber(result.value) + unitText);
    if (!result.otherSolutions.empty()) {
        std::string others;
        for (double o : result.otherSolutions) others += (others.empty() ? "" : ", ") + formatNumber(o);
        result.steps.push_back("Other mathematical solutions in the search range: " + others + unitText);
    }
    return result;
}

} // namespace phy104
