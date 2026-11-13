#include "MathSolverPage.h"

#include "ThemeManager.h"

#include "Components.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace phy104 {

MathSolverPage::MathSolverPage(QWidget* parent) : QWidget(parent), m_calc(5) {
    setObjectName("calcPage");                      // style sheet hook: the segment font is used on this page only
    auto* title = new QLabel("Calculator");
    title->setObjectName("pageTitle");
    auto* subtitle = new QLabel("Free math mode: type any expression - sin, cos, tan, asin, sqrt, abs, ln, log, "
                                "pi, e and Ans (the previous answer). Press Enter to calculate.");
    subtitle->setObjectName("muted");
    subtitle->setWordWrap(true);

    m_modeBadge = new QLabel("DEG");
    m_modeBadge->setObjectName("lcdBadge");
    auto* ansBadge = new QLabel("Ans");
    ansBadge->setObjectName("lcdBadge");

    m_history = new QTextBrowser;
    m_history->setObjectName("lcdScreen");
    m_history->setFixedHeight(120);

    m_input = new QLineEdit;
    m_input->setObjectName("lcdInput");
    ThemeManager::polishLcdInput(m_input);
    m_input->setPlaceholderText("e.g.  1+1+1+2-4/24   or   sqrt(abs(-16))+cos(0)");

    m_result = new QLabel("0");
    m_result->setObjectName("lcdResult");
    m_result->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_result->setMinimumHeight(64);

    m_status = new QLabel;
    m_status->setObjectName("statusError");
    m_status->hide();

    m_angleButton = new QPushButton("DEG / RAD");
    m_angleButton->setCheckable(true);
    m_angleButton->setChecked(true);                               // checked = degrees
    m_angleButton->setMinimumWidth(120);
    m_angleButton->setToolTip("Highlighted = degrees, plain = radians");
    m_keypad = new CalculatorKeypad;

    m_limit = new QComboBox;
    for (int n : {3, 5, 10, 20}) m_limit->addItem(QString::number(n), n);
    m_limit->setCurrentIndex(1);
    m_limit->setToolTip("How many calculations stay on the screen");
    auto* limitLabel = new QLabel("History lines");
    limitLabel->setObjectName("muted");

    auto* badges = new QHBoxLayout;
    badges->addWidget(m_modeBadge);
    badges->addWidget(ansBadge);
    badges->addStretch();

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_angleButton);
    buttons->addStretch();
    buttons->addWidget(limitLabel);
    buttons->addWidget(m_limit);

    auto* root = new QVBoxLayout(this);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addLayout(badges);
    root->addWidget(m_history);
    root->addWidget(m_input);
    root->addWidget(m_result);
    root->addWidget(m_status);
    root->addLayout(buttons);
    root->addWidget(m_keypad, 1);

    connect(m_input, &QLineEdit::returnPressed, this, &MathSolverPage::calculate);
    connect(m_keypad, &CalculatorKeypad::equals, this, &MathSolverPage::calculate);
    connect(m_keypad, &CalculatorKeypad::clearAll, this, &MathSolverPage::clearAll);
    connect(m_keypad, &CalculatorKeypad::backspace, this, [this] { m_input->backspace(); m_input->setFocus(); });
    connect(m_keypad, &CalculatorKeypad::insertText, this, [this](const QString& t) { m_input->insert(t); m_input->setFocus(); });
    connect(m_angleButton, &QPushButton::toggled, this, &MathSolverPage::toggleAngleMode);
    connect(m_limit, &QComboBox::currentIndexChanged, this, &MathSolverPage::onHistoryLimitChanged);

    showHistory();
}

void MathSolverPage::calculate() {
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    const CalcEntry entry = m_calc.evaluate(text.toStdString());

    if (entry.ok) {
        m_result->setText(QString::fromStdString(entry.result));
        m_status->hide();
        m_input->selectAll();                                      // typing replaces the old expression
    } else {
        m_result->setText("ERROR");
        m_status->setText("✖ " + QString::fromStdString(entry.error));
        m_status->show();
    }
    showHistory();
}

void MathSolverPage::showHistory() {
    QString html = "<table width='100%' cellspacing='2'>";
    for (const CalcEntry& e : m_calc.history()) {
        const QString expr = QString::fromStdString(e.expression).toHtmlEscaped();
        const QString res = e.ok ? "= " + QString::fromStdString(e.result).toHtmlEscaped() : "ERR";
        html += "<tr><td>" + expr + "</td><td align='right'><b>" + res + "</b></td></tr>";
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
    m_modeBadge->setText(degrees ? "DEG" : "RAD");
}

void MathSolverPage::onHistoryLimitChanged() {
    m_calc.setHistoryLimit(static_cast<std::size_t>(m_limit->currentData().toInt()));
    showHistory();
}

} // namespace phy104
