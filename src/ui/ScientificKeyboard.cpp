#include "ScientificKeyboard.h"
#include "Components.h"
#include "LatexRenderer.h"
#include "ThemeManager.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

namespace phy104 {

int ScientificKeyboard::caretOffset(const QString& latex) {
    int i = latex.indexOf("{}");
    if (i >= 0) return i + 1;
    i = latex.indexOf("()");
    if (i >= 0) return i + 1;
    i = latex.indexOf("\\right|");
    if (i >= 0) return i;
    return latex.size();
}

ScientificKeyboard::ScientificKeyboard(std::vector<ScientificPage> pages, QWidget* parent) : QFrame(parent), m_pages(std::move(pages)) {
    setObjectName("sciKeyboard");
    setFocusPolicy(Qt::StrongFocus);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(8);

    auto* head = new QHBoxLayout;
    auto* title = new QLabel("PHY104 / SCIENTIFIC INPUT");
    title->setObjectName("sciTitle");
    auto* mode = new QLabel("DEG");
    mode->setObjectName("caption");
    auto* close = new QToolButton;
    close->setObjectName("keyClose");
    close->setText("×");
    close->setFocusPolicy(Qt::NoFocus);
    close->setToolTip("Close (Esc)");
    head->addWidget(title);
    head->addStretch();
    head->addWidget(mode);
    head->addSpacing(8);
    head->addWidget(close);
    root->addLayout(head);

    auto* lcd = new QFrame;                                   // pending token + insertion target
    lcd->setObjectName("lcd");
    auto* ll = new QHBoxLayout(lcd);
    ll->setContentsMargins(10, 6, 10, 6);
    m_display = new QLabel;
    m_display->setObjectName("lcdKeyDisplay");
    m_caption = new QLabel;
    m_caption->setObjectName("lcdCaption");
    ll->addWidget(m_display, 1);
    ll->addWidget(m_caption, 0, Qt::AlignRight | Qt::AlignVCenter);
    root->addWidget(lcd);

    QStringList names;
    for (const auto& p : m_pages) names << QString::fromStdString(p.name);
    m_tabs = new ModeTabs(names);
    root->addWidget(m_tabs);

    m_stack = new QStackedWidget;
    for (const ScientificPage& page : m_pages) {
        auto* w = new QWidget;
        auto* grid = new QGridLayout(w);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(6);
        const bool useLatex = page.name == "Variables";
        for (std::size_t r = 0; r < page.rows.size(); ++r)
            for (std::size_t c = 0; c < page.rows[r].size(); ++c) {
                const ScientificKey& key = page.rows[r][c];
                auto* b = new QPushButton;
                b->setObjectName(key.control.empty() ? "rkey" : "rkeyCtl");
                b->setFocusPolicy(Qt::NoFocus);                    // the source editor keeps the caret
                b->setCursor(Qt::PointingHandCursor);
                b->setMinimumHeight(30);
                b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                b->setProperty("legend", QString::fromStdString(key.legend));
                b->setProperty("latex", QString::fromStdString(key.latex));
                b->setProperty("shiftLegend", QString::fromStdString(key.shiftLegend));
                b->setProperty("shiftLatex", QString::fromStdString(key.shiftLatex));
                b->setProperty("control", QString::fromStdString(key.control));
                if (!key.control.empty()) {
                    static const std::map<std::string, QString> legends = {{"@shift", "SHIFT"}, {"@left", "←"}, {"@right", "→"}, {"@back", "⌫"}, {"@clear", "AC"}};
                    b->setText(legends.at(key.control));
                    b->setToolTip(key.control == "@clear" ? "Clear the keyboard buffer (not the equation)" : QString());
                } else if (useLatex) {
                    QFont f; f.setPixelSize(15);
                    b->setIcon(QIcon(LatexRenderer::render(QString::fromStdString(key.latex), f, ThemeManager::instance().theme().text, 2.0)));
                    b->setIconSize(QSize(34, 20));
                    b->setToolTip(QString::fromStdString(key.legend));
                } else {
                    b->setText(QString::fromStdString(key.legend));
                }
                connect(b, &QPushButton::clicked, this, [this, b] {
                    const QString control = b->property("control").toString();
                    if (control == "@shift") { m_shift = !m_shift; b->setProperty("latched", m_shift); b->style()->unpolish(b); b->style()->polish(b); applyShift(); return; }
                    if (control == "@clear") { m_pending.clear(); m_caret = -1; m_tokenLengths.clear(); refreshDisplay(); return; }
                    if (control == "@left" || control == "@right") {
                        if (!m_target) return;
                        QTextCursor c = m_target->textCursor();
                        c.movePosition(control == "@left" ? QTextCursor::PreviousCharacter : QTextCursor::NextCharacter);
                        m_target->setTextCursor(c);
                        return;
                    }
                    if (control == "@back") {
                        if (!m_tokenLengths.isEmpty()) {                       // remove the last pending token
                            m_pending.chop(m_tokenLengths.takeLast());
                            if (m_caret > m_pending.size()) m_caret = -1;
                            refreshDisplay();
                        } else if (m_target) {                                  // delete the selection or the previous character of the source
                            QTextCursor c = m_target->textCursor();
                            if (c.hasSelection()) c.removeSelectedText(); else c.deletePreviousChar();
                            m_target->setTextCursor(c);
                        }
                        return;
                    }
                    const bool shifted = m_shift && !b->property("shiftLatex").toString().isEmpty();
                    const QString latex = shifted ? b->property("shiftLatex").toString() : b->property("latex").toString();
                    if (m_caret < 0 && (latex.contains("{}") || latex.contains("()") || latex.contains("\\\\right|"))) m_caret = m_pending.size() + caretOffset(latex);
                    m_pending += latex;
                    m_tokenLengths << latex.size();
                    refreshDisplay();
                });
                grid->addWidget(b, static_cast<int>(r), static_cast<int>(c));
            }
        for (int c = 0; c < 10; ++c) grid->setColumnStretch(c, 1);
        m_stack->addWidget(w);
    }
    root->addWidget(m_stack);

    m_insert = new QPushButton("INSERT / ENTER  ↵");
    m_insert->setObjectName("primary");
    m_insert->setFocusPolicy(Qt::NoFocus);
    m_insert->setMinimumHeight(30);
    root->addWidget(m_insert, 0, Qt::AlignLeft);

    connect(m_tabs, &ModeTabs::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_insert, &QPushButton::clicked, this, &ScientificKeyboard::insertPending);
    connect(close, &QToolButton::clicked, this, &ScientificKeyboard::closeRequested);
    refreshDisplay();
}

void ScientificKeyboard::setTarget(QPlainTextEdit* target) {
    m_target = target;
    if (target) connect(target, &QPlainTextEdit::cursorPositionChanged, this, &ScientificKeyboard::updateCaption, Qt::UniqueConnection);
    updateCaption();
}

void ScientificKeyboard::updateCaption() {
    QString where = "no source";
    if (m_target) { const QTextCursor c = m_target->textCursor(); where = QString("Ln %1, Col %2").arg(c.blockNumber() + 1).arg(c.positionInBlock() + 1); }
    m_caption->setText("INSERT → SOURCE · " + where);
}

void ScientificKeyboard::refreshDisplay() {
    QString shown = m_pending;
    shown.insert(m_caret >= 0 ? std::min<qsizetype>(m_caret, shown.size()) : shown.size(), QChar(0x2502));     // caret bar
    m_display->setText(shown);
    m_insert->setEnabled(!m_pending.isEmpty() && m_target);
}

void ScientificKeyboard::applyShift() {                           // SHIFT: capital Greek letters on the Greek page
    for (auto* b : findChildren<QPushButton*>("rkey")) {
        const QString capital = b->property("shiftLegend").toString();
        if (capital.isEmpty()) continue;
        b->setText(m_shift ? capital : b->property("legend").toString());
    }
}

void ScientificKeyboard::insertPending() {
    if (!m_target || m_pending.isEmpty()) return;
    QTextCursor c = m_target->textCursor();
    c.beginEditBlock();
    c.insertText(m_pending);                                       // replaces the selection
    const int end = c.position();
    c.setPosition(end - m_pending.size() + (m_caret >= 0 ? m_caret : m_pending.size()));
    c.endEditBlock();
    m_target->setTextCursor(c);
    m_target->setFocus();                                          // focus stays in the source editor
    const QString text = m_pending;
    m_pending.clear();
    m_caret = -1;
    m_tokenLengths.clear();
    refreshDisplay();
    emit inserted(text);
}

void ScientificKeyboard::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { emit closeRequested(); return; }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { insertPending(); return; }
    QFrame::keyPressEvent(e);
}

} // namespace phy104
