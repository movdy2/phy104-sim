#include "MarkdownWorkspace.h"
#include "Components.h"
#include "LatexRenderer.h"
#include "MarkdownHighlighter.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTimer>
#include <QUrl>
#include <QToolButton>
#include <QVBoxLayout>

namespace phy104 {

// ============================================================================ MarkdownEditor
MarkdownEditor::MarkdownEditor(QWidget* parent) : QPlainTextEdit(parent) {
    setObjectName("mdEditor");
    QFont f(ThemeManager::uiFont());
    f.setPixelSize(13);
    setFont(f);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setPlaceholderText("Write physics notes in Markdown…\n\n# Title   ## Chapter   ### Formula name   #### Exam warning\n- bullet points      $ n₁ sin θ₁ = n₂ sin θ₂ $ for formulas");
    setUndoRedoEnabled(true);
}

void MarkdownEditor::insertSnippet(const QString& text) {
    QTextCursor c = textCursor();
    const QString selected = c.selectedText().replace(QChar(0x2029), '\n');
    const qsizetype marker = text.indexOf("%1");
    c.beginEditBlock();
    c.removeSelectedText();
    const int start = c.position();
    c.insertText(marker >= 0 ? QString(text).replace("%1", selected) : text);
    if (marker >= 0) {
        const qsizetype cursorPosition = static_cast<qsizetype>(start) + marker +
                                         (selected.isEmpty() ? qsizetype{0} : selected.size());
        c.setPosition(static_cast<int>(cursorPosition));
    }
    c.endEditBlock();
    setTextCursor(c);
    setFocus();
}

void MarkdownEditor::setHeadingLevel(int level) {
    QTextCursor c = textCursor();
    c.beginEditBlock();
    c.movePosition(QTextCursor::StartOfBlock);
    QTextCursor end = c;
    end.movePosition(QTextCursor::EndOfBlock);
    static const QRegularExpression marker("^#{1,6}\\s*");
    const QString line = c.block().text();
    const auto m = marker.match(line);
    const int existing = m.hasMatch() ? static_cast<int>(m.captured().count('#')) : 0;
    c.setPosition(c.block().position() + static_cast<int>(m.hasMatch() ? m.capturedLength() : 0), QTextCursor::KeepAnchor);
    c.removeSelectedText();
    c.setPosition(textCursor().block().position());
    if (level > 0 && existing != level) c.insertText(QString(level, '#') + " ");     // same level again = toggle off
    c.endEditBlock();
    setTextCursor(c);
    setFocus();
}

void MarkdownEditor::toggleBullet() {
    QTextCursor c = textCursor();
    c.beginEditBlock();
    c.movePosition(QTextCursor::StartOfBlock);
    static const QRegularExpression bullet("^\\s*[-*+]\\s");
    const auto m = bullet.match(c.block().text());
    if (m.hasMatch()) { c.setPosition(c.position() + static_cast<int>(m.capturedLength()), QTextCursor::KeepAnchor); c.removeSelectedText(); }
    else c.insertText("- ");
    c.endEditBlock();
    setFocus();
}

void MarkdownEditor::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Tab && !(e->modifiers() & Qt::ControlModifier)) { insertPlainText("    "); return; }
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & Qt::ShiftModifier)) {
        // continue a list: "- item" + Enter -> "- "; Enter on an empty item ends the list
        static const QRegularExpression item("^(\\s*)([-*+]|\\d+[.)])\\s(.*)$");
        QTextCursor c = textCursor();
        const QString line = c.block().text();
        const auto m = item.match(line);
        if (m.hasMatch() && !c.hasSelection() && c.positionInBlock() >= m.capturedStart(3)) {
            if (m.captured(3).trimmed().isEmpty()) {
                c.select(QTextCursor::BlockUnderCursor);                  // remove the empty marker line's content
                c.movePosition(QTextCursor::StartOfBlock);
                c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                c.removeSelectedText();
                setTextCursor(c);
                return;
            }
            QString marker = m.captured(2);
            if (marker.back().isDigit() == false && (marker.endsWith('.') || marker.endsWith(')'))) {
                const int n = marker.left(marker.size() - 1).toInt();
                marker = QString::number(n + 1) + marker.back();
            }
            QPlainTextEdit::keyPressEvent(e);
            insertPlainText(m.captured(1) + marker + " ");
            return;
        }
    }
    QPlainTextEdit::keyPressEvent(e);
}

