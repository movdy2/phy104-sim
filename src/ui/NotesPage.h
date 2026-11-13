#pragma once
#include "BasePage.h"

class QLabel;

namespace phy104 {

class MarkdownWorkspace;
class Panel;

/// "Notes" - the clean Markdown study workspace of the Figma screen: write, edit and preview the lab notes.
/// The formula reference lives on the Formula Solver page and the files are written from the header export
/// icon (PDF / Excel / CSV), so this page is only the editor.
class NotesPage : public BasePage {
    Q_OBJECT
public:
    explicit NotesPage(AppContext* context, QWidget* parent = nullptr);

private:
    MarkdownWorkspace* m_notes;
    Panel* m_notesPanel;
};

} // namespace phy104
