#include "EquationSolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

using std::string;
using std::vector;
using std::size_t;
using std::max;
using std::fabs;
using std::sort;

namespace phy104 {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Sides { double lhs, rhs; };

// ============================================================
// فئة "المسألة" (Problem): تمثل المعادلة التي نبحث عن جذرها.
// تحسب قيمة الطرفين (lhs و rhs) عند قيمة معينة للمتغير المجهول.
// ============================================================
class Problem {
public:
    Problem(const string& l, const string& r, const string& u, const VariableMap& k)
        : m_lhs(l), m_rhs(r), m_unknown(u), m_vars(k) {}

    // حساب الطرفين عند قيمة x. ترجع false إذا كانت النتيجة غير معرفة (قسمة على صفر، asin(2)، إلخ).
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

    // دالة الفرق: g(x) = lhs - rhs. نبحث عن القيمة التي تجعلها صفراً.
    double g(double x) {
        Sides s{};
        return sides(x, s) ? s.lhs - s.rhs : kNaN;
    }

    // هل g(x) صفر حقيقي (وليس مجرد قفزة عبر قطب)؟
    bool isTrueRoot(double x) {
        Sides s{};
        if (!sides(x, s)) return false;
        const double scale = max({fabs(s.lhs), fabs(s.rhs), 1e-300});
        return fabs(s.lhs - s.rhs) <= 1e-6 * scale;
    }

private:
    string m_lhs, m_rhs, m_unknown;
    VariableMap m_vars;
};

// ============================================================
// التقسيم الثنائي (Bisection): خوارزمية للبحث عن الجذر بين قيمتين.
// تفترض أن الدالة تتغير إشارتها بين lo و hi.
// ============================================================
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

// ============================================================
// المشي إلى حافة المنطقة المعرفة:
// إذا انتقلنا من قيمة غير معرفة (NaN) إلى قيمة معرفة، نبحث عن الحد الفاصل.
// ============================================================
double definedEdge(Problem& p, double undefinedX, double definedX) {
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (undefinedX + definedX);
        if (std::isnan(p.g(mid))) undefinedX = mid; else definedX = mid;
    }
    return definedX;
}

// ============================================================
// بناء شبكة البحث (Grid): نحدد القيم التي سنبحث فيها.
// تعتمد على مجال البحث (زاوية، طور، عام).
// ============================================================
vector<double> makeGrid(SearchDomain domain) {
    vector<double> xs;
    const double pi = std::numbers::pi;
    
    if (domain == SearchDomain::Angle) {
        // شبكة خطية من 0 إلى pi (خطوة 0.05 درجة)
        const int n = 3600;                                  
        for (int i = 0; i <= n; ++i) xs.push_back(pi * i / n);
    } else if (domain == SearchDomain::Phase) {
        // شبكة خطية من 0 إلى 4pi
        const int n = 8000;
        for (int i = 0; i <= n; ++i) xs.push_back(4 * pi * i / n);
    } else {
        // شبكة لوغاريتمية من 10^-15 إلى 10^15 (للأرقام العامة)
        vector<double> pos;
        for (int k = -180; k <= 180; ++k) pos.push_back(std::pow(10.0, k / 12.0));
        for (auto it = pos.rbegin(); it != pos.rend(); ++it) xs.push_back(-*it);
        xs.push_back(0.0);
        for (double v : pos) xs.push_back(v);
    }
    return xs;
}

} // namespace

RootResult EquationSolver::solve(const string& lhs, const string& rhs,
                                 const string& unknown, const VariableMap& known,
                                 SearchDomain domain) {
    RootResult result;
    Problem problem(lhs, rhs, unknown, known);
    const vector<double> xs = makeGrid(domain);

    vector<double> roots;
    
    // إضافة جذر إلى القائمة (مع تجاهل المكرر)
    auto addRoot = [&](double x) {
        for (double r : roots)
            if (fabs(r - x) <= 1e-9 * max(1.0, fabs(x))) return;
        roots.push_back(x);
    };

    // محاولة إيجاد جذر بين قيمتين (a و b) باستخدام الإشارات
    auto tryBracket = [&](double a, double ga, double b, double gb) {
        if (std::isnan(ga) || std::isnan(gb) || (ga < 0) == (gb < 0)) return;
        const double r = bisect(problem, a, b);
        if (problem.isTrueRoot(r)) addRoot(r); // رفض الأقطاب
    };

    double prevX = 0.0, prevG = kNaN;
    bool havePrev = false;

    // المرور على كل القيم في الشبكة
    for (double x : xs) {
        const double gx = problem.g(x);
        const bool curOk = !std::isnan(gx), prevOk = !std::isnan(prevG);
        
        if (curOk) {
            Sides s{};
            problem.sides(x, s);
            const double scale = max({fabs(s.lhs), fabs(s.rhs), 1e-300});
            
            if (fabs(gx) <= 1e-15 * scale) {
                addRoot(x); // إصابة مباشرة
            } else if (prevOk) {
                tryBracket(prevX, prevG, x, gx); // تغير في الإشارة
            } else if (havePrev) {
                // دخلنا منطقة معرفة: نبحث عن الحد الفاصل
                const double edge = definedEdge(problem, prevX, x);
                tryBracket(edge, problem.g(edge), x, gx);
            }
        } else if (prevOk && havePrev) {
            // خرجنا من منطقة معرفة: نبحث عن الحد الفاصل
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
    
    sort(roots.begin(), roots.end());

    // اختيار الجذر المفضل فيزيائياً
    size_t pick = 0;
    const double pi = std::numbers::pi;
    
    if (domain == SearchDomain::Angle) {
        // للزوايا: نفضل الجذر في النطاق [0, pi/2]
        for (size_t i = 0; i < roots.size(); ++i)
            if (roots[i] >= -1e-12 && roots[i] <= pi / 2 + 1e-9) { pick = i; break; }
    } else if (domain == SearchDomain::Phase) {
        // للطور: نفضل الجذر الموجب الأول
        for (size_t i = 0; i < roots.size(); ++i)
            if (roots[i] > 1e-12) { pick = i; break; }
    } else {
        // للعام: نفضل الجذر الموجب (أو السالب الأقرب للصفر)
        bool found = false;
        for (size_t i = 0; i < roots.size(); ++i)
            if (roots[i] > 0) { pick = i; found = true; break; }
        if (!found) pick = roots.size() - 1;
    }

    result.ok = true;
    result.value = roots[pick];
    for (size_t i = 0; i < roots.size(); ++i)
        if (i != pick) result.others.push_back(roots[i]);
        
    return result;
}

} // namespace phy104