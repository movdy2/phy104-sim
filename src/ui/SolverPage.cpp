#include "SolverPage.h"
#include "AppContext.h"
#include "FormulaListDelegate.h"
#include "LatexRenderer.h"

#include "ThemeManager.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace phy104 {

SolverPage::SolverPage(std::shared_ptr<FormulaEngine> engine, std::shared_ptr<FormulaManager> manager,
                       std::shared_ptr<SolverEngine> solver, AppContext* context, QWidget* parent)
    : BasePage("Formula Solver", "Search equations and solve for one unknown.", context, parent),
      m_engine(std::move(engine)), m_manager(std::move(manager)), m_solver(std::move(solver)) {

    // ------------------------------------------------------------ left: formula browser
    auto* browser = new QFrame;
    browser->setObjectName("card");
    browser->setFixedWidth(390);

    auto* browserTitle = new QLabel("Formulas");
    browserTitle->setObjectName("sectionTitle");

    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search by name or ID…");
    m_search->setClearButtonEnabled(true);

    m_chapter = new QComboBox;

    m_list = new QListWidget;
    m_list->setItemDelegate(new FormulaListDelegate(m_list));
    m_list->setMouseTracking(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet("QListWidget { background: transparent; border: none; } QListWidget::item { border: none; }");

    m_pageSize = new QComboBox;
    for (int n : {3, 5, 10, 20}) m_pageSize->addItem(QString::number(n), n);
    m_pageSize->setCurrentIndex(1);                                // 5 equations on screen by default
    m_pageSize->setToolTip("How many equations are shown at the same time");

    m_prev = new QPushButton("◀");
    m_next = new QPushButton("▶");
    m_prev->setFixedWidth(40);
    m_next->setFixedWidth(40);
    m_pageLabel = new QLabel;
    m_pageLabel->setObjectName("muted");
    m_pageLabel->setAlignment(Qt::AlignCenter);

    auto* limitRow = new QHBoxLayout;
    auto* limitLabel = new QLabel("Equations on screen");
    limitLabel->setObjectName("muted");
    limitRow->addWidget(limitLabel);
    limitRow->addStretch();
    limitRow->addWidget(m_pageSize);

    auto* pagerRow = new QHBoxLayout;
    pagerRow->addWidget(m_prev);
    pagerRow->addWidget(m_pageLabel, 1);
    pagerRow->addWidget(m_next);

    auto* bl = new QVBoxLayout(browser);
    bl->setContentsMargins(12, 12, 12, 12);
    bl->addWidget(browserTitle);
    bl->addWidget(m_search);
    bl->addWidget(m_chapter);
    bl->addLayout(limitRow);
    bl->addWidget(m_list, 1);
    bl->addLayout(pagerRow);

    // ------------------------------------------------------------ right: solver
    m_formulaName = new QLabel;
    m_formulaName->setObjectName("sectionTitle");
    m_formulaChapter = new QLabel;
    m_formulaChapter->setObjectName("muted");
    m_formulaLatex = new LatexWidget;
    m_formulaLatex->setLcdStyle(true);                             // formula on a Casio LCD
    m_formulaLatex->setPointSize(24);

    auto* hint = new QLabel("Fill in the known values and leave exactly <b>one</b> field empty. "
                            "Angles in degrees, lengths in metres (type 550*nm or 0.5*mm).");
    hint->setObjectName("muted");
    hint->setWordWrap(true);

    m_form = new QFormLayout;
    m_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* solveBtn = new QPushButton("Solve");
    solveBtn->setObjectName("primary");
    auto* clearBtn = new QPushButton("Clear");
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(solveBtn);
    buttons->addWidget(clearBtn);
    buttons->addStretch();

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();

    m_answer = new LatexWidget;
    m_answer->setLcdStyle(true);
    m_answer->setPointSize(26);
    m_answer->hide();

    m_output = new QTextBrowser;
    m_output->setObjectName("lcdScreen");
    ThemeManager::polishLcdFont(m_output);
    m_output->setMinimumHeight(140);

    auto* right = new QVBoxLayout;
    right->addWidget(m_formulaName);
    right->addWidget(m_formulaChapter);
    right->addWidget(m_formulaLatex);
    right->addWidget(hint);
    right->addLayout(m_form);
    right->addLayout(buttons);
    right->addWidget(m_status);
    right->addWidget(m_answer);
    right->addWidget(m_output, 1);

    auto* root = new QHBoxLayout;
    root->addWidget(browser);
    root->addLayout(right, 1);
    content()->addLayout(root, 1);

    connect(m_search, &QLineEdit::textChanged, this, &SolverPage::onSearchChanged);
    connect(m_chapter, &QComboBox::currentIndexChanged, this, &SolverPage::onChapterChanged);
    connect(m_pageSize, &QComboBox::currentIndexChanged, this, &SolverPage::onPageSizeChanged);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &SolverPage::onItemSelected);
    connect(m_prev, &QPushButton::clicked, this, &SolverPage::prevPage);
    connect(m_next, &QPushButton::clicked, this, &SolverPage::nextPage);
    connect(solveBtn, &QPushButton::clicked, this, &SolverPage::onSolve);
    connect(clearBtn, &QPushButton::clicked, this, &SolverPage::onClear);

    refreshChapterCombo();
    refreshList(true);
    if (m_list->count() > 0) {                                     // select the first real formula
        for (int i = 0; i < m_list->count(); ++i) { m_list->setCurrentRow(i); break; }
    }
}

