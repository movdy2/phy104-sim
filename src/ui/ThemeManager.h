#pragma once
#include <QColor>
#include <QFont>
#include <QObject>

class QApplication;
class QLineEdit;
class QWidget;

namespace phy104 {

/// All colours of one theme. The dark theme is the Figma design (Gruvbox dark + amber accent);
/// the light theme is its Gruvbox-light counterpart. Widgets that paint themselves read from here.
struct Theme {
    bool dark = true;
    // surfaces
    QColor bg, sidebar, titleBar, card, cardHover, input, canvas, grid, border;
    // text
    QColor text, muted, dim;
    // accent (primary action, titles, active navigation)
    QColor amber, amberHover, amberPressed, amberTint;
    // semantic
    QColor lime;        // valid / success
    QColor error, errorBg;
    QColor sage;        // optical-bench lines (interfaces, lens, prism outline)
    // Casio LCD (same olive screen in both modes)
    QColor lcdBg, lcdText, lcdFrame, lcdDivider;
    // older names still used by the SVG diagrams, LaTeX and formula cards
    QColor yellow, green, red, orange, blue, aqua;

    static Theme figmaDark();
    static Theme figmaLight();
    static Theme gruvboxDark() { return figmaDark(); }
    static Theme gruvboxLight() { return figmaLight(); }
};

/// Loads the bundled UI, mono and Casio segment fonts, applies palette + style sheet,
/// and announces theme changes.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance();

    static void loadFonts();                       // call once, before creating widgets
    static QString uiFont() { return "Inter"; }
    static QString monoFont() { return "JetBrains Mono"; }

    void apply(QApplication& app, bool dark);
    const Theme& theme() const { return m_theme; }
    bool isDark() const { return m_theme.dark; }
    /// The theme chosen in the last run (saved by apply(); read by main() before the UI is built).
    static bool storedDarkTheme();

    /// One shared Casio-style DSEG14 face for every LCD surface, with a readable fallback
    /// for glyphs the segmented font does not contain.
    static QFont lcdFont(const QFont& base = QFont());
    static void polishLcdFont(QWidget* widget);

    /// Call on every Casio-LCD input field (segmented font + readable placeholder colour).
    static void polishLcdInput(QLineEdit* edit);

signals:
    void themeChanged();

private:
    ThemeManager() = default;
    static QString styleSheet(const Theme& t);

    Theme m_theme = Theme::figmaDark();
};

} // namespace phy104
