#pragma once
#include <QColor>
#include <QList>
#include <QSyntaxHighlighter>
#include <QTextLayout>

namespace phy104 {

/// Colours of the Markdown editor. The dark set is the specification of the study workspace;
/// the light set keeps the same roles with darker shades so the text stays readable on a light page.
struct MarkdownPalette {
    QColor h1, h2, h3, h4;      // title, chapter, section / formula name, exam warning
    QColor math;                // inline formulas
    QColor bullet;              // list items
    QColor quote, dim, text;
    bool dark = true;

    static MarkdownPalette darkSet();     // #d79921 #e78a4e #a9b665 #ea696c / #83a598 / #ebdbb2
    static MarkdownPalette lightSet();
    static MarkdownPalette forCurrentTheme();
};

/// Gruvbox Markdown highlighter:
///   # H1   yellow  bold 18 pt        ## H2   orange bold 14 pt
///   ### H3 green   bold 12 pt        #### H4 red    bold 11 pt   (exam warning / note)
///   formulas  $x$  $$x$$  \(x\)  `x`  and (parenthesised maths)  -> aqua, JetBrains Mono
///   bullets   - * + and 1.           -> light grey
/// It also marks **bold**, *italic*, > quotes, --- rules and ``` code fences.
class GruvboxMarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit GruvboxMarkdownHighlighter(QTextDocument* document);

    void setPalette(const MarkdownPalette& palette);
    const MarkdownPalette& palette() const { return m_palette; }

    /// The highlighting of one text block, as a pure function (used by the tests).
    /// previousState / *newState: 0 = normal text, 1 = inside a ``` fence.
    static QList<QTextLayout::FormatRange> analyze(const QString& block, int previousState, int* newState,
                                                   const MarkdownPalette& palette);

    /// Heading level 1..4 of a line ("#### " and deeper count as 4), or 0.
    static int headingLevel(const QString& line);
    static bool isBulletLine(const QString& line);

protected:
    void highlightBlock(const QString& text) override;

private:
    MarkdownPalette m_palette;
};

} // namespace phy104
