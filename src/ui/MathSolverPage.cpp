#include "MathSolverPage.h"

#include "AppContext.h"
#include "Components.h"
#include "ThemeManager.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace phy104 {

MathSolverPage::MathSolverPage(AppContext* context, QWidget* parent)
    : BasePage("Calculator", "", context, parent), m_calc(5) {
    setObjectName("calcPage");
    setUnitsTag("MATH · DEG");

    auto* columns = new QHBoxLayout;
    columns->setSpacing(12);

    auto* historyPanel = new Panel("History");
    m_history = new QTextBrowser;
    m_history->setObjectName("lcdScreen");
    ThemeManager::polishLcdFont(m_history);
    m_history->setMinimumWidth(250);
    historyPanel->body()->addWidget(m_history, 1);
    columns->addWidget(historyPanel, 2);

    auto* calculatorPanel = new Panel("Expression");
    auto* modeRow = new QHBoxLayout;
    m_angleButton = new QPushButton("DEG");
    m_angleButton->setObjectName("calcMode");
    m_angleButton->setCheckable(true);
    m_angleButton->setChecked(true);                         // the school default; checked = degrees
    m_angleButton->setToolTip("Switch between degrees and radians");
    modeRow->addStretch();
    modeRow->addWidget(m_angleButton);
    calculatorPanel->body()->addLayout(modeRow);

    m_input = new QLineEdit;
    m_input->setObjectName("lcdInput");
    ThemeManager::polishLcdInput(m_input);
    m_input->setPlaceholderText("Expression");
    const auto refreshPlaceholder = [this] {
        QPalette palette = m_input->palette();
        const Theme& theme = ThemeManager::instance().theme();
        palette.setColor(QPalette::PlaceholderText, theme.dark ? QColor(Qt::white) : theme.text);
        m_input->setPalette(palette);
    };
    refreshPlaceholder();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, refreshPlaceholder);
    calculatorPanel->body()->addWidget(m_input);

    m_result = new QLabel("0");
    m_result->setObjectName("lcdResult");
    ThemeManager::polishLcdFont(m_result, true);
    m_result->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_result->setMinimumHeight(64);
    calculatorPanel->body()->addWidget(m_result);

    m_status = new QLabel;
    m_status->setObjectName("statusError");
    m_status->setWordWrap(true);
    m_status->hide();
    calculatorPanel->body()->addWidget(m_status);

    m_keypad = new CalculatorKeypad;
    m_keypad->setObjectName("calculatorKeypad");
    calculatorPanel->body()->addWidget(m_keypad, 1);
    columns->addWidget(calculatorPanel, 3);
    content()->addLayout(columns, 1);

    connect(m_input, &QLineEdit::returnPressed, this, &MathSolverPage::calculate);
    connect(m_keypad, &CalculatorKeypad::equals, this, &MathSolverPage::calculate);
    connect(m_keypad, &CalculatorKeypad::clearAll, this, &MathSolverPage::clearAll);
    connect(m_keypad, &CalculatorKeypad::backspace, this, [this] { m_input->backspace(); m_input->setFocus(); });
    connect(m_keypad, &CalculatorKeypad::insertText, this, [this](const QString& text) {
        m_input->insert(text);
        m_input->setFocus();
    });
    connect(m_angleButton, &QPushButton::toggled, this, &MathSolverPage::toggleAngleMode);
    showHistory();
}

void MathSolverPage::calculate() {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    const CalcEntry entry = m_calc.evaluate(text.toStdString());

    if (entry.ok) {
        m_result->setText(QString::fromStdString(entry.result));
        m_status->hide();
        m_input->selectAll();                                      // next typing replaces the previous expression
    } else {
        m_result->setText("ERROR");
        m_status->setText("✖ " + QString::fromStdString(entry.error));
        m_status->show();
    }
    showHistory();
}

void MathSolverPage::showHistory() {
    QString html = "<table width='100%' cellspacing='2'>";
    for (const CalcEntry& entry : m_calc.history()) {
        const QString expression = QString::fromStdString(entry.expression).toHtmlEscaped();
        const QString result = entry.ok ? "= " + QString::fromStdString(entry.result).toHtmlEscaped() : "ERROR";
        html += "<tr><td>" + expression + "</td><td align='right'><b>" + result + "</b></td></tr>";
    }
    html += "</table>";
    m_history->setHtml(html);
    m_history->verticalScrollBar()->setValue(m_history->verticalScrollBar()->maximum());
}

void MathSolverPage::clearAll() {
    m_calc.clearHistory();
    m_input->clear();
    m_result->setText("0");
    m_status->hide();
    showHistory();
    m_input->setFocus();
}

void MathSolverPage::toggleAngleMode(bool degrees) {
    m_calc.setAngleUnit(degrees ? AngleUnit::Degrees : AngleUnit::Radians);
    m_angleButton->setText(degrees ? "DEG" : "RAD");
    setUnitsTag(degrees ? "MATH · DEG" : "MATH · RAD");
}

} // namespace phy104
