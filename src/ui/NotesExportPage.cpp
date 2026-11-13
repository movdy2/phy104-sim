#include "NotesExportPage.h"
#include "AppContext.h"
#include "Components.h"
#include "MarkdownWorkspace.h"

namespace phy104 {
namespace {
const char* const kStarterNotes =
    "# Lab notes\n\n## Refraction & TIR\n\n### Snell's law\n- $n₁ sin θ₁ = n₂ sin θ₂$\n- compare the fitted index with the reference value\n\n"
    "#### Exam warning\nAngles are measured from the normal, not from the surface.\n";
}

NotesExportPage::NotesExportPage(AppContext* ctx, QWidget* parent)
    : BasePage("Notes", "MARKDOWN STUDY WORKSPACE / Write, edit and preview your lab notes", ctx, parent) {
    // One panel across the whole window: the editor, its toolbar and the live preview, nothing else.
    m_notesPanel = new Panel("Session notes", "SAVED " + ctx->lastSavedTime());
    m_notes = new MarkdownWorkspace;
    const QString saved = QString::fromStdString(ctx->log.notes());
    m_notes->setMarkdown(saved.isEmpty() ? QString::fromUtf8(kStarterNotes) : saved);
    m_notesPanel->body()->setContentsMargins(12, 0, 12, 10);
    m_notesPanel->body()->addWidget(m_notes, 1);
    content()->addWidget(m_notesPanel, 1);

    // autosave: the workspace reports its content ~0.7 s after the last keystroke; the context stores it in the
    // session log (and into every snapshot's notesContent) and writes the .phy file
    connect(m_notes, &MarkdownWorkspace::contentChanged, ctx, &AppContext::setNotes);
    connect(ctx, &AppContext::notesAppended, this, [this] { m_notes->setMarkdown(QString::fromStdString(m_ctx->log.notes())); });
    connect(ctx, &AppContext::notesSaved, this, [this] {
        m_notesPanel->setMeta("SAVED " + m_ctx->lastSavedTime());
        m_notes->setSavedText("Saved " + m_ctx->lastSavedTime());
    });
}

} // namespace phy104
