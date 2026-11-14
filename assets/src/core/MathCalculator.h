#pragma once
#include "ExpressionEvaluator.h"
#include <deque>
#include <string>

namespace phy104 {

struct CalcEntry {
    std::string expression;
    std::string result;     // formatted result (empty when !ok)
    bool ok = false;
    std::string error;      // readable message when !ok
};

/// Free math mode (like a scientific calculator): evaluates any expression with
/// ExpressionEvaluator - no physics formula needed. Remembers `Ans` and a bounded history.
class MathCalculator {
public:
    explicit MathCalculator(std::size_t historyLimit = 5) : m_limit(historyLimit) {}

    /// Evaluates, stores the entry in the history and updates Ans (only on success).
    CalcEntry evaluate(const std::string& expression);

    void setAngleUnit(AngleUnit unit) { m_unit = unit; }
    AngleUnit angleUnit() const { return m_unit; }

    /// Screen limit: how many entries are kept / shown. Older ones are dropped.
    void setHistoryLimit(std::size_t limit);
    std::size_t historyLimit() const { return m_limit; }
    const std::deque<CalcEntry>& history() const { return m_history; }
    void clearHistory();

    double ans() const { return m_ans; }

    /// 10 significant digits, no trailing zeros ("0.3", "1.2346e+20").
    static std::string formatResult(double value);

private:
    void trim();

    std::size_t m_limit;
    std::deque<CalcEntry> m_history;
    AngleUnit m_unit = AngleUnit::Degrees;      // like most calculators' school default
    double m_ans = 0.0;
};

} // namespace phy104
