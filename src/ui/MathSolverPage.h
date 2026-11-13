#pragma once
#include "MathCalculator.h"
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;

namespace phy104 { class CalculatorKeypad; }

namespace phy104 {

/// Free math mode: a scientific-calculator page that evaluates any expression
/// (functions, pi, e, Ans) without a physics formula.
class MathSolverPage : public QWidget {
    Q_OBJECT
public:
    explicit MathSolverPage(QWidget* parent = nullptr);

private slots:
    void calculate();
    void clearAll();
    void toggleAngleMode(bool degrees);
    void onHistoryLimitChanged();

private:
    void showHistory();

    MathCalculator m_calc;
    QTextBrowser* m_history;
    QLineEdit* m_input;
    QLabel* m_result;
    QLabel* m_status;
    QLabel* m_modeBadge;
    QPushButton* m_angleButton;
    QComboBox* m_limit;
    CalculatorKeypad* m_keypad;
};

} // namespace phy104
