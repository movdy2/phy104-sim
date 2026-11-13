#include "EquationLabPage.h"
#include "AppContext.h"
#include "Components.h"
#include "EquationText.h"
#include "ExpressionEvaluator.h"
#include "LatexFormatter.h"
#include "LatexRenderer.h"
#include "ScientificKeyboard.h"
#include "SolverEngine.h"
#include "ThemeManager.h"
#include "Units.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTextBlock>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace phy104 {
namespace {

const char* const kStarter = "r_m = \\sqrt{\\frac{m\\lambda R}{n}}";

/// Sensible starting values for common optics symbols (the design example: m = 4, λ = 600 nm, R = 1 m, n = 1).
std::pair<QString, QString> defaultsFor(const std::string& id) {
    if (id == "lambda" || id == "lambda_0") return {"600", "nm"};
    if (id == "m") return {"4", ""};
    if (id == "R") return {"1", "m"};
    if (id == "r_m" || id == "r") return {"", "mm"};
    return {"1", ""};
}

/// 4 decimals like the LCD of the design ("1.5492"); very small or large values use scientific notation.
QString shownNumber(double v) {
    if (v == 0.0 || (std::fabs(v) >= 1e-3 && std::fabs(v) < 1e6)) return QString::number(v, 'f', 4);
    return QString::fromStdString(LatexFormatter::number(v));
}

std::string numberText(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.12g", v);
    return buf;
}

} // namespace

EquationLabPage::EquationLabPage(AppContext* ctx, std::vector<ScientificPage> keyboardPages, QWidget* parent)
    : BasePage("Equation Lab", "LATEX WORKSPACE / Edit, preview & evaluate an optics equation", ctx, parent) {
    useSaveExportHeader();
    auto* row = new QHBoxLayout;
    row->setSpacing(12);

    // ---------------------------------------------------------------- left: LaTeX source
    m_sourcePanel = new Panel("LaTeX source", "MATH MODE");
    auto* bar = new QHBoxLayout;
    auto* templates = new QToolButton;
    templates->setObjectName("fmt");
    templates->setText("Insert template ▾");
    templates->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(templates);
    struct T { const char* name; const char* latex; };
    for (const T& t : {T{"Snell's law", "n_1 \\sin\\theta_1 = n_2 \\sin\\theta_2"}, T{"Critical angle", "\\sin\\theta_c = \\frac{n_2}{n_1}"},
                       T{"Thin lens", "\\frac{1}{f} = \\frac{1}{d_o} + \\frac{1}{d_i}"}, T{"Newton's rings", "r_m = \\sqrt{\\frac{m\\lambda R}{n}}"},
                       T{"Double slit", "\\Delta y = \\frac{\\lambda L}{d}"}, T{"Single slit", "a \\sin\\theta = m\\lambda"},
                       T{"Grating", "d \\sin\\theta_m = m\\lambda"}, T{"Malus' law", "I = I_0 \\cos^2\\theta"}}) {
        QAction* a = menu->addAction(t.name);
        connect(a, &QAction::triggered, this, [this, latex = QString(t.latex)] {
            if (!m_source->document()->isEmpty()) m_source->appendPlainText("");
            m_source->textCursor().insertText(latex);
            m_source->setFocus();
        });
    }
    templates->setMenu(menu);
    m_keyboardToggle = new QToolButton;
    m_keyboardToggle->setObjectName("fmtAccent");
    m_keyboardToggle->setCheckable(true);
    m_keyboardToggle->setText("Keyboard · closed");
    auto* mode = new QLabel("LaTeX · math mode");
    mode->setObjectName("hint");
    bar->addWidget(templates);
    bar->addWidget(m_keyboardToggle);
    bar->addStretch();
    bar->addWidget(mode);
    m_sourcePanel->body()->addLayout(bar);

    m_source = new QPlainTextEdit;
    m_source->setObjectName("mdEditor");
    QFont f(ThemeManager::monoFont());
    f.setPixelSize(16);
    m_source->setFont(f);
    m_source->setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    std::string initial = kStarter;
    if (!ctx->log.equations().empty()) {
        initial.clear();
        for (const auto& e : ctx->log.equations()) initial += (initial.empty() ? "" : "\n") + e;
    }
    m_source->setPlainText(QString::fromStdString(initial));
    m_sourcePanel->body()->addWidget(m_source, 1);
    m_position = new QLabel;
    m_position->setObjectName("caption");
    m_parse = new QLabel;
    m_parse->setObjectName("hint");
    m_sourcePanel->body()->addWidget(m_position);
    m_sourcePanel->body()->addWidget(m_parse);
    row->addWidget(m_sourcePanel, 5);

    // ---------------------------------------------------------------- right: preview, result, variables
    auto* right = new QVBoxLayout;
    right->setSpacing(12);
    m_renderPanel = new Panel("Live rendered equation", "VALID");
    m_rendered = new LatexWidget;
    m_rendered->setPointSize(30);
    m_substitution = new QLabel;
    m_substitution->setObjectName("mono");
    m_substitution->setWordWrap(true);
    m_result = new LatexWidget;
    m_result->setLcdStyle(true);
    m_result->setPointSize(28);
    m_renderPanel->body()->addWidget(m_rendered);
    m_renderPanel->body()->addWidget(m_substitution);
    m_renderPanel->body()->addWidget(m_result);
    right->addWidget(m_renderPanel);

    m_varsPanel = new Panel("Extracted variables", "");
    m_vars = new QTableWidget(0, 4);
    m_vars->setHorizontalHeaderLabels({"SYMBOL", "VALUE", "UNIT", "STATUS"});
    m_vars->verticalHeader()->hide();
    m_vars->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_vars->setSelectionMode(QAbstractItemView::NoSelection);
    m_vars->setShowGrid(false);
    m_varsPanel->body()->setContentsMargins(12, 0, 12, 8);
    m_varsPanel->body()->addWidget(m_vars);
    right->addWidget(m_varsPanel, 1);
    row->addLayout(right, 6);
    content()->addLayout(row, 1);

    // ---------------------------------------------------------------- keyboard popup (docked bottom-left)
    m_keyboard = new ScientificKeyboard(std::move(keyboardPages), this);
    m_keyboard->setTarget(m_source);
    m_keyboard->setFixedSize(600, 330);
    m_keyboard->hide();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(180);
    connect(m_timer, &QTimer::timeout, this, &EquationLabPage::evaluate);
    connect(m_source, &QPlainTextEdit::textChanged, m_timer, qOverload<>(&QTimer::start));
    connect(m_source, &QPlainTextEdit::cursorPositionChanged, this, [this] {
        const QTextCursor c = m_source->textCursor();
        m_position->setText(QString("Ln %1, Col %2 · UTF-8").arg(c.blockNumber() + 1).arg(c.positionInBlock() + 1));
        m_timer->start();                                            // the active equation is the line under the caret
    });
    connect(m_vars, &QTableWidget::itemChanged, this, [this] { if (!m_updatingTable) m_timer->start(); });
    connect(m_keyboardToggle, &QToolButton::toggled, this, &EquationLabPage::showKeyboard);
    connect(m_keyboard, &ScientificKeyboard::closeRequested, this, [this] { m_keyboardToggle->setChecked(false); });
    connect(m_source, &QPlainTextEdit::textChanged, this, [this] { saveEquations(); });
    evaluate();
}

