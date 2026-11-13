#pragma once
#include "Models.h"

#include <QFrame>
#include <QPointer>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

namespace phy104 {

class ModeTabs;

/// The "Scientific input" keyboard of the design: raised Casio-style keys in categories (Greek symbols / Math operators /
/// Functions / Variables). A key adds a LaTeX token to the pending buffer shown on the LCD; INSERT / ENTER commits the buffer
/// into the source editor at the caret (replacing the selection) and puts the caret inside the first {} or ().
///  ← / → move the SOURCE caret, ⌫ deletes the last pending token (or the selection / previous character of the source),
///  AC clears the pending buffer only, SHIFT latches capital Greek letters, Esc closes without inserting.
class ScientificKeyboard : public QFrame {
    Q_OBJECT
public:
    explicit ScientificKeyboard(std::vector<ScientificPage> pages, QWidget* parent = nullptr);

    void setTarget(QPlainTextEdit* target);
    QString pending() const { return m_pending; }
    bool shiftLatched() const { return m_shift; }
    /// Index inside `latex` where the caret goes after inserting: inside the first {} or (), between \left| and \right|, else the end.
    static int caretOffset(const QString& latex);

signals:
    void inserted(const QString& latex);
    void closeRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void insertPending();
    void refreshDisplay();
    void handleControl(const QString& control, QPushButton* button = nullptr);
    void applyShift();
    void updateCaption();

    std::vector<ScientificPage> m_pages;
    QPointer<QPlainTextEdit> m_target;
    ModeTabs* m_tabs;
    QStackedWidget* m_stack;
    QLabel *m_display, *m_caption;
    QPushButton* m_insert = nullptr;
    QPushButton* m_shiftButton = nullptr;
    QString m_pending;
    int m_caret = -1;
    QList<int> m_tokenLengths;
    bool m_shift = false;
};

} // namespace phy104
