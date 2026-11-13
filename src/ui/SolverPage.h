#pragma once
#include "FormulaEngine.h"
#include "FormulaManager.h"
#include "SolverEngine.h"
#include <QWidget>
#include <map>
#include <memory>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;

namespace phy104 {

class LatexWidget;

/// Formula Solver page.
///  left : search box, chapter filter, paged list of formula cards (screen limit)
///  right: LaTeX formula on an LCD, LCD input fields, status line, LCD answer + steps
class SolverPage : public QWidget {
    Q_OBJECT
public:
    SolverPage(std::shared_ptr<FormulaEngine> engine, std::shared_ptr<FormulaManager> manager,
               std::shared_ptr<SolverEngine> solver, QWidget* parent = nullptr);

public slots:
    /// Selects a formula by id (also makes sure it is visible in the list).
    void selectFormula(const QString& id);

private slots:
    void onSearchChanged();
    void onChapterChanged();
    void onPageSizeChanged();
    void onItemSelected();
    void prevPage();
    void nextPage();
    void onSolve();
    void onClear();

private:
    void refreshList(bool resetPage);
    void refreshChapterCombo();
    void showFormula(const Formula& formula);
    void setStatus(const QString& kind, const QString& text);   // kind: Ok | Warn | Error | "" (hide)

    std::shared_ptr<FormulaEngine> m_engine;
    std::shared_ptr<FormulaManager> m_manager;
    std::shared_ptr<SolverEngine> m_solver;
    const Formula* m_current = nullptr;

    // browser (left)
    QLineEdit* m_search;
    QComboBox* m_chapter;
    QComboBox* m_pageSize;
    QListWidget* m_list;
    QPushButton* m_prev;
    QPushButton* m_next;
    QLabel* m_pageLabel;
    std::vector<const Formula*> m_results;
    int m_pageIndex = 0;

    // solver (right)
    QLabel* m_formulaName;
    QLabel* m_formulaChapter;
    LatexWidget* m_formulaLatex;
    QFormLayout* m_form;
    std::map<std::string, QLineEdit*> m_edits;
    QLabel* m_status;
    LatexWidget* m_answer;
    QTextBrowser* m_output;
};

} // namespace phy104
