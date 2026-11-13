#pragma once
#include "BasePage.h"

#include <QString>
#include <map>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QTableWidget;
class QTimer;
class QToolButton;

namespace phy104 {

class LatexWidget;
class Panel;
class ScientificKeyboard;

/// Equation Lab: write an equation in LaTeX, see it typeset, and evaluate it.
///  source editor  -> LaTeX -> plain math (EquationText) -> variables table -> SolverEngine
/// The variable left empty (or the single symbol on the left of "=") is the output.
class EquationLabPage : public BasePage {
    Q_OBJECT
public:
    EquationLabPage(AppContext* context, std::vector<struct ScientificPage> keyboardPages, QWidget* parent = nullptr);

    QPlainTextEdit* source() const { return m_source; }
    ScientificKeyboard* keyboard() const { return m_keyboard; }
    QTableWidget* variablesTable() const { return m_vars; }
    bool parsed() const { return m_parsedOk; }
    QString resultText() const;                 // the LCD result as plain text, e.g. "r_m = 1.5492 mm"

public slots:
    void evaluate();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct VarState { QString value; QString unit; };
    QString currentEquation() const;
    void rebuildTable(const std::vector<std::string>& ids, const std::string& output);
    void showKeyboard(bool visible);
    void placeKeyboard();
    void saveEquations();

    QPlainTextEdit* m_source;
    LatexWidget* m_rendered;
    LatexWidget* m_result;
    QLabel *m_position, *m_parse, *m_substitution;
    Panel *m_sourcePanel, *m_renderPanel, *m_varsPanel;
    QTableWidget* m_vars;
    QToolButton* m_keyboardToggle;
    ScientificKeyboard* m_keyboard;
    QTimer* m_timer;
    std::map<std::string, VarState> m_values;
    QString m_resultText;
    bool m_parsedOk = false;
    bool m_updatingTable = false;
};

} // namespace phy104
