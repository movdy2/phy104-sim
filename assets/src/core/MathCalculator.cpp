#include "MathCalculator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using std::string;
using std::size_t;
using std::max;
using std::runtime_error;

namespace phy104 {

/// تنسيق النتيجة: 10 أرقام معنوية، وحذف "−0"
string MathCalculator::formatResult(double value) {
    if (value == 0.0) return "0";                      // يحذف أيضاً "-0"
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.10g", value);
    return buffer;
}

/// حساب تعبير رياضي حر (بدون معادلة فيزيائية)
CalcEntry MathCalculator::evaluate(const string& expression) {
    CalcEntry entry;
    entry.expression = expression;
    try {
        // نمرر قيمة Ans السابقة كمتغير داخل التعبير
        const double value = ExpressionEvaluator::evaluate(expression, {{"Ans", m_ans}}, m_unit);
        
        // التحقق من أن النتيجة ليست NaN أو Infinity
        if (!std::isfinite(value)) throw runtime_error("Math error (result is not a real number)");
        
        entry.result = formatResult(value);
        entry.ok = true;
        m_ans = value; // تحديث Ans للمرة القادمة
    } catch (const std::exception& e) {
        entry.error = e.what();
    }
    m_history.push_back(entry);
    trim(); // الحفاظ على حجم السجل (History Limit)
    return entry;
}

/// تحديد الحد الأقصى لعدد السجلات في السجل (History)
void MathCalculator::setHistoryLimit(size_t limit) {
    m_limit = max<size_t>(1, limit); // لا يقل عن 1
    trim();
}

/// مسح السجل وإعادة تعيين Ans
void MathCalculator::clearHistory() {
    m_history.clear();
    m_ans = 0.0;
}

/// حذف أقدم السجلات إذا تجاوزت الحد الأقصى
void MathCalculator::trim() {
    while (m_history.size() > m_limit) m_history.pop_front();
}

} // namespace phy104