// ============================================================================ MarkdownWorkspace
QString MarkdownWorkspace::starterNotes() {
    return "# Lab notes\n\n## Refraction & TIR\n\n### Snell's law\n- $n₁ sin θ₁ = n₂ sin θ₂$\n- compare the fitted index with the reference value\n\n"
           "#### Exam warning\nAngles are measured from the normal, not from the surface.\n";
}

QString MarkdownWorkspace::problemTemplate() {
    return "### Given:\n- \n\n### Formula:\n- $ $\n\n### Substitution & Solution:\n- \n";
}

QString MarkdownWorkspace::toPreviewMarkdown(const QString& markdown) {
    static const QRegularExpression block("\\$\\$\\s*([^$\\n]+?)\\s*\\$\\$");
    static const QRegularExpression inl("\\$(?!\\s)([^$\\n]*[^$\\s]|[^$\\s])\\$");
    static const QRegularExpression paren("\\\\\\(\\s*([^\\n]*?)\\s*\\\\\\)");
    QStringList out;
    bool fenced = false;
    for (QString line : markdown.split('\n')) {
        if (line.trimmed().startsWith("```")) fenced = !fenced;
        if (!fenced) {
            line.replace(block, "`\\1`");
            line.replace(inl, "`\\1`");
            line.replace(paren, "`\\1`");
        }
        out << line;
    }
    return out.join('\n');
}

MarkdownWorkspace::MarkdownWorkspace(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    // header: title + mode switch
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("MARKDOWN · FORMULA JOTTER");
    title->setObjectName("caption");
    m_modeTabs = new ModeTabs({"Edit", "Split", "Preview"});
    m_modeTabs->setFixedWidth(210);
    m_modeTabs->setCurrent(Split);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_modeTabs);
    root->addLayout(header);
    auto* toolbarScroll = new QScrollArea;                      // long toolbar: scrolls sideways instead of being clipped
    toolbarScroll->setWidget(buildToolbar());
    toolbarScroll->setWidgetResizable(true);
    toolbarScroll->setFrameShape(QFrame::NoFrame);
    toolbarScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolbarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    toolbarScroll->setFixedHeight(86);
    root->addWidget(toolbarScroll);

    m_editor = new MarkdownEditor;
    m_highlighter = new GruvboxMarkdownHighlighter(m_editor->document());
    m_preview = new QTextBrowser;
    m_preview->setObjectName("mdPreview");
    m_preview->setOpenExternalLinks(true);
    m_splitter = new QSplitter(Qt::Vertical);               // editor above the preview: both get the full width
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setSizes({1, 1});
    root->addWidget(m_splitter, 1);

    auto* footer = new QHBoxLayout;
    m_stats = new QLabel;
    m_stats->setObjectName("caption");
    m_saved = new QLabel("Autosave on");
    m_saved->setObjectName("caption");
    footer->addWidget(m_stats);
    footer->addStretch();
    footer->addWidget(m_saved);
    root->addLayout(footer);

    // timers: autosave 0.7 s after the last keystroke; the preview and statistics refresh sooner
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(700);
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(120);
    m_statsTimer = new QTimer(this);
    m_statsTimer->setSingleShot(true);
    m_statsTimer->setInterval(200);
    connect(m_saveTimer, &QTimer::timeout, this, &MarkdownWorkspace::flush);
    connect(m_previewTimer, &QTimer::timeout, this, &MarkdownWorkspace::renderPreview);
    connect(m_statsTimer, &QTimer::timeout, this, &MarkdownWorkspace::updateStats);
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this] {
        if (m_loading) return;
        m_pending = true;
        m_saved->setText("Editing…");
        m_saveTimer->start();
        m_statsTimer->start();
        if (m_mode != Edit) m_previewTimer->start();
    });
    connect(m_modeTabs, &ModeTabs::currentChanged, this, [this](int i) { setMode(static_cast<Mode>(i)); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MarkdownWorkspace::flush);          // never lose the last edit
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &MarkdownWorkspace::applyTheme);

    applyTheme();
    setMode(Split);
    updateStats();
}