// ------------------------------------------------------------------ browser logic
void SolverPage::refreshChapterCombo() {
    const std::string query = m_search->text().toStdString();
    const auto counts = m_manager->countByChapter(query);
    const QString previous = m_chapter->currentData().toString();

    QSignalBlocker block(m_chapter);
    m_chapter->clear();
    int total = 0;
    for (const auto& [id, n] : counts) total += n;
    m_chapter->addItem(QString("All chapters (%1)").arg(total), QString());
    for (const Chapter& c : m_manager->chapters()) {
        const auto it = counts.find(c.id);
        const int n = it == counts.end() ? 0 : it->second;
        m_chapter->addItem(QString("%1 (%2)").arg(QString::fromStdString(c.title)).arg(n),
                           QString::fromStdString(c.id));
    }
    const int idx = m_chapter->findData(previous);
    m_chapter->setCurrentIndex(idx >= 0 ? idx : 0);
}

void SolverPage::refreshList(bool resetPage) {
    m_results = m_manager->search(m_search->text().toStdString(), m_chapter->currentData().toString().toStdString());
    if (resetPage) m_pageIndex = 0;
    const FormulaPage page = FormulaManager::paginate(m_results, m_pageIndex, m_pageSize->currentData().toInt());
    m_pageIndex = page.pageIndex;

    const QString keepId = m_current ? QString::fromStdString(m_current->id) : QString();
    QSignalBlocker block(m_list);
    m_list->clear();
    std::string lastChapter;
    int reselect = -1;
    for (const Formula* f : page.items) {
        auto* item = new QListWidgetItem(QString::fromStdString(f->name));
        item->setData(FormulaListDelegate::IdRole, QString::fromStdString(f->id));
        item->setData(FormulaListDelegate::ChapterRole,
                      f->chapter != lastChapter ? QString::fromStdString(m_manager->chapterTitle(f->chapter)) : QString());
        item->setData(FormulaListDelegate::LatexRole,
                      QString::fromStdString(f->latex.empty() ? f->equation : f->latex));
        lastChapter = f->chapter;
        m_list->addItem(item);
        if (QString::fromStdString(f->id) == keepId) reselect = m_list->count() - 1;
    }
    if (reselect >= 0) m_list->setCurrentRow(reselect);

    m_pageLabel->setText(page.total == 0
        ? QString("No formulas found")
        : QString("Page %1 / %2  ·  %3 formulas").arg(page.pageIndex + 1).arg(page.pageCount).arg(page.total));
    m_prev->setEnabled(page.pageIndex > 0);
    m_next->setEnabled(page.pageIndex + 1 < page.pageCount);
}

void SolverPage::onSearchChanged()   { refreshChapterCombo(); refreshList(true); }
void SolverPage::onChapterChanged()  { refreshList(true); }
void SolverPage::onPageSizeChanged() { refreshList(true); }
void SolverPage::prevPage()          { --m_pageIndex; refreshList(false); }
void SolverPage::nextPage()          { ++m_pageIndex; refreshList(false); }

