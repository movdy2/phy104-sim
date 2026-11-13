#pragma once
#include "ExpressionEvaluator.h"
#include <string>
#include <vector>

namespace phy104 {

/// المجال الفيزيائي للبحث عن الجذر (يحدد أين نبحث).
enum class SearchDomain {
    General,   // أي رقم حقيقي؛ نفضل الإجابات الموجبة
    Angle,     // زاوية بالراديان في [0, pi]؛ نفضل [0, pi/2]
    Phase      // طور بالراديان في [0, 4pi]
};

/// نتيجة البحث عن الجذر.
struct RootResult {
    bool ok = false;                  // هل تم إيجاد حل؟
    std::string error;                // رسالة الخطأ (إذا لم يوجد حل)
    double value = 0.0;               // الحل المختار (بنفس وحدات `known`)
    std::vector<double> others;       // حلول أخرى موجودة في نطاق البحث
};

/// محلل معادلات عددي (Numeric Root Finder):
/// يحل  lhs = rhs  لمتغير واحد مجهول:
///   g(x) = lhs(x) - rhs(x)
///   1. يمسح شبكة من قيم x ويبحث عن تغيرات في إشارة g
///   2. يحسن كل تغير إشارة باستخدام التقسيم الثنائي (Bisection)
///   3. يرفض "الجذور" التي هي في الحقيقة أقطاب (مثل 1/x قرب 0)
/// هذا هو السبب في أننا لا نحتاج إلى كتابة معادلات معاد ترتيبها يدوياً.
class EquationSolver {
public:
    static RootResult solve(const std::string& lhs, const std::string& rhs,
                            const std::string& unknown, const VariableMap& known,
                            SearchDomain domain);
};

} // namespace phy104