QWidget* MarkdownWorkspace::buildToolbar() {
    auto* box = new QWidget;
    auto* rows = new QVBoxLayout(box);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(4);

    auto button = [this](QHBoxLayout* row, const QString& label, const QString& tip, const std::function<void()>& action) {
        auto* b = new QToolButton;
        b->setObjectName("fmt");
        b->setText(label);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);                                  // the editor keeps the cursor
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumWidth(30);
        connect(b, &QToolButton::clicked, this, [this, action] { if (m_mode == Preview) setMode(Split); action(); });
        row->addWidget(b);
        return b;
    };
    auto separator = [](QHBoxLayout* row) {
        auto* line = new QFrame;
        line->setFrameShape(QFrame::VLine);
        line->setFixedHeight(18);
        line->setStyleSheet("color: " + ThemeManager::instance().theme().border.name() + ";");
        row->addSpacing(2);
        row->addWidget(line);
        row->addSpacing(2);
    };

    // row 1: optics symbols
    auto* r1 = new QHBoxLayout;
    r1->setSpacing(2);
    struct Sym { const char* label; const char* text; const char* tip; };
    const Sym symbols[] = {{"θ", "θ", "theta - angle"}, {"λ", "λ", "lambda - wavelength"}, {"φ", "φ", "phi"}, {"α", "α", "alpha"}, {"β", "β", "beta"},
                           {"γ", "γ", "gamma"}, {"δ", "δ", "delta - deviation"}, {"Δ", "Δ", "Delta - difference"}, {"π", "π", "pi"}, {"μ", "μ", "mu"},
                           {"ε", "ε", "epsilon"}, {"n₁", "n₁", "refractive index 1"}, {"n₂", "n₂", "refractive index 2"}, {"°", "°", "degree"},
                           {"×", "×", "times"}, {"√", "√(%1)", "square root"}, {"≈", "≈", "approximately"}, {"→", "→", "arrow"}, {"±", "±", "plus-minus"}};
    for (const Sym& s : symbols) button(r1, s.label, s.tip, [this, t = QString(s.text)] { snippet(t); });
    r1->addStretch();
    rows->addLayout(r1);

    // row 2: functions, structure, formulas, template
    auto* r2 = new QHBoxLayout;
    r2->setSpacing(2);
    for (const char* fn : {"sin", "arcsin", "cos", "tan"}) button(r2, fn, QString("insert %1(…)").arg(fn), [this, t = QString(fn) + "(%1)"] { snippet(t); });
    separator(r2);
    button(r2, "$ $", "inline formula  $ … $  (wraps the selection)", [this] { snippet("$%1$"); });
    button(r2, "H1", "Title (yellow, 18 pt)", [this] { m_editor->setHeadingLevel(1); });
    button(r2, "H2", "Chapter (orange, 14 pt)", [this] { m_editor->setHeadingLevel(2); });
    button(r2, "H3", "Section / formula name (green, 12 pt)", [this] { m_editor->setHeadingLevel(3); });
    button(r2, "H4 ⚠", "Exam warning / note (red, 11 pt)", [this] { m_editor->setHeadingLevel(4); });
    button(r2, "•", "bullet point", [this] { m_editor->toggleBullet(); });
    separator(r2);

    auto* formulas = new QToolButton;
    formulas->setObjectName("fmt");
    formulas->setText("Formulas ▾");
    formulas->setToolTip("Insert a standard optics formula");
    formulas->setPopupMode(QToolButton::InstantPopup);
    formulas->setFocusPolicy(Qt::NoFocus);
    auto* menu = new QMenu(formulas);
    struct F { const char* name; const char* formula; };
    const F list[] = {{"Snell's law", "n₁ sin θ₁ = n₂ sin θ₂"}, {"Critical angle", "sin θc = n₂ / n₁"}, {"Lens / mirror equation", "1/f = 1/dₒ + 1/dᵢ"},
                      {"Magnification", "m = -dᵢ/dₒ = hᵢ/hₒ"}, {"Prism deviation", "δ = i₁ + i₂ - A"}, {"Double slit", "β = λL / d"},
                      {"Malus' law", "I = I₀ cos²θ"}, {"Brewster's angle", "tan θp = n₂ / n₁"}};
    for (const F& f : list) {
        QAction* a = menu->addAction(f.name);
        connect(a, &QAction::triggered, this, [this, n = QString(f.name), fm = QString(f.formula)] {
            if (m_mode == Preview) setMode(Split);
            snippet("\n### " + n + "\n- $" + fm + "$\n");
        });
    }
    formulas->setMenu(menu);
    r2->addWidget(formulas);
    auto* tmpl = button(r2, "Problem template", "Given / Formula / Substitution & Solution", [this] { insertTemplate(); });
    tmpl->setObjectName("fmtAccent");
    r2->addStretch();
    rows->addLayout(r2);
    return box;
}