void SolverPage::onItemSelected() {
    const auto items = m_list->selectedItems();
    if (items.isEmpty()) return;
    const Formula* f = m_engine->findById(items.first()->data(FormulaListDelegate::IdRole).toString().toStdString());
    if (f && f != m_current) showFormula(*f);
}

void SolverPage::selectFormula(const QString& id) {
    const Formula* f = m_engine->findById(id.toStdString());
    if (!f) return;
    m_search->clear();
    QSignalBlocker block(m_chapter);
    m_chapter->setCurrentIndex(0);
    refreshChapterCombo();
    m_results = m_manager->search("", "");
    const int size = m_pageSize->currentData().toInt();
    for (std::size_t i = 0; i < m_results.size(); ++i)
        if (m_results[i] == f) { m_pageIndex = static_cast<int>(i) / size; break; }
    m_current = f;
    refreshList(false);
    showFormula(*f);
}

// ------------------------------------------------------------------ solver logic
void SolverPage::showFormula(const Formula& formula) {
    m_current = &formula;
    m_formulaName->setText(QString::fromStdString(formula.name));
    m_formulaChapter->setText(QString::fromStdString(m_manager->chapterTitle(formula.chapter)));
    m_formulaLatex->setLatex(QString::fromStdString(formula.latex.empty() ? formula.equation : formula.latex));

    while (m_form->rowCount() > 0) m_form->removeRow(0);          // deletes the old widgets
    m_edits.clear();
    for (const auto& var : formula.variables) {
        auto* edit = new QLineEdit(QString::fromStdString(var.defaultValue));
        edit->setObjectName("lcdInput");                          // Casio LCD style
        ThemeManager::polishLcdInput(edit);
        const auto ov = formula.unitOverrides.find(var.id);
        const std::string unit = ov != formula.unitOverrides.end() ? ov->second : var.unit;
        edit->setPlaceholderText(unit.empty() ? "number" : QString::fromStdString(unit));
        edit->setToolTip(QString::fromStdString(var.name + (var.description.empty() ? "" : " - " + var.description)));
        edit->setMinimumWidth(220);
        connect(edit, &QLineEdit::returnPressed, this, &SolverPage::onSolve);
        m_edits[var.id] = edit;

        auto* label = new QLabel(QString::fromStdString(var.display) + "  <span style='color:gray'>" +
                                 QString::fromStdString(var.name).toHtmlEscaped() + "</span>");
        label->setTextFormat(Qt::RichText);
        m_form->addRow(label, edit);
    }
    setStatus("", "");
    m_answer->hide();
    m_output->clear();
}

void SolverPage::setStatus(const QString& kind, const QString& text) {
    if (kind.isEmpty()) { m_status->hide(); return; }
    m_status->setObjectName("status" + kind);
    m_status->setText(text);
    m_status->style()->unpolish(m_status);                        // re-apply the style sheet for the new objectName
    m_status->style()->polish(m_status);
    m_status->show();
}

void SolverPage::onSolve() {
    if (!m_current) return;
    std::map<std::string, std::string> inputs;
    for (const auto& [id, edit] : m_edits) inputs[id] = edit->text().toStdString();

    const SolveResult r = m_solver->solve(*m_current, inputs);
    m_answer->hide();
    if (!r.ok) {
        const QString message = QString::fromStdString(r.error);
        if (r.physical) {                                         // physics warning: orange
            const QString hint = QString::fromStdString(r.hint);
            setStatus("Warn", "⚠ " + (hint.isEmpty() ? message : hint));
        } else {                                                  // input mistake: red
            setStatus("Error", "✖ " + message);
        }
        m_output->setHtml("<p>" + message.toHtmlEscaped() + "</p>");
        return;
    }

    setStatus("Ok", "✔ Solved   ·   check: " + QString::fromStdString(r.checkSummary));
    m_answer->setLatex(QString::fromStdString(r.answerLatex));
    m_answer->show();

    QString html;
    for (const auto& step : r.steps) {
        if (step.rfind("Answer:", 0) == 0) continue;              // the big LCD answer shows it
        html += "<p>" + QString::fromStdString(step).toHtmlEscaped().replace("\n", "<br>") + "</p>";
    }
    m_output->setHtml(html);
}

void SolverPage::onClear() {
    if (m_current) showFormula(*m_current);
}

} // namespace phy104
