#include "NotesPage.h"
#include "AppContext.h"
#include "Components.h"
#include "MarkdownWorkspace.h"
#include "ScientificKeyboard.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace phy104 {
namespace {
const char* const kStarterNotes =
    "# Lab notes\n\n## Refraction & TIR\n\n### Snell's law\n- $n₁ sin θ₁ = n₂ sin θ₂$\n- compare the fitted index with the reference value\n\n"
    "#### Exam warning\nAngles are measured from the normal, not from the surface.\n";
}

NotesPage::NotesPage(AppContext* ctx, std::vector<ScientificPage> keyboardPages, QWidget* parent)
    : BasePage("Notes", "MARKDOWN STUDY WORKSPACE / Write, edit and preview your lab notes", ctx, parent) {
    // One panel across the whole window: the editor, its toolbar and the live preview, nothing else.
    m_notesPanel = new Panel("Session notes", "SAVED " + ctx->lastSavedTime());
    m_notes = new MarkdownWorkspace;
    const QString saved = QString::fromStdString(ctx->log.notes());
    m_notes->setMarkdown(saved.isEmpty() ? QString::fromUtf8(kStarterNotes) : saved);

    // the same scientific keyboard as the Equation Lab, docked under the note and writing LaTeX into it
    m_keyboard = new ScientificKeyboard(std::move(keyboardPages), this);
    m_keyboard->setTarget(m_notes->editor());
    m_keyboard->setFixedSize(600, 330);
    m_keyboard->hide();
    m_keyboardToggle = new QToolButton;
    m_keyboardToggle->setObjectName("notesKeyboard");
    m_keyboardToggle->setText("⌨ Keyboard");
    m_keyboardToggle->setCheckable(true);
    m_keyboardToggle->setToolTip("Scientific keyboard: types LaTeX formulas straight into the note");
    auto* bar = new QHBoxLayout;
    bar->addStretch();
    bar->addWidget(m_keyboardToggle);

    m_notesPanel->body()->setContentsMargins(12, 0, 12, 10);
    m_notesPanel->body()->addLayout(bar);
    m_notesPanel->body()->addWidget(m_notes, 1);
    m_notesPanel->body()->addWidget(m_keyboard, 0, Qt::AlignLeft);
    content()->addWidget(m_notesPanel, 1);

    connect(m_keyboardToggle, &QToolButton::toggled, this, &NotesPage::toggleKeyboard);
    connect(m_keyboard, &ScientificKeyboard::closeRequested, this, [this] { m_keyboardToggle->setChecked(false); });

    // autosave: the workspace reports its content ~0.7 s after the last keystroke; the context stores it in the
    // session log (and into every snapshot's notesContent) and writes the .phy file
    connect(m_notes, &MarkdownWorkspace::contentChanged, ctx, &AppContext::setNotes);
    connect(ctx, &AppContext::notesAppended, this, [this] { m_notes->setMarkdown(QString::fromStdString(m_ctx->log.notes())); });
    connect(ctx, &AppContext::notesSaved, this, [this] {
        m_notesPanel->setMeta("SAVED " + m_ctx->lastSavedTime());
        m_notes->setSavedText("Saved " + m_ctx->lastSavedTime());
    });
}

void NotesPage::toggleKeyboard(bool on) {
    m_keyboard->setVisible(on);
    m_keyboardToggle->setText(on ? "⌨ Keyboard · open" : "⌨ Keyboard");
    if (!on) return;
    // the keyboard inserts LaTeX tokens, so put the caret inside a formula:  $…$
    m_notes->editor()->insertSnippet("$%1$");
    m_keyboard->setTarget(m_notes->editor());
    m_keyboard->setFocus();
}

} // namespace phy104
