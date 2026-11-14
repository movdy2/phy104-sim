#include "MathCalculator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace phy104 {

std::string MathCalculator::formatResult(double value) {
    if (value == 0.0) return "0";                      // also removes "-0"
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.10g", value);
    return buffer;
}

CalcEntry MathCalculator::evaluate(const std::string& expression) {
    CalcEntry entry;
    entry.expression = expression;
    try {
        const double value = ExpressionEvaluator::evaluate(expression, {{"Ans", m_ans}}, m_unit);
        if (!std::isfinite(value)) throw std::runtime_error("Math error (result is not a real number)");
        entry.result = formatResult(value);
        entry.ok = true;
        m_ans = value;
    } catch (const std::exception& e) {
        entry.error = e.what();
    }
    m_history.push_back(entry);
    trim();
    return entry;
}

void MathCalculator::setHistoryLimit(std::size_t limit) {
    m_limit = std::max<std::size_t>(1, limit);
    trim();
}

void MathCalculator::clearHistory() {
    m_history.clear();
    m_ans = 0.0;
}

void MathCalculator::trim() {
    while (m_history.size() > m_limit) m_history.pop_front();
}

} // namespace phy104
