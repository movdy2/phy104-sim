#pragma once
#include <QPixmap>
#include <QPlainTextEdit>
#include <QWidget>

class QLabel;
class QSplitter;
class QTextBrowser;
class QTimer;

namespace phy104 {

class GruvboxMarkdownHighlighter;
class ModeTabs;

/// Plain-text editor for Markdown notes: list continuation on Enter, Tab inserts spaces,
/// and `insertSnippet()` for the toolbar.
class MarkdownEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit MarkdownEditor(QWidget* parent = nullptr);
    /// Inserts `text` at the cursor. "%1" is replaced by the selected text (or removed), and the cursor is
    /// placed where "%1" was - so "$%1$" wraps the selection in dollar signs, or leaves the cursor between them.
    void insertSnippet(const QString& text);
    /// Puts `level` (1..4) hashes at the start of the current line (0 = remove, same level again = toggle off).
    void setHeadingLevel(int level);
    void toggleBullet();
protected:
    void keyPressEvent(QKeyEvent* event) override;
};

/// Study workspace (Notes & Formula Jotter): Markdown editor + live preview + physics snippet toolbar.
///  * modes: Edit | Split | Preview
///  * Gruvbox colour-coded syntax highlighting (H1 yellow, H2 orange, H3 green, H4 red, formulas aqua, bullets light grey)
///  * autosave: `contentChanged()` is emitted ~0.7 s after the last keystroke (and on quit), the page stores it in the session
class MarkdownWorkspace : public QWidget {
    Q_OBJECT
public:
    enum Mode { Edit = 0, Split = 1, Preview = 2 };

    explicit MarkdownWorkspace(QWidget* parent = nullptr);

    /// Shared first-run notes shown in both the standalone Notes page and the Study Hub tab.
    static QString starterNotes();

    /// Loads text without triggering an autosave.
    void setMarkdown(const QString& markdown);
    QString markdown() const;
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }
    /// Emits contentChanged now if there are edits that were not reported yet.
    void flush();
    void setSavedText(const QString& text);

    MarkdownEditor* editor() const { return m_editor; }
    QTextBrowser* preview() const { return m_preview; }

    /// "### Given:" / "### Formula:" / "### Substitution & Solution:" layout for a worked problem.
    static QString problemTemplate();
    /// $x$, $$x$$ and \(x\) become inline code so the preview can show them as formulas.
    static QString toPreviewMarkdown(const QString& markdown);

signals:
    void contentChanged(const QString& markdown);

private:
    QWidget* buildToolbar();
    void insertTemplate();
    void snippet(const QString& text);
    void renderPreview();
    void updateStats();
    void restylePreview();
    QString withFormulaImages(const QString& markdown, QList<QPair<QString, QPixmap>>& resources) const;
    void applyTheme();

    Mode m_mode = Split;
    MarkdownEditor* m_editor;
    QTextBrowser* m_preview;
    QSplitter* m_splitter;
    GruvboxMarkdownHighlighter* m_highlighter;
    ModeTabs* m_modeTabs;
    QLabel *m_stats, *m_saved;
    QTimer *m_saveTimer, *m_previewTimer, *m_statsTimer;
    bool m_loading = false;
    bool m_pending = false;
};

} // namespace phy104