QString EquationLabPage::resultText() const { return m_resultText; }

QString EquationLabPage::currentEquation() const {
    const QStringList lines = m_source->toPlainText().split('\n');
    const int idx = m_source->textCursor().blockNumber();
    if (idx >= 0 && idx < lines.size() && !lines[idx].trimmed().isEmpty()) return lines[idx].trimmed();
    for (const QString& l : lines) if (!l.trimmed().isEmpty()) return l.trimmed();
    return {};
}

void EquationLabPage::saveEquations() {
    std::vector<std::string> lines;
    for (const QString& l : m_source->toPlainText().split('\n')) if (!l.trimmed().isEmpty()) lines.push_back(l.toStdString());
    m_ctx->log.setEquations(std::move(lines));
}

void EquationLabPage::rebuildTable(const std::vector<std::string>& ids, const std::string& output) {
    m_updatingTable = true;
    for (const auto& id : ids)                                       // remember values; new symbols get defaults
        if (!m_values.count(id)) { const auto d = defaultsFor(id); m_values[id] = {d.first, d.second}; }
    m_vars->setRowCount(static_cast<int>(ids.size()));
    const Theme& t = ThemeManager::instance().theme();
    int known = 0;
    for (int r = 0; r < static_cast<int>(ids.size()); ++r) {
        const std::string& id = ids[static_cast<std::size_t>(r)];
        QString latex;
        try { latex = QString::fromStdString(EquationText::expressionToLatex(id)); } catch (...) { latex = QString::fromStdString(id); }
        auto* sym = new QTableWidgetItem(QString::fromStdString(id));
        sym->setFlags(Qt::ItemIsEnabled);
        sym->setForeground(t.amber);
        auto* val = new QTableWidgetItem(m_values[id].value);
        auto* unit = new QTableWidgetItem(m_values[id].unit);
        QString status;
        if (id == output) { status = "Computed"; val->setForeground(t.dim); }
        else {
            ++known;
            bool ok = false;
            const double v = m_values[id].value.toDouble(&ok);
            status = !ok ? "Known" : (std::fabs(v - std::round(v)) < 1e-9 ? "Known · integer" : (v > 0 ? "Known · > 0" : "Known"));
        }
        auto* st = new QTableWidgetItem(status);
        st->setFlags(Qt::ItemIsEnabled);
        st->setForeground(id == output ? t.lime : t.muted);
        m_vars->setItem(r, 0, sym); m_vars->setItem(r, 1, val); m_vars->setItem(r, 2, unit); m_vars->setItem(r, 3, st);
        m_vars->setRowHeight(r, 30);
    }
    m_varsPanel->setMeta(QString("%1 KNOWN · %2 OUTPUT").arg(known).arg(output.empty() ? 0 : 1));
    m_updatingTable = false;
}

