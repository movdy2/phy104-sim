#include "LatexRenderer.h"
#include "ThemeManager.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QRegularExpression>
#include <QPainterPath>
#include <functional>
#include <optional>
#include <QtMath>
#include <map>
#include <vector>

namespace phy104 {
namespace {

// ------------------------------------------------------------------ boxes
// A Box knows its size (width, ascent above baseline, descent below) and how to paint itself.
struct Box {
    qreal w = 0, a = 0, d = 0;
    std::function<void(QPainter&, qreal x, qreal baseline)> paint = [](QPainter&, qreal, qreal) {};   // empty box: draws nothing (never a null call)
};

enum class Kind { Ord, Bin, Rel, Open, Close, Func };
struct Item { Box box; Kind kind = Kind::Ord; };

QFont makeFont(const QFont& base, qreal pixelSize, bool italic) {
    QFont f = base;
    f.setFamilies({"STIX Two Text", "Cambria", "DejaVu Serif", "Times New Roman", "serif"});
    f.setPixelSize(qMax(1, qRound(pixelSize)));
    f.setItalic(italic);
    f.setBold(false);
    return f;
}

Box makeSpace(qreal w) { Box b; b.w = w; b.paint = [](QPainter&, qreal, qreal) {}; return b; }

Box makeText(const QString& s, const QFont& base, qreal size, bool italic) {
    const QFont f = makeFont(base, size, italic);
    Box b;
    b.w = QFontMetricsF(f).horizontalAdvance(s);
    b.a = 0.74 * size;                                  // uniform cap height keeps rows even
    b.d = 0.22 * size;
    b.paint = [=](QPainter& p, qreal x, qreal base) { p.setFont(f); p.drawText(QPointF(x, base), s); };
    return b;
}

Box hcat(const std::vector<Box>& boxes) {
    Box r;
    for (const auto& b : boxes) { r.w += b.w; r.a = qMax(r.a, b.a); r.d = qMax(r.d, b.d); }
    r.paint = [boxes](QPainter& p, qreal x, qreal base) {
        for (const auto& b : boxes) { if (b.paint) b.paint(p, x, base); x += b.w; }
    };
    return r;
}

Box makeScripts(const Box& base, const Box* sub, const Box* sup, qreal size) {
    const qreal supUp = qMax(0.40 * size, base.a - 0.55 * size);
    const qreal subDown = 0.22 * size;
    Box r;
    r.w = base.w + qMax(sup ? sup->w : 0, sub ? sub->w : 0) + 0.04 * size;
    r.a = qMax(base.a, sup ? supUp + sup->a : 0);
    r.d = qMax(base.d, sub ? subDown + sub->d : 0);
    Box b = base; Box su = sup ? *sup : Box(); Box sb = sub ? *sub : Box();
    const bool hasSup = sup != nullptr, hasSub = sub != nullptr;
    r.paint = [=](QPainter& p, qreal x, qreal base0) {
        b.paint(p, x, base0);
        if (hasSup) su.paint(p, x + b.w + 0.02 * size, base0 - supUp);
        if (hasSub) sb.paint(p, x + b.w + 0.02 * size, base0 + subDown);
    };
    return r;
}

Box makeFrac(const Box& num, const Box& den, qreal size) {
    const qreal bar = 0.27 * size, gap = 0.16 * size;
    Box r;
    r.w = qMax(num.w, den.w) + 0.3 * size;
    r.a = bar + gap + num.d + num.a;
    r.d = qMax<qreal>(0, -bar + gap + den.a + den.d);
    r.paint = [=](QPainter& p, qreal x, qreal base) {
        const qreal w = qMax(num.w, den.w) + 0.3 * size;
        num.paint(p, x + (w - num.w) / 2, base - bar - gap - num.d);
        den.paint(p, x + (w - den.w) / 2, base - bar + gap + den.a);
        p.save();
        QPen pen = p.pen(); pen.setWidthF(0.06 * size); pen.setCapStyle(Qt::FlatCap); p.setPen(pen);
        p.drawLine(QPointF(x + 0.1 * size, base - bar), QPointF(x + w - 0.1 * size, base - bar));
        p.restore();
    };
    return r;
}

Box makeSqrt(const Box& c, qreal size) {
    const qreal rw = 0.62 * size, top = 0.14 * size;
    Box r;
    r.w = rw + c.w + 0.12 * size;
    r.a = c.a + top + 0.08 * size;
    r.d = c.d;
    r.paint = [=](QPainter& p, qreal x, qreal base) {
        const qreal barY = base - c.a - top;
        QPainterPath path;
        path.moveTo(x + 0.02 * size, base - 0.30 * size);
        path.lineTo(x + 0.14 * size, base - 0.36 * size);
        path.lineTo(x + 0.32 * size, base + c.d);
        path.lineTo(x + rw, barY);
        path.lineTo(x + rw + c.w + 0.12 * size, barY);
        p.save();
        QPen pen = p.pen(); pen.setWidthF(0.06 * size); pen.setJoinStyle(Qt::MiterJoin); p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.restore();
        c.paint(p, x + rw, base);
    };
    return r;
}

// stretchy delimiter drawn as a path; kind: ( ) [ ] | or '.' (invisible)
Box makeDelimiter(QChar kind, qreal top, qreal bottom, qreal size) {
    Box r;
    const qreal dw = kind == '.' ? 0.0 : (kind == '|' ? 0.18 * size : 0.34 * size);
    r.w = dw;
    r.a = top; r.d = bottom;
    r.paint = [=](QPainter& p, qreal x, qreal base) {
        if (kind == '.') return;
        const qreal y0 = base - top, y1 = base + bottom, ym = (y0 + y1) / 2;
        QPainterPath path;
        if (kind == '(')      { path.moveTo(x + dw * 0.8, y0); path.quadTo(x - dw * 0.1, ym, x + dw * 0.8, y1); }
        else if (kind == ')') { path.moveTo(x + dw * 0.2, y0); path.quadTo(x + dw * 1.1, ym, x + dw * 0.2, y1); }
        else if (kind == '[') { path.moveTo(x + dw * 0.8, y0); path.lineTo(x + dw * 0.25, y0); path.lineTo(x + dw * 0.25, y1); path.lineTo(x + dw * 0.8, y1); }
        else if (kind == ']') { path.moveTo(x + dw * 0.2, y0); path.lineTo(x + dw * 0.75, y0); path.lineTo(x + dw * 0.75, y1); path.lineTo(x + dw * 0.2, y1); }
        else                  { path.moveTo(x + dw / 2, y0); path.lineTo(x + dw / 2, y1); }
        p.save();
        QPen pen = p.pen(); pen.setWidthF(0.06 * size); p.setPen(pen); p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.restore();
    };
    return r;
}

Box makeVec(const Box& c, qreal size) {
    Box r = c;
    r.a = c.a + 0.2 * size;
    r.paint = [=](QPainter& p, qreal x, qreal base) {
        c.paint(p, x, base);
        const qreal y = base - c.a - 0.08 * size, x0 = x + 0.1 * size, x1 = x + c.w;
        p.save();
        QPen pen = p.pen(); pen.setWidthF(0.05 * size); p.setPen(pen);
        p.drawLine(QPointF(x0, y), QPointF(x1, y));
        QPainterPath head;
        head.moveTo(x1, y); head.lineTo(x1 - 0.12 * size, y - 0.06 * size); head.lineTo(x1 - 0.12 * size, y + 0.06 * size); head.closeSubpath();
        p.setBrush(pen.color()); p.drawPath(head);
        p.restore();
    };
    return r;
}

// ------------------------------------------------------------------ parser
const std::map<QString, QString> kGreekLower = {
    {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"}, {"epsilon", "ε"}, {"varepsilon", "ε"},
    {"theta", "θ"}, {"lambda", "λ"}, {"mu", "μ"}, {"pi", "π"}, {"phi", "φ"}, {"varphi", "φ"}, {"rho", "ρ"},
    {"sigma", "σ"}, {"tau", "τ"}, {"omega", "ω"}, {"nu", "ν"}, {"eta", "η"}, {"kappa", "κ"}, {"psi", "ψ"},
    {"zeta", "ζ"}, {"xi", "ξ"}, {"chi", "χ"}, {"iota", "ι"}};
const std::map<QString, QString> kGreekUpper = {
    {"Delta", "Δ"}, {"Omega", "Ω"}, {"Sigma", "Σ"}, {"Phi", "Φ"}, {"Theta", "Θ"}, {"Lambda", "Λ"}, {"Pi", "Π"},
    {"Gamma", "Γ"}, {"Xi", "Ξ"}, {"Psi", "Ψ"}};
const std::map<QString, QString> kSymbols = {          // upright symbols: command -> text, kind
    {"times", "×"}, {"cdot", "·"}, {"circ", "°"}, {"infty", "∞"}, {"pm", "±"}, {"approx", "≈"},
    {"neq", "≠"}, {"leq", "≤"}, {"geq", "≥"}, {"propto", "∝"}, {"partial", "∂"}, {"degree", "°"},
    {"div", "÷"}, {"int", "∫"}, {"to", "→"}, {"rightarrow", "→"}};
const char* const kFunctions[] = {"sin", "cos", "tan", "sec", "csc", "cot", "arcsin", "arccos", "arctan",
                                  "log", "ln", "max", "min", "exp", "sinh", "cosh", "tanh"};

class Parser {
public:
    Parser(const QString& s, const QFont& font, qreal size) : m_s(s), m_font(font), m_size(size) {}

    Box parse() {
        Box b = parseRow(false, false);
        if (m_i < m_s.size() && m_error.isEmpty()) m_error = "Unexpected '}' or \\right";
        return b;
    }
    const QString& error() const { return m_error; }

private:
    QString m_s;
    QFont m_font;
    qreal m_size;
    int m_i = 0;
    QString m_error;

    bool atEnd() const { return m_i >= m_s.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_s[m_i]; }
    void skipSpaces() { while (!atEnd() && m_s[m_i].isSpace()) ++m_i; }
    void fail(const QString& msg) { if (m_error.isEmpty()) m_error = msg; }
    bool lookingAt(const QString& text) const { return m_s.mid(m_i, text.size()) == text; }

    Box text(const QString& s, bool italic) { return makeText(s, m_font, m_size, italic); }

    QString readCommandName() {                 // after the backslash
        int start = m_i;
        while (!atEnd() && m_s[m_i].isLetter()) ++m_i;
        if (m_i == start && !atEnd()) ++m_i;    // single-char command like \,
        return m_s.mid(start, m_i - start);
    }

    Box parseRow(bool untilBrace, bool untilRight) {
        std::vector<Item> items;
        for (;;) {
            skipSpaces();
            if (atEnd()) break;
            if (peek() == '}') { if (untilBrace) break; fail("Unbalanced '}'"); ++m_i; continue; }
            if (lookingAt("\\right")) { if (untilRight) break; fail("\\right without \\left"); m_i += 6; continue; }

            Item item = parseAtom(false);
            // scripts and primes
            std::optional<Box> sub, sup;
            for (;;) {
                skipSpaces();
                if (peek() == '^') { ++m_i; sup = parseScript(); }
                else if (peek() == '_') { ++m_i; sub = parseScript(); }
                else if (peek() == '\'') { ++m_i; sup = primeBox(); }
                else break;
            }
            if (sub || sup) {
                item.box = makeScripts(item.box, sub ? &*sub : nullptr, sup ? &*sup : nullptr, m_size);
            }
            items.push_back(std::move(item));
        }
        return combine(items);
    }

    Box primeBox() {                            // the prime is drawn as a short slanted stroke (fonts draw it tiny)
        const qreal s = m_size;
        Box b;
        b.w = 0.22 * s; b.a = 0.55 * s; b.d = 0;
        b.paint = [s](QPainter& p, qreal x, qreal base) {
            p.save();
            QPen pen = p.pen(); pen.setWidthF(0.075 * s); pen.setCapStyle(Qt::RoundCap); p.setPen(pen);
            p.drawLine(QPointF(x + 0.04 * s, base - 0.12 * s), QPointF(x + 0.14 * s, base - 0.55 * s));
            p.restore();
        };
        return b;
    }
    Box parseScript() {
        const qreal saved = m_size; m_size *= 0.7;
        Box b = parseArg();
        m_size = saved;
        return b;
    }

    Box parseArg() {
        skipSpaces();
        if (peek() == '{') {
            ++m_i;
            Box b = parseRow(true, false);
            if (peek() == '}') ++m_i; else fail("Missing '}'");
            return b;
        }
        if (atEnd()) { fail("Missing argument"); return Box(); }
        return parseAtom(true).box;
    }

    // Puts spacing around relations and binary operators.
    Box combine(const std::vector<Item>& items) {
        std::vector<Box> out;
        for (std::size_t k = 0; k < items.size(); ++k) {
            const Kind kind = items[k].kind;
            const Kind prev = k > 0 ? items[k - 1].kind : Kind::Open;
            qreal before = 0, after = 0;
            if (kind == Kind::Rel) { before = after = 0.28 * m_size; }
            else if (kind == Kind::Bin) {
                const bool unary = prev == Kind::Open || prev == Kind::Bin || prev == Kind::Rel;
                if (!unary) before = after = 0.22 * m_size;
            }
            else if (kind == Kind::Func) {
                after = 0.10 * m_size;
                if (prev == Kind::Ord || prev == Kind::Close) before = 0.12 * m_size;
            }
            if (before > 0) out.push_back(makeSpace(before));
            out.push_back(items[k].box);
            if (after > 0) out.push_back(makeSpace(after));
        }
        return hcat(out);
    }

    Item parseAtom(bool singleChar) {
        const QChar c = peek();
        if (c == '{') {
            ++m_i;
            Box b = parseRow(true, false);
            if (peek() == '}') ++m_i; else fail("Missing '}'");
            return {b, Kind::Ord};
        }
        if (c == '\\') { ++m_i; return parseCommand(); }
        if (c.isDigit() || c == '.') {
            int start = m_i;
            ++m_i;
            while (!singleChar && !atEnd() && (m_s[m_i].isDigit() || m_s[m_i] == '.')) ++m_i;
            return {text(m_s.mid(start, m_i - start), false), Kind::Ord};
        }
        if (c.isLetter()) { ++m_i; return {text(QString(c), true), Kind::Ord}; }
        ++m_i;
        switch (c.unicode()) {
        case '+': return {text("+", false), Kind::Bin};
        case '-': return {text("−", false), Kind::Bin};
        case '=': return {text("=", false), Kind::Rel};
        case '<': case '>': return {text(QString(c), false), Kind::Rel};
        case '(': case '[': return {text(QString(c), false), Kind::Open};
        case ')': case ']': return {text(QString(c), false), Kind::Close};
        case ',': return {hcat({text(",", false), makeSpace(0.15 * m_size)}), Kind::Ord};
        case '/': return {text("/", false), Kind::Ord};
        case '|': return {text("|", false), Kind::Ord};
        default:  return {text(QString(c), false), Kind::Ord};
        }
    }

    Item parseCommand() {
        const QString name = readCommandName();
        if (name == "frac" || name == "dfrac") {
            const qreal saved = m_size; m_size *= 0.92;
            Box num = parseArg(); Box den = parseArg();
            m_size = saved;
            return {makeFrac(num, den, m_size), Kind::Ord};
        }
        if (name == "sqrt") {
            skipSpaces();
            if (peek() == '[') { while (!atEnd() && peek() != ']') ++m_i; ++m_i; }   // \sqrt[n]{..}: n ignored
            return {makeSqrt(parseArg(), m_size), Kind::Ord};
        }
        if (name == "left") {
            skipSpaces();
            const QChar open = readDelimiter();
            Box inner = parseRow(false, true);
            QChar close = '.';
            if (lookingAt("\\right")) { m_i += 6; skipSpaces(); close = readDelimiter(); }
            else fail("\\left without \\right");
            const qreal top = inner.a + 0.10 * m_size, bottom = inner.d + 0.10 * m_size;
            return {hcat({makeDelimiter(open, top, bottom, m_size), inner, makeDelimiter(close, top, bottom, m_size)}), Kind::Ord};
        }
        if (name == "text" || name == "mathrm" || name == "mathbf" || name == "operatorname") {
            skipSpaces();
            if (peek() != '{') { fail("\\" + name + " needs {..}"); return {Box(), Kind::Ord}; }
            ++m_i;
            int start = m_i;
            while (!atEnd() && peek() != '}') ++m_i;
            const QString raw = m_s.mid(start, m_i - start);
            if (peek() == '}') ++m_i; else fail("Missing '}'");
            return {text(raw, false), Kind::Ord};
        }
        if (name == "vec" || name == "overrightarrow") return {makeVec(parseArg(), m_size), Kind::Ord};

        if (kGreekLower.count(name)) return {text(kGreekLower.at(name), true), Kind::Ord};
        if (kGreekUpper.count(name)) return {text(kGreekUpper.at(name), false), Kind::Ord};
        if (kSymbols.count(name)) {
            const bool bin = (name == "times" || name == "cdot" || name == "pm" || name == "div");
            const bool rel = (name == "approx" || name == "neq" || name == "leq" || name == "geq" || name == "propto");
            return {text(kSymbols.at(name), false), bin ? Kind::Bin : (rel ? Kind::Rel : Kind::Ord)};
        }
        for (const char* f : kFunctions)
            if (name == f) return {text(name, false), Kind::Func};
        if (name == "," || name == ";" || name == " " || name == "quad" || name == "qquad" || name == "!") {
            const qreal w = name == "quad" ? 1.0 : (name == "qquad" ? 2.0 : (name == "!" ? 0.0 : 0.18));
            return {makeSpace(w * m_size), Kind::Ord};
        }
        if (name == "{" || name == "}") return {text(name, false), Kind::Ord};

        fail("Unknown command \\" + name);
        return {text("\\" + name, false), Kind::Ord};
    }

    QChar readDelimiter() {
        if (atEnd()) { fail("Missing delimiter"); return '.'; }
        if (peek() == '\\') { ++m_i; const QString n = readCommandName(); return n == "{" ? QChar('(') : QChar('.'); }
        const QChar c = peek(); ++m_i;
        return c;
    }
};

struct Layout {
    Box box;
    QString error;
};

qreal pixelSizeOf(const QFont& font) {
    if (font.pixelSize() > 0) return font.pixelSize();
    const qreal pt = font.pointSizeF() > 0 ? font.pointSizeF() : 12.0;
    return pt * 96.0 / 72.0;
}

Layout layout(const QString& latex, const QFont& font) {
    const qreal size = pixelSizeOf(font);
    Parser parser(latex, font, size);
    Layout l;
    l.box = parser.parse();
    l.error = parser.error();
    return l;
}

} // namespace

// ---------------------------------------------------------------------------- public API
QSizeF LatexRenderer::measure(const QString& latex, const QFont& font) {
    const Layout l = layout(latex, font);
    return QSizeF(l.box.w + 4, l.box.a + l.box.d + 4);
}

QPixmap LatexRenderer::render(const QString& latex, const QFont& font, const QColor& color, qreal dpr) {
    const Layout l = layout(latex, font);
    const QSizeF size(l.box.w + 4, l.box.a + l.box.d + 4);
    QPixmap pixmap(qCeil(size.width() * dpr), qCeil(size.height() * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    QPen pen(color); pen.setWidthF(1.0); p.setPen(pen);
    if (l.box.paint) l.box.paint(p, 2, 2 + l.box.a);
    return pixmap;
}

QString LatexRenderer::fromPlainMath(const QString& input) {
    if (input.contains('\\')) return input;                                          // already LaTeX
    QString s = input;
    // fractions: "n₂ / n₁", "1/f", "λL / d" (operands are runs of letters, digits and subscripts)
    static const QRegularExpression frac(QString::fromUtf8("([\\p{L}\\p{N}'₀-₉ₐₑᵢₒₓₙₘ]+)\\s*/\\s*([\\p{L}\\p{N}'₀-₉ₐₑᵢₒₓₙₘ]+)"));
    s.replace(frac, "\\frac{\\1}{\\2}");
    static const QRegularExpression root1(QString::fromUtf8("√\\(([^()]*)\\)")), root2(QString::fromUtf8("√([\\p{L}\\p{N}]+)"));
    s.replace(root1, "\\sqrt{\\1}");
    s.replace(root2, "\\sqrt{\\1}");
    static const QRegularExpression funcs("\\b(arcsin|arccos|arctan|asin|acos|atan|sinh|cosh|tanh|sin|cos|tan|ln|log|exp)\\b");
    s.replace(funcs, "\\\\1 ");
    s.replace("\\asin", "\\arcsin").replace("\\acos", "\\arccos").replace("\\atan", "\\arctan");
    static const QList<QPair<QString, QString>> symbols = {
        {"×", "\\times "}, {"·", "\\cdot "}, {"÷", "\\div "}, {"−", "-"}, {"≈", "\\approx "}, {"≠", "\\neq "}, {"≤", "\\leq "}, {"≥", "\\geq "},
        {"→", "\\to "}, {"±", "\\pm "}, {"∞", "\\infty "}, {"°", "^{\\circ}"}, {"∝", "\\propto "}};
    for (const auto& [from, to] : symbols) s.replace(from, to);
    static const QString greekLower = QString::fromUtf8("αβγδεζηθικλμνξπρστφχψω"), greekUpper = QString::fromUtf8("ΓΔΘΛΞΠΣΦΨΩ");
    static const char* lowerNames[] = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta", "theta", "iota", "kappa", "lambda", "mu", "nu", "xi", "pi", "rho", "sigma", "tau", "phi", "chi", "psi", "omega"};
    static const char* upperNames[] = {"Gamma", "Delta", "Theta", "Lambda", "Xi", "Pi", "Sigma", "Phi", "Psi", "Omega"};
    QString out;
    for (const QChar ch : s) {
        const int lo = greekLower.indexOf(ch), up = greekUpper.indexOf(ch);
        if (lo >= 0) out += QString("\\") + lowerNames[lo] + " ";
        else if (up >= 0) out += QString("\\") + upperNames[up] + " ";
        else out += ch;
    }
    // subscripts / superscripts: runs become _{...} and ^{...}
    static const QString subFrom = QString::fromUtf8("₀₁₂₃₄₅₆₇₈₉ₐₑᵢₒₓₙₘ"), subTo = "0123456789aeioxnm";
    static const QString supFrom = QString::fromUtf8("⁰¹²³⁴⁵⁶⁷⁸⁹⁻"), supTo = "0123456789-";
    static const QRegularExpression subRun(QString::fromUtf8("[₀-₉ₐₑᵢₒₓₙₘ]+")), supRun(QString::fromUtf8("[⁰¹²³⁴⁵⁶⁷⁸⁹⁻]+"));
    auto convert = [&out](const QRegularExpression& re, const QString& from, const QString& to, const char* op) {
        QString result;
        int last = 0;
        for (auto it = re.globalMatch(out); it.hasNext();) {
            const auto m = it.next();
            result += out.mid(last, m.capturedStart() - last) + op + "{";
            for (const QChar c : m.captured()) result += to.at(from.indexOf(c));
            result += "}";
            last = static_cast<int>(m.capturedEnd());
        }
        out = result + out.mid(last);
    };
    convert(subRun, subFrom, subTo, "_");
    convert(supRun, supFrom, supTo, "^");
    return out.simplified();
}

bool LatexRenderer::validate(const QString& latex, QString* error) {
    QFont f; f.setPixelSize(16);
    const Layout l = layout(latex, f);
    if (error) *error = l.error;
    return l.error.isEmpty();
}

// ---------------------------------------------------------------------------- widget
LatexWidget::LatexWidget(QWidget* parent) : QWidget(parent), m_baseFont(QWidget::font()) {
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        if (m_lcd) setFont(ThemeManager::lcdFont(m_baseFont));
        update();
    });
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void LatexWidget::setLatex(const QString& latex) { m_latex = latex; updateGeometry(); update(); }
void LatexWidget::setPointSize(qreal points)    { m_points = points; updateGeometry(); update(); }
void LatexWidget::setLcdStyle(bool lcd) {
    m_lcd = lcd;
    setFont(lcd ? ThemeManager::lcdFont(m_baseFont) : m_baseFont);
    updateGeometry();
    update();
}
void LatexWidget::setInkColor(const QColor& c)  { m_inkOverride = c; update(); }

QColor LatexWidget::ink() const {
    if (m_inkOverride.isValid()) return m_inkOverride;
    const Theme& t = ThemeManager::instance().theme();
    return m_lcd ? t.lcdText : t.text;
}

QSize LatexWidget::sizeHint() const {
    QFont f = font(); f.setPointSizeF(m_points);
    const QSizeF s = LatexRenderer::measure(m_latex, f);
    const int pad = m_lcd ? 14 : 4;
    return QSize(qCeil(s.width()) + 2 * pad, qMax(qCeil(s.height()) + 2 * pad, m_lcd ? 56 : 32));
}
QSize LatexWidget::minimumSizeHint() const { return sizeHint(); }

void LatexWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Theme& t = ThemeManager::instance().theme();
    if (m_lcd) {
        p.setPen(QPen(t.lcdFrame, 3));
        p.setBrush(t.lcdBg);
        p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 8, 8);
    }
    if (m_latex.isEmpty()) return;
    QFont f = font(); f.setPointSizeF(m_points);
    const QPixmap pix = LatexRenderer::render(m_latex, f, ink(), devicePixelRatioF() * 2.0);
    const QSizeF logical = pix.deviceIndependentSize();
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const double x = m_left ? (m_lcd ? 14.0 : 2.0) : (width() - logical.width()) / 2;
    p.drawPixmap(QPointF(x, (height() - logical.height()) / 2), pix);
}

} // namespace phy104
