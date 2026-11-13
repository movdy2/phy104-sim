#pragma once
#include <QFont>
#include <QPixmap>
#include <QSizeF>
#include <QString>
#include <QWidget>

namespace phy104 {

/// Small LaTeX layout engine built on QPainter (Qt has no LaTeX support).
/// Supported: letters/digits, + - = ( ) [ ] , ' ^ _ {..}, \frac, \sqrt, \left..\right,
/// \text \mathrm, \vec, Greek letters, \sin \cos \tan \log \ln, \times \cdot \circ \infty \pm ...
/// Output is a transparent high-DPI pixmap, so it scales smoothly on any screen.
class LatexRenderer {
public:
    static QPixmap render(const QString& latex, const QFont& font, const QColor& color, qreal devicePixelRatio = 2.0);
    static QSizeF measure(const QString& latex, const QFont& font);

    /// Readable unicode maths -> LaTeX:  "n₁ sin θ₁ = n₂ sin θ₂"  ->  n_{1} \\sin \\theta _{1} = ...
    ///   a / b -> \\frac{a}{b}, √(x) -> \\sqrt{x}, Greek letters, ₁ ₂ subscripts, ² ³ superscripts, × · ± ≈ → °.
    /// Text that already contains a backslash is treated as LaTeX and returned unchanged.
    static QString fromPlainMath(const QString& text);

    /// Syntax check (unknown command, unbalanced braces, missing argument).
    static bool validate(const QString& latex, QString* error = nullptr);
};

/// Shows one formula. Either on the normal background (Gruvbox text colour)
/// or on a Casio LCD screen (olive background, dark ink).
class LatexWidget : public QWidget {
    Q_OBJECT
public:
    explicit LatexWidget(QWidget* parent = nullptr);

    void setLatex(const QString& latex);
    void setPointSize(qreal points);
    void setLcdStyle(bool lcd);                 // true: draw an LCD screen behind the formula
    void setInkColor(const QColor& color);      // overrides the automatic colour (e.g. green answer)
    void setAlignLeft(bool left) { m_left = left; update(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QColor ink() const;

    QFont m_baseFont;
    QString m_latex;
    qreal m_points = 18.0;
    bool m_lcd = false;
    bool m_left = false;
    QColor m_inkOverride;
};

} // namespace phy104