void EquationLabPage::evaluate() {
    // keep what the user typed in the table
    for (int r = 0; r < m_vars->rowCount(); ++r) {
        const std::string id = m_vars->item(r, 0)->text().toStdString();
        m_values[id] = {m_vars->item(r, 1)->text(), m_vars->item(r, 2)->text()};
    }
    const QString line = currentEquation();
    m_rendered->setLatex(line);
    m_result->setLatex({});
    m_substitution->clear();
    m_resultText.clear();
    m_parsedOk = false;

    std::string expr;
    try {
        expr = EquationText::latexToExpression(line.toStdString());
    } catch (const std::exception& e) {
        m_parse->setText("● Syntax: " + QString::fromStdString(e.what()));
        m_parse->setStyleSheet(QString("color: %1;").arg(ThemeManager::instance().theme().error.name()));
        m_renderPanel->setMeta("INVALID");
        rebuildTable({}, "");
        return;
    }
    m_parse->setStyleSheet(QString());
    m_renderPanel->setMeta("VALID");

    const auto ids = ExpressionEvaluator::identifiers(expr);
    const auto eq = expr.find('=');
    std::string lhs = eq == std::string::npos ? expr : expr.substr(0, eq);
    const auto lhsIds = ExpressionEvaluator::identifiers(lhs);
    std::string output;
    if (eq != std::string::npos) {
        if (lhsIds.size() == 1 && lhs == lhsIds[0]) output = lhsIds[0];
        else for (const auto& id : ids) { const auto v = m_values.find(id); if (v != m_values.end() && v->second.value.trimmed().isEmpty()) { output = id; break; } }
    }
    rebuildTable(ids, output);
    m_parse->setText(QString("● Parsed · %1 variable%2 detected").arg(ids.size()).arg(ids.size() == 1 ? "" : "s"));
    m_parsedOk = true;

    try {
        if (eq == std::string::npos) {                               // a bare expression: just evaluate it
            VariableMap vars;
            for (const auto& id : ids) vars[id] = m_values[id].value.toDouble() * Units::toSI(m_values[id].unit.toStdString()).value_or(1.0);
            const double v = ExpressionEvaluator::evaluate(expr, vars);
            m_result->setLatex("=" + shownNumber(v));
            m_resultText = "= " + shownNumber(v);
            return;
        }
        if (output.empty()) { m_substitution->setText("Leave one value empty (or write  y = …) to choose the output."); return; }

        Formula formula;
        formula.id = "custom";
        formula.equation = expr;
        std::map<std::string, std::string> inputs;
        for (const auto& id : ids) {
            Variable v;
            v.id = id; v.display = id; v.name = id;
            formula.variables.push_back(v);
            if (id == output) { inputs[id] = ""; continue; }
            VariableMap none;
            const double base = ExpressionEvaluator::evaluate(m_values[id].value.toStdString(), none);
            const auto scale = Units::toSI(m_values[id].unit.toStdString());
            inputs[id] = numberText(base * scale.value_or(1.0));
        }
        const SolveResult r = SolverEngine().solve(formula, inputs);
        if (!r.ok) { m_substitution->setText("✖ " + QString::fromStdString(r.error)); m_renderPanel->setMeta("NO RESULT"); return; }
        for (const auto& s : r.steps) if (s.rfind("Substitute:", 0) == 0) m_substitution->setText(QString::fromStdString(s.substr(12)));
        const std::string unit = m_values[output].unit.toStdString();
        const double shown = r.value / Units::toSI(unit).value_or(1.0);
        QString symbol;
        try { symbol = QString::fromStdString(EquationText::expressionToLatex(output)); } catch (...) { symbol = QString::fromStdString(output); }
        m_result->setLatex(symbol + "=" + shownNumber(shown) + (unit.empty() ? "" : "\\,\\text{" + QString::fromStdString(unit) + "}"));
        m_resultText = QString("%1 = %2%3").arg(QString::fromStdString(output), shownNumber(shown), unit.empty() ? "" : " " + QString::fromStdString(unit));
    } catch (const std::exception& e) {
        m_substitution->setText("✖ " + QString::fromStdString(e.what()));
        m_renderPanel->setMeta("NO RESULT");
    }
}

void EquationLabPage::showKeyboard(bool visible) {
    m_keyboardToggle->setText(visible ? "Keyboard · open" : "Keyboard · closed");
    if (visible) { placeKeyboard(); m_keyboard->show(); m_keyboard->raise(); } else m_keyboard->hide();
}

void EquationLabPage::placeKeyboard() { m_keyboard->move(24, std::max(60, height() - m_keyboard->height() - 16)); }
void EquationLabPage::resizeEvent(QResizeEvent* e) { BasePage::resizeEvent(e); if (m_keyboard->isVisible()) placeKeyboard(); }


} // namespace phy104
