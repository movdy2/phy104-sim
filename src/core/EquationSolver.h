#pragma once
#include "ExpressionEvaluator.h"
#include <string>
#include <vector>

namespace phy104 {

/// Which values are physically sensible for the unknown (decides where we search).
enum class SearchDomain {
    General,   // any real number; positive answers preferred
    Angle,     // radians in [0, pi]; answers in [0, pi/2] preferred
    Phase      // radians in [0, 4 pi]
};

struct RootResult {
    bool ok = false;
    std::string error;
    double value = 0.0;               // the chosen solution (in the same units used for `known`)
    std::vector<double> others;       // other solutions found in the search range
};

/// Solves  lhs = rhs  for ONE unknown variable by numeric root finding:
///   g(x) = lhs(x) - rhs(x)
///   1. scan a grid of x values and look for sign changes of g
///   2. refine every sign change with bisection
///   3. reject "roots" that are really poles (like 1/x near 0)
/// This is why no hand-written rearranged formulas are needed.
class EquationSolver {
public:
    static RootResult solve(const std::string& lhs, const std::string& rhs,
                            const std::string& unknown, const VariableMap& known,
                            SearchDomain domain);
};

} // namespace phy104
