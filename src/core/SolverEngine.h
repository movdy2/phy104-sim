#pragma once
#include "ExpressionEvaluator.h"
#include "Models.h"
#include <map>
#include <string>
#include <vector>
#include <utility>
using std::string;
using std::map;
using std::vector;
using std::pair;
//---------------------------------------------------------------------------------------------------------------------
namespace phy104 {

struct SolveResult {
    bool ok = false;
    string error; // filled when ok == false
    string formulaText;
    string unknownId; // "thetab"
    string unknownDisplay; // "θ_b"
    double value = 0.0; // in the unknown's unit (degrees for angles in deg)
    string unit;
    vector<double> otherSolutions;// extra solutions in the search range (same unit)
    vector<string> steps; // readable solution steps
};

/// Solves a Formula for its single unknown:
/// * unknown alone on one side -> direct substitution
/// * otherwise -> numeric root finding (EquationSolver)
/// Angles typed in degrees are converted to radians internally and back.
class SolverEngine {
public:
    /// `constants` lets users type c, ε₀, μ₀, h ... inside input fields.
    explicit SolverEngine(vector<PhysicsConstant> constants = {});

    /// inputs: variable id -> text typed by the user (empty text = the unknown)
    SolveResult solve(const Formula& formula, const map<string, string>& inputs) const;

    /// Number formatting used everywhere (6 significant digits).
    static string formatNumber(double value);

    /// Replaces variable ids in `text` by the given replacement strings (functions untouched).
    static string substitute(const string& text, const map<string, string>& replacements);

private:
    string prepareInput(const string& text, const string& unit) const;

    VariableMap m_constants; // id -> value
    vector<pair<string, string>> m_aliases; // "ε₀" -> "epsilon0"
};

} // namespace phy104