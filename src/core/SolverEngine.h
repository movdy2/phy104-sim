#pragma once
#include "ExpressionEvaluator.h"
#include "Models.h"
#include <map>
#include <string>
#include <vector>

namespace phy104 {

struct SolveResult {
    bool ok = false;
    std::string error;                 // filled when ok == false
    std::string formulaText;
    std::string unknownId;             // "thetab"
    std::string unknownDisplay;        // "θ_b"
    double value = 0.0;                // in the unknown's unit (degrees for angles in deg)
    std::string unit;
    std::vector<double> otherSolutions;// extra solutions in the search range (same unit)
    std::vector<std::string> steps;    // readable solution steps
    std::string answerLatex;           // "\\theta_{b}=19.4712^{\\circ}" for the UI
    std::string checkSummary;          // "left side = 0.5, right side = 0.5"
    bool physical = false;             // true: no/undefined solution (a physics warning), false: bad input
    std::string hint;                  // formula-specific explanation, e.g. total internal reflection
};

/// Solves a Formula for its single unknown:
///   * unknown alone on one side  ->  direct substitution
///   * otherwise                  ->  numeric root finding (EquationSolver)
/// Angles typed in degrees are converted to radians internally and back.
class SolverEngine {
public:
    /// `constants` lets users type c, ε₀, μ₀, h ... inside input fields.
    explicit SolverEngine(std::vector<PhysicsConstant> constants = {});

    /// inputs: variable id -> text typed by the user (empty text = the unknown)
    SolveResult solve(const Formula& formula, const std::map<std::string, std::string>& inputs) const;

    /// Number formatting used everywhere (6 significant digits).
    static std::string formatNumber(double value);

    /// Replaces variable ids in `text` by the given replacement strings (functions untouched).
    static std::string substitute(const std::string& text, const std::map<std::string, std::string>& replacements);

private:
    std::string prepareInput(const std::string& text, const std::string& unit) const;

    VariableMap m_constants;                                       // id -> value
    std::vector<std::pair<std::string, std::string>> m_aliases;    // "ε₀" -> "epsilon0"
};

} // namespace phy104
