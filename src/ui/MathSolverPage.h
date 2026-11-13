#pragma once
#include "MathCalculator.h"
#include "Models.h"

#include <vector>
#include <QWidget>

class QComboBox;
class QStackedWidget;
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
    /// `groups` are the plain-text key groups of keyboard.json (Greek, Constants, Functions, Operators, Numbers,
    /// Variables): the calculator shows them under the keypad and inserts their text - no LaTeX here, that is the
    /// Equation Lab's job.
    explicit MathSolverPage(std::vector<KeyboardGroup> groups = {}, QWidget* parent = nullptr);

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
    QComboBox* m_keysCombo = nullptr;
    QStackedWidget* m_keysStack = nullptr;
};

} // namespace phy104
