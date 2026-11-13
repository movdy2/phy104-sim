#include "ScientificKeyboard.h"
#include "Components.h"
#include "LatexRenderer.h"
#include "ThemeManager.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace phy104 {

qsizetype ScientificKeyboard::caretOffset(const QString& latex) {
    qsizetype i = latex.indexOf("{}");
    if (i >= 0) return i + 1;
    i = latex.indexOf("()");
    if (i >= 0) return i + 1;
    i = latex.indexOf("\\right|");
    if (i >= 0) return i;
    return latex.size();
}

ScientificKeyboard::ScientificKeyboard(std::vector<ScientificPage> pages, QWidget* parent)
    : QFrame(parent), m_pages(std::move(pages)) {
    setObjectName("sciKeyboard");
    setFocusPolicy(Qt::StrongFocus);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 6);
    root->setSpacing(4);

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
    ll->setContentsMargins(8, 4, 8, 4);
    m_display = new QLabel;
    m_display->setObjectName("lcdKeyDisplay");
    ThemeManager::polishLcdFont(m_display);
    m_caption = new QLabel;
    m_caption->setObjectName("lcdCaption");
    ThemeManager::polishLcdFont(m_caption);
    ll->addWidget(m_display, 1);
    ll->addWidget(m_caption, 0, Qt::AlignRight | Qt::AlignVCenter);
    root->addWidget(lcd);

    QStringList names;
    for (const auto& p : m_pages) names << QString::fromStdString(p.name);
    m_tabs = new ModeTabs(names);
    root->addWidget(m_tabs);

    m_stack = new QStackedWidget;
    m_stack->setMinimumHeight(0);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    for (const ScientificPage& page : m_pages) {
        auto* w = new QWidget;
        auto* grid = new QGridLayout(w);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(3);
        const bool useLatex = page.name == "Variables";
        for (std::size_t r = 0; r < page.rows.size(); ++r) {
            for (std::size_t c = 0; c < page.rows[r].size(); ++c) {
                const ScientificKey& key = page.rows[r][c];
                if (!key.control.empty()) continue;                   // editing actions are shared below, not repeated per page

                auto* b = new QPushButton;
                b->setObjectName("rkey");
                b->setFocusPolicy(Qt::NoFocus);                       // the source editor keeps the caret
                b->setCursor(Qt::PointingHandCursor);
                b->setMinimumHeight(22);
                b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                b->setProperty("legend", QString::fromStdString(key.legend));
                b->setProperty("latex", QString::fromStdString(key.latex));
                b->setProperty("shiftLegend", QString::fromStdString(key.shiftLegend));
                b->setProperty("shiftLatex", QString::fromStdString(key.shiftLatex));
                if (useLatex) {
                    QFont f(ThemeManager::monoFont());
                    f.setPixelSize(15);
                    b->setIcon(QIcon(LatexRenderer::render(QString::fromStdString(key.latex), f,
                                                           ThemeManager::instance().theme().text, 2.0)));
                    b->setIconSize(QSize(30, 16));
                    b->setToolTip(QString::fromStdString(key.legend));
                } else {
                    b->setText(QString::fromStdString(key.legend));
                }
                connect(b, &QPushButton::clicked, this, [this, b] {
                    const QString shiftLatex = b->property("shiftLatex").toString();
                    const QString latex = m_shift && !shiftLatex.isEmpty()
                                              ? shiftLatex : b->property("latex").toString();
                    if (m_caret < 0 && (latex.contains("{}") || latex.contains("()") || latex.contains("\\right|")))
                        m_caret = m_pending.size() + caretOffset(latex);
                    m_pending += latex;
                    m_tokenLengths << latex.size();
                    refreshDisplay();
                });
                grid->addWidget(b, static_cast<int>(r), static_cast<int>(c));
            }
        }
        for (int c = 0; c < 10; ++c) grid->setColumnStretch(c, 1);
        m_stack->addWidget(w);
    }
    root->addWidget(m_stack, 1);

    auto* controls = new QHBoxLayout;
    controls->setSpacing(4);
    auto makeControl = [this, controls](const QString& label, const QString& command, const QString& tip,
                                        int width) {
        auto* button = new QPushButton(label);
        button->setObjectName("rkeyCtl");
        button->setProperty("control", command);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumSize(width, 22);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        button->setToolTip(tip);
        controls->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, command, button] { handleControl(command, button); });
        return button;
    };
    m_shiftButton = makeControl("SHIFT", "@shift", "Toggle capital Greek letters", 56);
    makeControl("←", "@left", "Move the source caret left", 38);
    makeControl("→", "@right", "Move the source caret right", 38);
    makeControl("⌫", "@back", "Delete the last key or source character", 38);
    makeControl("AC", "@clear", "Clear the pending keyboard buffer", 42);
    controls->addStretch(1);
    root->addLayout(controls);

    m_insert = new QPushButton("INSERT / ENTER  ↵");
    m_insert->setObjectName("primary");
    m_insert->setFocusPolicy(Qt::NoFocus);
    m_insert->setMinimumHeight(26);
    root->addWidget(m_insert, 0, Qt::AlignLeft);

    m_shiftButton->setEnabled(m_tabs->current() == 0);
    connect(m_tabs, &ModeTabs::currentChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_tabs, &ModeTabs::currentChanged, this, [this](int index) {
        m_shiftButton->setEnabled(index == 0);
    });
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

void ScientificKeyboard::handleControl(const QString& control, QPushButton* button) {
    if (control == "@shift") {
        m_shift = !m_shift;
        if (button) {
            button->setProperty("latched", m_shift);
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
        applyShift();
        return;
    }
    if (control == "@clear") {
        m_pending.clear();
        m_caret = -1;
        m_tokenLengths.clear();
        refreshDisplay();
        return;
    }
    if (control == "@left" || control == "@right") {
        if (!m_target) return;
        QTextCursor c = m_target->textCursor();
        c.movePosition(control == "@left" ? QTextCursor::PreviousCharacter : QTextCursor::NextCharacter);
        m_target->setTextCursor(c);
        return;
    }
    if (control == "@back") {
        if (!m_tokenLengths.isEmpty()) {                         // remove the last pending token
            m_pending.chop(m_tokenLengths.takeLast());
            if (m_caret > m_pending.size()) m_caret = -1;
            refreshDisplay();
        } else if (m_target) {                                    // delete the selection or previous source character
            QTextCursor c = m_target->textCursor();
            if (c.hasSelection()) c.removeSelectedText(); else c.deletePreviousChar();
            m_target->setTextCursor(c);
        }
    }
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
    const qsizetype end = c.position();
    const qsizetype caret = m_caret >= 0 ? m_caret : m_pending.size();
    c.setPosition(static_cast<int>(end - m_pending.size() + caret));
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
