// UI-level checks that need Qt Widgets/Svg but no visible window (run with the offscreen platform):
//   * every LaTeX string in the JSON files is understood by our renderer
//   * every SVG diagram has only known colour tokens and renders with Qt in both themes
//   * the Gruvbox + Casio LCD colours are exactly the ones in the specification
#include "Icons.h"
#include "MarkdownHighlighter.h"
#include "MarkdownWorkspace.h"
#include "ScientificKeyboard.h"
#include "JsonRepositories.h"
#include "LatexRenderer.h"
#include "SvgDiagramView.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QFile>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <algorithm>
#include <iostream>
#include <set>

using namespace phy104;

static int g_failures = 0;
static void check(bool ok, const QString& what) {
    if (!ok) { ++g_failures; std::cerr << "FAIL: " << what.toStdString() << "\n"; }
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString data = DATA_DIR, assets = ASSETS_DIR;

    // ---- LaTeX ----
    int latexCount = 0;
    try {
        for (const auto& f : JsonFormulaRepository(data + "/formulas.json").loadAll()) {
            QString err;
            check(LatexRenderer::validate(QString::fromStdString(f.latex), &err), "LaTeX of formula " + QString::fromStdString(f.id) + ": " + err);
            ++latexCount;
        }
        for (const auto& v : JsonVariableRepository(data + "/variables.json").loadAll()) {
            QString err;
            check(!v.latex.empty(), "variable has LaTeX: " + QString::fromStdString(v.id));
            check(LatexRenderer::validate(QString::fromStdString(v.latex), &err), "LaTeX of variable " + QString::fromStdString(v.id) + ": " + err);
            ++latexCount;
        }
    } catch (const std::exception& e) { check(false, e.what()); }
    for (const char* broken : {"\\frac{1}", "\\frac{", "\\sqrt", "x^", "x_", "\\left(", "{{", "}}", "\\frac{}{}", "^{}", "\\operatorname{min}()"}) {   // half-typed formulas must never crash
        const QPixmap p = LatexRenderer::render(broken, QFont(), Qt::white, 1.0);
        check(!p.isNull(), QString("renders without crashing: ") + broken);
    }
    check(!LatexRenderer::validate("\\notacommand{x}"), "unknown command is rejected");
    check(!LatexRenderer::validate("\\frac{1}{2"), "missing brace is rejected");
    check(LatexRenderer::validate("\\frac{1}{s}+\\frac{1}{s'}=\\frac{1}{f}"), "thin lens equation is accepted");
    QFont font; font.setPointSize(14);
    const QPixmap pix = LatexRenderer::render("\\sqrt{\\frac{a}{b}}", font, Qt::white, 2.0);
    check(!pix.isNull() && pix.width() > 10 && pix.height() > 10, "LaTeX renders to a pixmap");
    check(LatexRenderer::measure("\\frac{a}{b}", font).height() > LatexRenderer::measure("a", font).height(), "fraction is taller than a letter");

    // ---- theme colours: the Figma component sheet ----
    const Theme dark = Theme::figmaDark();
    check(dark.bg == QColor("#282828") && dark.card == QColor("#3c3836") && dark.text == QColor("#ebdbb2"), "dark: #282828 / #3c3836 / #ebdbb2");
    check(dark.amber == QColor("#d79921") && dark.amberHover == QColor("#e5ac39") && dark.amberPressed == QColor("#b57614"), "amber primary, hover, pressed");
    check(dark.border == QColor("#504945") && dark.error == QColor("#fb4934"), "border #504945, error #fb4934");
    check(dark.lcdBg == QColor("#9bb094") && dark.lcdText == QColor("#0b1c08") && dark.lcdFrame == QColor("#171e15") && dark.lcdDivider == QColor("#708369"), "Casio LCD colours");
    check(Theme::figmaLight().lcdBg == dark.lcdBg, "LCD is the same olive screen in light mode");
    ThemeManager::loadFonts();
    check(QFontDatabase::families().contains("Inter") && QFontDatabase::families().contains("JetBrains Mono"), "bundled fonts Inter + JetBrains Mono load");
    check(QFontDatabase::families().contains("DSEG7 Classic") && QFontDatabase::families().contains("DSEG14 Classic"), "Casio-style segment fonts load");
    for (const char* icon : {"layout-dashboard", "split", "focus", "triangle", "waves", "notebook-pen", "file-spreadsheet", "scan-line", "arrow-up-right", "sigma", "calculator", "ruler", "image", "download", "printer"})
        check(!Icons::pixmap(icon, Qt::white, 18).isNull(), QString("icon exists: ") + icon);

    // ---- readable unicode maths -> LaTeX (notes preview, formula reference) ----
    {
        auto conv = [](const char* s) { return LatexRenderer::fromPlainMath(QString::fromUtf8(s)); };
        const QString snell = conv("n₁ sin θ₁ = n₂ sin θ₂");
        check(snell.contains("\\sin") && snell.contains("\\theta") && snell.contains("_{1}") && snell.contains("_{2}") && !snell.contains(QString::fromUtf8("θ")), "Snell: functions, Greek, subscripts");
        check(conv("1/f = 1/dₒ + 1/dᵢ").contains("\\frac{1}{f}") && conv("1/f = 1/dₒ + 1/dᵢ").contains("\\frac{1}{d_{o}}"), "fractions");
        check(conv("sin θc = n₂ / n₁").contains("\\frac{n_{2}}{n_{1}}"), "spaced fraction");
        check(conv("I = I₀ cos²θ").contains("^{2}") && conv("I = I₀ cos²θ").contains("I_{0}"), "superscripts");
        check(conv("r = √(mλR)").contains("\\sqrt{") && conv("a × b ≈ 30°").contains("\\times") && conv("a × b ≈ 30°").contains("^{\\circ}"), "roots and symbols");
        check(conv("\\frac{a}{b}") == "\\frac{a}{b}", "LaTeX input is left unchanged");
        check(conv("asin(x)").contains("\\arcsin"), "asin becomes arcsin");
        for (const char* f : {"n₁ sin θ₁ = n₂ sin θ₂", "θc = asin(n₂ / n₁)", "1/f = 1/dₒ + 1/dᵢ", "m = -dᵢ/dₒ = hᵢ/hₒ", "δ = i₁ + i₂ - A", "n = sin[(A+δmin)/2] / sin(A/2)", "β = λL/d  ·  yₘ = mλL/d", "I/I₀ = cos²(πdy/λL)", "s' > 0: real image", "|m| > 1"}) {
            QString err;
            check(LatexRenderer::validate(conv(f), &err), QString("reference formula renders: ") + f + " -> " + conv(f) + " " + err);
        }
    }

    // ---- Markdown highlighter: the specified colours, weights and sizes ----
    {
        const MarkdownPalette pal = MarkdownPalette::darkSet();
        auto first = [&](const QString& line, int state = 0, int* out = nullptr) {
            int s = 0;
            const auto r = GruvboxMarkdownHighlighter::analyze(line, state, out ? out : &s, pal);
            return r;
        };
        struct H { const char* text; const char* color; double pt; };
        for (const H& h : {H{"# Primary title", "#d79921", 18}, H{"## Chapter", "#e78a4e", 14}, H{"### Snell's law", "#a9b665", 12}, H{"#### Exam warning", "#ea696c", 11}}) {
            const auto r = first(h.text);
            check(r.size() == 1 && r[0].start == 0 && r[0].length == QString(h.text).size(), QString("heading covers the line: ") + h.text);
            if (r.size() == 1) {
                check(r[0].format.foreground().color() == QColor(h.color), QString("heading colour ") + h.color);
                check(r[0].format.fontWeight() == QFont::Bold, QString("heading bold: ") + h.text);
                check(std::fabs(r[0].format.fontPointSize() - h.pt) < 1e-9, QString("heading size %1 pt").arg(h.pt));
            }
        }
        check(GruvboxMarkdownHighlighter::headingLevel("##### deep") == 4 && GruvboxMarkdownHighlighter::headingLevel("#nospace") == 0 &&
              GruvboxMarkdownHighlighter::headingLevel("plain # text") == 0, "heading levels");

        auto mathColor = [&](const QString& line) {                       // text covered by an aqua monospace range
            auto ranges = first(line);
            std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
            QStringList found;
            for (const auto& r : ranges)
                if (r.format.foreground().color() == QColor("#83a598") && r.format.fontFamilies().toStringList().contains("JetBrains Mono"))
                    found << line.mid(r.start, r.length);
            return found;
        };
        check(mathColor("The law $n₁ sin θ₁ = n₂ sin θ₂$ holds") == QStringList{"$n₁ sin θ₁ = n₂ sin θ₂$"}, "inline $formula$ is aqua mono");
        check(mathColor("block $$β = λL / d$$ here") == QStringList{"$$β = λL / d$$"}, "$$formula$$ is aqua mono");
        check(mathColor("see (θ₁ = 30°) and `code`") == QStringList{"(θ₁ = 30°)", "`code`"}, "(maths in brackets) and `code` are aqua mono");
        check(mathColor("call sin(x) and (see page 3)") == QStringList{"sin(x)"} && mathColor("(see page 3) is not maths").isEmpty(), "function calls are formulas, ordinary brackets stay plain");
        check(mathColor("a ($x$) b") == QStringList{"($x$)"} || mathColor("a ($x$) b") == QStringList{"$x$"}, "a formula inside brackets is one range");
        check(mathColor("costs $ 5 only").isEmpty(), "a lone dollar sign with a space is not a formula");
        check(mathColor("\\(n_1\\) works").size() == 1, "\\( ... \\) is a formula");

        for (const char* b : {"- item", "* item", "+ item", "  - nested", "1. first", "2) second"}) {
            const auto r = first(b);
            bool grey = !r.isEmpty();
            for (const auto& x : r) grey = grey && x.format.foreground().color() == QColor("#ebdbb2");
            check(grey, QString("bullet is light grey: ") + b);
        }
        check(first("plain text without markup").isEmpty() && first("").isEmpty(), "plain text is not coloured");
        check(first("-notabullet").isEmpty() && first("**bold**").size() == 1, "markers need a space; **bold** is bold");

        int state = 0;                                                    // fenced code blocks
        first("```", 0, &state);
        check(state == 1, "opening fence");
        const auto inside = first("# not a heading $x$", 1, &state);
        check(state == 1 && inside.size() == 1 && inside[0].format.fontFamilies().toStringList().contains("JetBrains Mono"), "inside a fence nothing is interpreted");
        first("```", 1, &state);
        check(state == 0, "closing fence");

        const MarkdownPalette light = MarkdownPalette::lightSet();
        check(light.h1 != pal.h1 && !light.dark && pal.dark, "a light palette exists");
        check(MarkdownWorkspace::problemTemplate().contains("### Given:") && MarkdownWorkspace::problemTemplate().contains("### Formula:") &&
              MarkdownWorkspace::problemTemplate().contains("### Substitution & Solution:"), "problem template has the three sections");
        check(MarkdownWorkspace::toPreviewMarkdown("a $x$ and $$y$$ and \\(z\\)") == "a `x` and `y` and `z`", "formulas become inline code for the preview");
        check(MarkdownWorkspace::toPreviewMarkdown("```\n$x$\n```") == "```\n$x$\n```" && MarkdownWorkspace::toPreviewMarkdown("costs $ 5") == "costs $ 5", "fences and lone $ are left alone");
    }

    // ---- keyboard.json: the Figma layout and every variable of variables.json ----
    try {
        const auto pages = JsonKeyboardRepository(data + "/keyboard.json").scientific();
        check(pages.size() == 4 && pages[0].name == "Greek symbols" && pages[1].name == "Math operators" && pages[2].name == "Functions" && pages[3].name == "Variables", "keyboard categories");
        std::set<std::string> varSymbols;
        for (const auto& v : JsonVariableRepository(data + "/variables.json").loadAll()) varSymbols.insert(v.display);
        std::set<std::string> keyLegends;
        int keys = 0;
        for (const auto& page : pages)
            for (const auto& row : page.rows)
                for (const auto& key : row) {
                    if (!key.control.empty()) continue;
                    ++keys;
                    QString err;
                    check(LatexRenderer::validate(QString::fromStdString(key.latex), &err), "key LaTeX renders: " + QString::fromStdString(key.latex) + " " + err);
                    if (!key.shiftLatex.empty()) check(LatexRenderer::validate(QString::fromStdString(key.shiftLatex)), "shift LaTeX renders");
                    if (page.name == "Variables") keyLegends.insert(key.legend);
                }
        for (const auto& s : varSymbols) check(keyLegends.count(s) > 0, "variable on the keyboard: " + QString::fromStdString(s));
        check(varSymbols.size() == 55 && keys > 150, "55 variables and the scientific keys are on the keyboard");
        bool controls = true;
        for (const auto& page : pages) {
            std::set<std::string> c;
            for (const auto& key : page.rows.back()) if (!key.control.empty()) c.insert(key.control);
            controls = controls && c.count("@shift") && c.count("@left") && c.count("@right") && c.count("@back") && c.count("@clear");
        }
        check(controls, "every page ends with SHIFT ← → ⌫ AC");
        const auto groups = JsonKeyboardRepository(data + "/keyboard.json").loadAll();
        check(!groups.empty() && groups.back().name == "Variables" && groups.back().keys.size() == 55, "classic keyboard has the Variables group");
        check(ScientificKeyboard::caretOffset("\\sqrt{}") == 6 && ScientificKeyboard::caretOffset("\\sin()") == 5 && ScientificKeyboard::caretOffset("\\frac{}{}") == 6 &&
              ScientificKeyboard::caretOffset("\\left|\\right|") == 6 && ScientificKeyboard::caretOffset("\\lambda") == 7, "caret goes inside the first placeholder");
    } catch (const std::exception& e) { check(false, e.what()); }

    // ---- SVG diagrams ----
    const QStringList files = QDir(assets + "/diagrams").entryList({"*.svg"}, QDir::Files);
    check(files.size() == 6, "six SVG diagrams");
    const QRegularExpression token("@[A-Za-z]+@");
    const QStringList allowed = {"@fg@", "@muted@", "@red@", "@green@", "@yellow@", "@orange@", "@blue@", "@aqua@", "@card@"};
    for (bool darkMode : {true, false}) {
        ThemeManager::instance().apply(app, darkMode);
        for (const QString& name : files) {
            QFile file(assets + "/diagrams/" + name);
            check(file.open(QIODevice::ReadOnly), name + ": cannot be opened");    // open() is [[nodiscard]]
            const QByteArray raw = file.readAll();
            auto it = token.globalMatch(QString::fromUtf8(raw));
            while (it.hasNext()) check(allowed.contains(it.next().captured()), name + ": unknown colour token");
            const QByteArray themed = SvgDiagramView::applyTheme(raw);
            check(!themed.contains('@'), name + ": a colour token was not replaced");
            QSvgRenderer renderer(themed);
            check(renderer.isValid() && renderer.defaultSize().width() > 100, name + " renders");
        }
    }

    if (g_failures == 0) std::cout << "UI tests passed (" << latexCount << " LaTeX strings, " << files.size() << " diagrams x 2 themes)\n";
    return g_failures == 0 ? 0 : 1;
}
