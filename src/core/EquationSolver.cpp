#include "EquationSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace phy104 {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Sides { double lhs, rhs; };

class Problem {
public:
    Problem(const std::string& l, const std::string& r, const std::string& u, const VariableMap& k)
        : m_lhs(l), m_rhs(r), m_unknown(u), m_vars(k) {}

    /// Both sides at x. Returns false when the maths is undefined there (÷0, asin(2), sqrt(-1)...).
    bool sides(double x, Sides& out) {
        m_vars[m_unknown] = x;
        try {
            out.lhs = ExpressionEvaluator::evaluate(m_lhs, m_vars);
            out.rhs = ExpressionEvaluator::evaluate(m_rhs, m_vars);
        } catch (const std::exception&) {
            return false;
        }
        return std::isfinite(out.lhs) && std::isfinite(out.rhs);
    }
    double g(double x) {
        Sides s{};
        return sides(x, s) ? s.lhs - s.rhs : kNaN;
    }
    /// Is g(x) really zero (not just a jump across a pole)?
    bool isTrueRoot(double x) {
        Sides s{};
        if (!sides(x, s)) return false;
        const double scale = std::max({std::fabs(s.lhs), std::fabs(s.rhs), 1e-300});
        return std::fabs(s.lhs - s.rhs) <= 1e-6 * scale;
    }

private:
    std::string m_lhs, m_rhs, m_unknown;
    VariableMap m_vars;
};

double bisect(Problem& p, double lo, double hi) {
    double glo = p.g(lo);
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double gm = p.g(mid);
        if (std::isnan(gm)) break;
        if (gm == 0.0) return mid;
        if ((glo < 0) == (gm < 0)) { lo = mid; glo = gm; } else { hi = mid; }
    }
    return 0.5 * (lo + hi);
}

/// `undefinedX` has no value (g is NaN), `definedX` has one. Walk to the edge of the
/// defined region (e.g. n1 = n2 for sqrt(n1^2 - n2^2)) and return the defined side.
double definedEdge(Problem& p, double undefinedX, double definedX) {
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (undefinedX + definedX);
        if (std::isnan(p.g(mid))) undefinedX = mid; else definedX = mid;
    }
    return definedX;
}

std::vector<double> makeGrid(SearchDomain domain) {
    std::vector<double> xs;
    const double pi = std::numbers::pi;
    if (domain == SearchDomain::Angle) {
        const int n = 3600;                                  // 0.05 degree steps
        for (int i = 0; i <= n; ++i) xs.push_back(pi * i / n);
    } else if (domain == SearchDomain::Phase) {
        const int n = 8000;
        for (int i = 0; i <= n; ++i) xs.push_back(4 * pi * i / n);
    } else {
        // Log-spaced grid, 10^-15 ... 10^15, both signs, 12 points per decade.
        std::vector<double> pos;
        for (int k = -180; k <= 180; ++k) pos.push_back(std::pow(10.0, k / 12.0));
        for (auto it = pos.rbegin(); it != pos.rend(); ++it) xs.push_back(-*it);
        xs.push_back(0.0);
        for (double v : pos) xs.push_back(v);
    }
    return xs;
}

} // namespace

RootResult EquationSolver::solve(const std::string& lhs, const std::string& rhs,
                                 const std::string& unknown, const VariableMap& known,
                                 SearchDomain domain) {
    RootResult result;
    Problem problem(lhs, rhs, unknown, known);
    const std::vector<double> xs = makeGrid(domain);

    std::vector<double> roots;
    auto addRoot = [&](double x) {
        for (double r : roots)
            if (std::fabs(r - x) <= 1e-9 * std::max(1.0, std::fabs(x))) return;   // duplicate
        roots.push_back(x);
    };

    auto tryBracket = [&](double a, double ga, double b, double gb) {
        if (std::isnan(ga) || std::isnan(gb) || (ga < 0) == (gb < 0)) return;
        const double r = bisect(problem, a, b);
        if (problem.isTrueRoot(r)) addRoot(r);                // rejects poles
    };

    double prevX = 0.0, prevG = kNaN;
    bool havePrev = false;
    for (double x : xs) {
        const double gx = problem.g(x);
        const bool curOk = !std::isnan(gx), prevOk = !std::isnan(prevG);
        if (curOk) {
            Sides s{};
            problem.sides(x, s);
            const double scale = std::max({std::fabs(s.lhs), std::fabs(s.rhs), 1e-300});
            if (std::fabs(gx) <= 1e-15 * scale) {
                addRoot(x);                                   // exact hit / touching root
            } else if (prevOk) {
                tryBracket(prevX, prevG, x, gx);              // ordinary sign change
            } else if (havePrev) {                            // entering the defined region
                const double edge = definedEdge(problem, prevX, x);
                tryBracket(edge, problem.g(edge), x, gx);
            }
        } else if (prevOk && havePrev) {                      // leaving the defined region
            const double edge = definedEdge(problem, x, prevX);
            tryBracket(prevX, prevG, edge, problem.g(edge));
        }
        prevX = x;
        prevG = gx;
        havePrev = true;
    }

    if (roots.empty()) {
        result.error = "No solution found for these values "
                       "(for example total internal reflection, or the numbers are not physically possible).";
        return result;
    }
    std::sort(roots.begin(), roots.end());

    // Choose the physically preferred root.
    std::size_t pick = 0;
    const double pi = std::numbers::pi;
    if (domain == SearchDomain::Angle) {
        for (std::size_t i = 0; i < roots.size(); ++i)
            if (roots[i] >= -1e-12 && roots[i] <= pi / 2 + 1e-9) { pick = i; break; }
    } else if (domain == SearchDomain::Phase) {
        for (std::size_t i = 0; i < roots.size(); ++i)
            if (roots[i] > 1e-12) { pick = i; break; }
    } else {
        bool found = false;
        for (std::size_t i = 0; i < roots.size(); ++i)
            if (roots[i] > 0) { pick = i; found = true; break; }
        if (!found) pick = roots.size() - 1;                 // closest negative root to zero
    }

    result.ok = true;
    result.value = roots[pick];
    for (std::size_t i = 0; i < roots.size(); ++i)
        if (i != pick) result.others.push_back(roots[i]);
    return result;
}

} // namespace phy104
