#pragma once
#include "BasePage.h"
#include "Models.h"

#include <vector>

class QToolButton;

namespace phy104 {

class MarkdownWorkspace;
class Panel;
class ScientificKeyboard;

/// "Notes" - the clean Markdown study workspace of the Figma screen: write, edit and preview the lab notes.
/// The formula reference lives on the Formula Solver page and the files are written from the header export
/// icon (PDF / Excel / CSV), so this page is only the editor - plus the pop-up scientific keyboard that types
/// LaTeX straight into the note (the same keyboard as the Equation Lab).
class NotesPage : public BasePage {
    Q_OBJECT
public:
    NotesPage(AppContext* context, std::vector<ScientificPage> keyboardPages, QWidget* parent = nullptr);

private:
    void toggleKeyboard(bool on);

    MarkdownWorkspace* m_notes;
    Panel* m_notesPanel;
    QToolButton* m_keyboardToggle;
    ScientificKeyboard* m_keyboard;
};

} // namespace phy104
