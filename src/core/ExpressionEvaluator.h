#pragma once
#include <map>
#include <string>
#include <vector>

namespace phy104 {

// نوع مختصر لخريطة المتغيرات (اسم المتغير -> قيمته)
using VariableMap = std::map<std::string, double>;

/// وحدة الزاوية المستخدمة في الدوال المثلثية (sin, cos, tan, ...)
/// الدوال المثلثية تستخدم الراديان افتراضياً. الآلة الحاسبة تمرر Degree
enum class AngleUnit { Radians, Degrees };

/// محلل ومقيّم التعبيرات الرياضية (Recursive-descent parser).
/// يدعم: + - * / ^ ( )، الأرقام (3e8)، المتغيرات، الثوابت (pi, e)،
/// والدوال: sin cos tan sec csc cot asin acos atan sqrt abs ln log.
/// الوحدات المدمجة: nm um mm cm km  ->  "550*nm" يعمل.
/// ترمي std::runtime_error إذا كان التعبير غير صالح.
class ExpressionEvaluator {
public:
    /// تقييم تعبير رياضي وإرجاع النتيجة.
    /// ترمي std::runtime_error إذا كان التعبير غير صالح.
    static double evaluate(const std::string& expression, const VariableMap& variables = {},
                           AngleUnit angleUnit = AngleUnit::Radians);

    /// إرجاع قائمة بأسماء المتغيرات المستخدمة في التعبير.
    /// (أسماء الدوال و "pi" لا تُعد متغيرات).
    static std::vector<std::string> identifiers(const std::string& expression);

    /// التحقق مما إذا كان الاسم هو دالة رياضية معروفة (مثل sin, cos).
    static bool isFunctionName(const std::string& name);

    /// تحويل الرموز التعبيرية (مثل ×, ÷, −, π, √, [ ] { }) إلى رموز يفهمها المحلل.
    static std::string normalize(std::string text);
};

} // namespace phy104