void MarkdownWorkspace::snippet(const QString& text) { m_editor->insertSnippet(text); }

void MarkdownWorkspace::insertTemplate() {
    QTextCursor c = m_editor->textCursor();
    const bool startOfLine = c.atBlockStart() && c.block().text().isEmpty();
    const bool emptyDoc = m_editor->document()->isEmpty();
    const QString prefix = (emptyDoc || startOfLine) ? QString() : QString("\n\n");
    c.beginEditBlock();
    c.insertText(prefix);
    const int start = c.position();
    c.insertText(problemTemplate());
    const int givenBulletOffset = static_cast<int>(QString("### Given:\n- ").size());
    c.setPosition(start + givenBulletOffset);                                // cursor right after the first "- ".
    c.endEditBlock();
    m_editor->setTextCursor(c);
    m_editor->setFocus();
}

void MarkdownWorkspace::setMarkdown(const QString& markdown) {
    m_loading = true;
    m_editor->setPlainText(markdown);
    m_loading = false;
    m_pending = false;
    renderPreview();
    updateStats();
}

QString MarkdownWorkspace::markdown() const { return m_editor->toPlainText(); }

void MarkdownWorkspace::flush() {
    m_saveTimer->stop();
    if (!m_pending) return;
    m_pending = false;
    emit contentChanged(markdown());
}

void MarkdownWorkspace::setSavedText(const QString& text) { m_saved->setText(text); }

void MarkdownWorkspace::setMode(Mode mode) {
    m_mode = mode;
    m_modeTabs->setCurrent(mode);
    m_editor->setVisible(mode != Preview);
    m_preview->setVisible(mode != Edit);
    if (mode != Edit) renderPreview();
}

void MarkdownWorkspace::updateStats() {
    const QString text = markdown();
    const int words = static_cast<int>(text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size());
    m_stats->setText(QString("%1 words · %2 characters · %3 lines").arg(words).arg(text.size()).arg(m_editor->document()->blockCount()));
}

