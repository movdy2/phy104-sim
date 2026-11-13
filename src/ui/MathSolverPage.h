#pragma once
#include "BasePage.h"
#include "MathCalculator.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTextBrowser;
class QPushButton;

namespace phy104 {
class AppContext;
class CalculatorKeypad;

/// Clean, free-math mode: enter an expression, get a result, and reuse recent answers with Ans.
class MathSolverPage : public BasePage {
    Q_OBJECT
public:
    explicit MathSolverPage(AppContext* context, QWidget* parent = nullptr);

private slots:
    void calculate();
    void clearAll();
    void toggleAngleMode(bool degrees);

private:
    void showHistory();

    MathCalculator m_calc;
    QTextBrowser* m_history = nullptr;
    QLineEdit* m_input = nullptr;
    QLabel* m_result = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_angleButton = nullptr;
    CalculatorKeypad* m_keypad = nullptr;
};

} // namespace phy104
