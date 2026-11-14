#include "MarkdownHighlighter.h"
#include "ThemeManager.h"

#include <QRegularExpression>
#include <algorithm>
#include <QTextCharFormat>

namespace phy104 {
namespace {

// compiled once; the highlighter runs on every keystroke for the changed block only
const QRegularExpression& reHeading() { static const QRegularExpression r("^\\s{0,3}(#{1,6})\\s+\\S"); return r; }
const QRegularExpression& reBullet()  { static const QRegularExpression r("^\\s*(?:[-*+]|\\d+[.)])\\s+"); return r; }
const QRegularExpression& reQuote()   { static const QRegularExpression r("^\\s*>"); return r; }
const QRegularExpression& reRule()    { static const QRegularExpression r("^\\s*(?:-{3,}|\\*{3,}|_{3,})\\s*$"); return r; }
const QRegularExpression& reFence()   { static const QRegularExpression r("^\\s*```"); return r; }
const QRegularExpression& reBold()    { static const QRegularExpression r("\\*\\*[^*\\n]+\\*\\*"); return r; }
const QRegularExpression& reItalic()  { static const QRegularExpression r("(?<![*\\w])\\*(?![\\s*])[^*\\n]*[^*\\s]\\*(?![*\\w])"); return r; }

// formulas: $$..$$  $..$  \(..\)  `..`  and parentheses that contain a mathematical symbol or function
const QList<QRegularExpression>& mathPatterns() {
    static const QList<QRegularExpression> list = {
        QRegularExpression("\\$\\$[^$\\n]+\\$\\$"),
        QRegularExpression("(?<!\\$)\\$(?![\\s$])[^$\\n]*[^$\\s]\\$(?!\\$)|(?<!\\$)\\$[^$\\s]\\$(?!\\$)"),
        QRegularExpression("\\\\\\([^\\n]*?\\\\\\)"),
        QRegularExpression("`[^`\\n]+`"),
        QRegularExpression("\\b(?:sin|cos|tan|asin|acos|atan|arcsin|arccos|arctan|ln|log|sqrt)\\s*\\([^()\\n]*\\)"),
        QRegularExpression("(?<!\\\\)\\((?:[^()\\n]*[=^_√θλφαβγδΔπμε∞±×÷<>≈≤≥∑∫][^()\\n]*|[^()\\n]*\\b(?:sin|cos|tan|asin|acos|atan|arcsin|arccos|arctan|ln|log|sqrt)\\b[^()\\n]*)\\)")};
    return list;
}

QTextCharFormat headingFormat(const QColor& color, double points) {
    QTextCharFormat f;
    f.setForeground(color);
    f.setFontWeight(QFont::Bold);
    f.setFontPointSize(points);
    return f;
}

QTextCharFormat mathFormat(const QColor& color) {
    QTextCharFormat f;
    f.setForeground(color);
    f.setFontFamilies({ThemeManager::monoFont(), "monospace"});
    return f;
}

void add(QList<QTextLayout::FormatRange>& out, int start, int length, const QTextCharFormat& f) {
    if (length > 0) out.append({start, length, f});
}

} // namespace

MarkdownPalette MarkdownPalette::darkSet() {
    MarkdownPalette p;
    p.h1 = "#d79921"; p.h2 = "#e78a4e"; p.h3 = "#a9b665"; p.h4 = "#ea696c";
    p.math = "#83a598"; p.bullet = "#ebdbb2";
    p.quote = "#a89984"; p.dim = "#928374"; p.text = "#ebdbb2";
    p.dark = true;
    return p;
}

MarkdownPalette MarkdownPalette::lightSet() {
    MarkdownPalette p;
    p.h1 = "#b57614"; p.h2 = "#af3a03"; p.h3 = "#79740e"; p.h4 = "#9d0006";
    p.math = "#076678"; p.bullet = "#3c3836";
    p.quote = "#665c54"; p.dim = "#7c6f64"; p.text = "#3c3836";
    p.dark = false;
    return p;
}

MarkdownPalette MarkdownPalette::forCurrentTheme() {
    return ThemeManager::instance().isDark() ? darkSet() : lightSet();
}

int GruvboxMarkdownHighlighter::headingLevel(const QString& line) {
    const auto m = reHeading().match(line);
    return m.hasMatch() ? std::min<int>(4, static_cast<int>(m.captured(1).size())) : 0;
}

bool GruvboxMarkdownHighlighter::isBulletLine(const QString& line) { return reBullet().match(line).hasMatch(); }

QList<QTextLayout::FormatRange> GruvboxMarkdownHighlighter::analyze(const QString& block, int previousState, int* newState,
                                                                    const MarkdownPalette& pal) {
    QList<QTextLayout::FormatRange> out;
    int state = previousState == 1 ? 1 : 0;
    const int n = static_cast<int>(block.size());

    // fenced code block: everything between ``` lines is monospace and not interpreted
    const bool fence = reFence().match(block).hasMatch();
    if (state == 1 || fence) {
        add(out, 0, n, mathFormat(pal.math));
        if (fence) state = state == 1 ? 0 : 1;
        if (newState) *newState = state;
        return out;
    }
    if (newState) *newState = 0;

    // block level
    if (const int level = headingLevel(block)) {
        static const double sizes[] = {0, 18, 14, 12, 11};                   // pt, as specified
        const QColor colors[] = {QColor(), pal.h1, pal.h2, pal.h3, pal.h4};
        add(out, 0, n, headingFormat(colors[level], sizes[level]));
        return out;                                                          // headings are not interpreted further
    }
    if (reRule().match(block).hasMatch()) {
        QTextCharFormat f; f.setForeground(pal.dim);
        add(out, 0, n, f);
        return out;
    }
    if (reQuote().match(block).hasMatch()) {
        QTextCharFormat f; f.setForeground(pal.quote); f.setFontItalic(true);
        add(out, 0, n, f);
    } else if (const auto b = reBullet().match(block); b.hasMatch()) {
        QTextCharFormat f; f.setForeground(pal.bullet);
        add(out, 0, n, f);
        QTextCharFormat marker = f; marker.setFontWeight(QFont::Bold);
        add(out, 0, static_cast<int>(b.capturedLength()), marker);
    }

    // inline emphasis, then formulas on top (formulas win where they overlap)
    for (auto it = reBold().globalMatch(block); it.hasNext();) {
        const auto m = it.next();
        QTextCharFormat f; f.setFontWeight(QFont::Bold);
        add(out, static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), f);
    }
    for (auto it = reItalic().globalMatch(block); it.hasNext();) {
        const auto m = it.next();
        QTextCharFormat f; f.setFontItalic(true);
        add(out, static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedLength()), f);
    }
    // formulas: collect every match, drop the ones fully inside another (so "$$x$$" is ONE range, not three)
    QList<QPair<int, int>> spans;                                            // start, end (exclusive)
    for (const QRegularExpression& re : mathPatterns())
        for (auto it = re.globalMatch(block); it.hasNext();) {
            const auto m = it.next();
            spans.append({static_cast<int>(m.capturedStart()), static_cast<int>(m.capturedEnd())});
        }
    std::sort(spans.begin(), spans.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first < b.first : a.second > b.second; });
    int coveredUntil = -1;
    for (const auto& s : spans) {
        if (s.second <= coveredUntil) continue;                              // contained in an earlier span
        add(out, s.first, s.second - s.first, mathFormat(pal.math));
        coveredUntil = std::max(coveredUntil, s.second);
    }
    return out;
}

GruvboxMarkdownHighlighter::GruvboxMarkdownHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document), m_palette(MarkdownPalette::forCurrentTheme()) {}

void GruvboxMarkdownHighlighter::setPalette(const MarkdownPalette& palette) {
    m_palette = palette;
    rehighlight();
}

void GruvboxMarkdownHighlighter::highlightBlock(const QString& text) {
    int state = 0;
    const auto ranges = analyze(text, previousBlockState(), &state, m_palette);
    for (const auto& r : ranges) setFormat(r.start, r.length, r.format);   // later ranges override earlier ones
    setCurrentBlockState(state);
}

} // namespace phy104