/// $x$, $$x$$ and \(x\) become images rendered by the LaTeX engine (readable unicode maths is converted first).
QString MarkdownWorkspace::withFormulaImages(const QString& markdown, QList<QPair<QString, QPixmap>>& resources) const {
    static const QRegularExpression block("\\$\\$\\s*([^$\\n]+?)\\s*\\$\\$");
    static const QRegularExpression inl("\\$(?!\\s)([^$\\n]*[^$\\s]|[^$\\s])\\$");
    static const QRegularExpression paren("\\\\\\(\\s*([^\\n]*?)\\s*\\\\\\)");
    const MarkdownPalette pal = m_highlighter->palette();
    QFont f(ThemeManager::uiFont());
    f.setPixelSize(16);
    auto image = [&](const QString& formula) {
        const QString name = QString("latex://%1").arg(resources.size());
        resources.append({name, LatexRenderer::render(LatexRenderer::fromPlainMath(formula), f, pal.math, 2.0)});
        return QString("![formula](%1)").arg(name);           // alt text is required: Qt's importer drops ![](...)
    };
    QStringList out;
    bool fenced = false;
    for (QString line : markdown.split('\n')) {
        if (line.trimmed().startsWith("```")) fenced = !fenced;
        if (!fenced) {
            static const QRegularExpression emptyAlt("!\\[\\s*\\]\\(");   // QTextDocument drops images whose alt text is empty
            line.replace(emptyAlt, "![image](");                          // give them one so the preview keeps them
            for (const QRegularExpression* re : {&block, &inl, &paren}) {
                QString result;
                int last = 0;
                for (auto it = re->globalMatch(line); it.hasNext();) {
                    const auto mt = it.next();
                    result += line.mid(last, mt.capturedStart() - last) + image(mt.captured(1));
                    last = static_cast<int>(mt.capturedEnd());
                }
                line = result + line.mid(last);
            }
        }
        out << line;
    }
    return out.join('\n');
}

void MarkdownWorkspace::renderPreview() {
    if (m_mode == Edit) return;
    const int scroll = m_preview->verticalScrollBar()->value();
    QList<QPair<QString, QPixmap>> resources;
    const QString source = withFormulaImages(markdown(), resources);
    m_preview->document()->setMarkdown(source);
    for (const auto& [name, pix] : resources)                       // resources are added after setMarkdown (it clears the document)
        m_preview->document()->addResource(QTextDocument::ImageResource, QUrl(name), pix);
    restylePreview();
    m_preview->verticalScrollBar()->setValue(scroll);
}

/// The Markdown importer knows headings, lists and code spans; give them the Gruvbox roles of the editor.
void MarkdownWorkspace::restylePreview() {
    const MarkdownPalette pal = m_highlighter->palette();
    QTextDocument* doc = m_preview->document();
    struct Job { int pos, len; QTextCharFormat fmt; };
    QList<Job> jobs;
    static const double sizes[] = {0, 18, 14, 12, 11};
    const QColor colors[] = {QColor(), pal.h1, pal.h2, pal.h3, pal.h4};
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const int level = std::min(4, b.blockFormat().headingLevel());
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (frag.charFormat().isImageFormat()) continue;                   // formulas are images now
            QTextCharFormat f;
            if (level > 0) {
                f.setForeground(colors[level]);
                f.setFontWeight(QFont::Bold);
                f.setFontPointSize(sizes[level]);
            } else if (frag.charFormat().fontFixedPitch()) {
                f.setForeground(pal.math);
                f.setFontFamilies({ThemeManager::monoFont(), "monospace"});
            } else if (b.textList()) {
                f.setForeground(pal.bullet);
            } else continue;
            jobs.append({frag.position(), frag.length(), f});
        }
    }
    QTextCursor c(doc);
    c.beginEditBlock();
    for (const Job& j : jobs) {
        c.setPosition(j.pos);
        c.setPosition(j.pos + j.len, QTextCursor::KeepAnchor);
        c.mergeCharFormat(j.fmt);
    }
    c.endEditBlock();
}

void MarkdownWorkspace::applyTheme() {
    m_highlighter->setPalette(MarkdownPalette::forCurrentTheme());
    if (m_mode != Edit && m_preview) renderPreview();
}

} // namespace phy104
