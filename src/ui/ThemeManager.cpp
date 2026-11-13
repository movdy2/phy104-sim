#include "ThemeManager.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QLineEdit>
#include <QPalette>
#include <QSettings>
#include <QStringList>
#include <QWidget>
#include <QStyleFactory>

namespace phy104 {

Theme Theme::figmaDark() {
    Theme t;
    t.dark = true;
    t.bg = "#282828";   t.sidebar = "#3c3836";  t.titleBar = "#1d2021";
    t.card = "#3c3836"; t.cardHover = "#504945"; t.input = "#282828";
    t.canvas = "#1d2021"; t.grid = "#2e2c2b";   t.border = "#504945";
    t.text = "#ebdbb2"; t.muted = "#d5c4a1";    t.dim = "#c7b9a8";   // small grey text: brighter, still quieter than the body
    t.amber = "#d79921"; t.amberHover = "#e5ac39"; t.amberPressed = "#b57614"; t.amberTint = QColor(215, 153, 33, 36);
    t.lime = "#b8bb26"; t.error = "#fb4934";    t.errorBg = "#442e2a";
    t.sage = "#8fb5a5";
    t.lcdBg = "#9bb094"; t.lcdText = "#0b1c08"; t.lcdFrame = "#171e15"; t.lcdDivider = "#708369";
    t.yellow = t.amber; t.green = "#98971a"; t.red = "#cc241d"; t.orange = "#d65d0e"; t.blue = "#458588"; t.aqua = "#689d6a";
    return t;
}

Theme Theme::figmaLight() {
    Theme t;
    t.dark = false;
    // the light theme of the Figma sheet was too white: paper, cards and canvas are all one step darker now
    t.bg = "#f0e2b8";   t.sidebar = "#e2d0a4";  t.titleBar = "#e9daae";
    t.card = "#e5d3a8"; t.cardHover = "#d9c69a"; t.input = "#f6ebcd";
    t.canvas = "#e8dab0"; t.grid = "#dccc9f";   t.border = "#cbb98d";
    t.text = "#3c3836"; t.muted = "#4f4a45";    t.dim = "#5c534d";
    t.amber = "#b57614"; t.amberHover = "#d79921"; t.amberPressed = "#9d6a0f"; t.amberTint = QColor(181, 118, 20, 34);
    t.lime = "#79740e"; t.error = "#9d0006";    t.errorBg = "#f4d9bf";
    t.sage = "#427b58";
    t.lcdBg = "#9bb094"; t.lcdText = "#0b1c08"; t.lcdFrame = "#171e15"; t.lcdDivider = "#708369";
    t.yellow = t.amber; t.green = "#79740e"; t.red = "#9d0006"; t.orange = "#af3a03"; t.blue = "#076678"; t.aqua = "#427b58";
    return t;
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

void ThemeManager::loadFonts() {
    for (const char* f : {":/fonts/Inter-Regular.otf", ":/fonts/Inter-Medium.otf", ":/fonts/Inter-SemiBold.otf",
                          ":/fonts/JetBrainsMono-Regular.ttf", ":/fonts/JetBrainsMono-Bold.ttf",
                          ":/fonts/DSEG7Classic-Bold.ttf", ":/fonts/DSEG14Classic-Bold.ttf"})
        QFontDatabase::addApplicationFont(f);
}

namespace {
/// One settings scope for the whole application (the organisation name of QSettings() would be empty otherwise).
QSettings* settingsScope() {
    static QSettings settings("PHY104", "PHY104OpticalAssistant");
    return &settings;
}
}

bool ThemeManager::storedDarkTheme() { return settingsScope()->value("ui/dark", true).toBool(); }

void ThemeManager::apply(QApplication& app, bool dark) {
    settingsScope()->setValue("ui/dark", dark);       // remembered for the next start (see main.cpp)
    m_theme = dark ? Theme::figmaDark() : Theme::figmaLight();
    const Theme& t = m_theme;

    app.setStyle(QStyleFactory::create("Fusion"));
    QFont font(uiFont());
    font.setPixelSize(13);
    app.setFont(font);

    QPalette p;
    p.setColor(QPalette::Window, t.bg);          p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.input);         p.setColor(QPalette::AlternateBase, t.card);
    p.setColor(QPalette::Text, t.text);          p.setColor(QPalette::Button, t.card);
    p.setColor(QPalette::ButtonText, t.text);    p.setColor(QPalette::Highlight, t.amber);
    p.setColor(QPalette::HighlightedText, t.bg); p.setColor(QPalette::ToolTipBase, t.card);
    p.setColor(QPalette::ToolTipText, t.text);   p.setColor(QPalette::PlaceholderText, t.dim);
    p.setColor(QPalette::Link, t.amber);
    p.setColor(QPalette::Disabled, QPalette::Text, t.dim);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.dim);
    app.setPalette(p);
    // A universal stylesheet font-family would override per-widget LCD segment fonts.
    app.setStyleSheet(styleSheet(t));
    emit themeChanged();
}

QFont ThemeManager::lcdFont(const QFont& base, bool numeric) {
    QFont font = base;
    font.setFamilies(numeric ? QStringList{"DSEG7 Classic", "DSEG14 Classic", "JetBrains Mono"}
                             : QStringList{"DSEG14 Classic", "JetBrains Mono"});
    return font;
}

void ThemeManager::polishLcdFont(QWidget* widget, bool numeric) {
    if (!widget) return;
    widget->setFont(lcdFont(widget->font(), numeric));
    QObject::connect(&instance(), &ThemeManager::themeChanged, widget, [widget, numeric] {
        widget->setFont(ThemeManager::lcdFont(widget->font(), numeric));
    });
}

void ThemeManager::polishLcdInput(QLineEdit* edit) {
    if (!edit) return;
    polishLcdFont(edit);
    QPalette p = edit->palette();
    QColor ink = Theme::figmaDark().lcdText;
    ink.setAlpha(115);
    p.setColor(QPalette::PlaceholderText, ink);
    edit->setPalette(p);
}

QString ThemeManager::styleSheet(const Theme& t) {
    // MSVC rejects a single string literal over 16380 bytes (C2026): the sheet is appended in three parts.
    QString css = R"CSS(
QMainWindow, QWidget#central, QWidget#pageArea { background: @bg@; }
QToolTip { background: @card@; color: @text@; border: 1px solid @amber@; padding: 4px 6px; }

/* ---------- sidebar ---------- */
QFrame#sidebar { background: @sidebar@; border: none; border-right: 1px solid @border@; }
QLabel#brandName { font-family: 'JetBrains Mono'; font-size: 22px; font-weight: 700; color: @amber@; }
QLabel#brandSub  { font-size: 13px; font-weight: 600; color: @text@; }
QLabel#brandTag  { font-family: 'JetBrains Mono'; font-size: 9px; color: @dim@; }
QLabel#navSection { font-family: 'JetBrains Mono'; font-size: 9px; color: @dim@; padding: 10px 14px 2px 14px; }
QPushButton#navItem { text-align: left; padding: 0 12px; border: none; border-left: 2px solid transparent; border-radius: 0;
    background: transparent; color: @text@; font-size: 12px; min-height: 40px; }
QPushButton#navItem:hover { background: @cardHover@; }
QPushButton#navItem:checked { background: @amberTint@; border-left: 2px solid @amber@; color: @amber@; font-weight: 500; }
QPushButton#navItemSmall { text-align: left; padding: 0 12px; border: none; border-left: 2px solid transparent; border-radius: 0;
    background: transparent; color: @text@; font-size: 12px; min-height: 34px; }
QPushButton#navItemSmall:hover { background: @cardHover@; }
QPushButton#navItemSmall:checked { background: @amberTint@; border-left: 2px solid @amber@; color: @amber@; font-weight: 500; }
QLabel#sessionCap { font-family: 'JetBrains Mono'; font-size: 9px; color: @dim@; }
QLabel#sessionText { font-size: 11px; color: @muted@; }
QLabel#engineReady { font-family: 'JetBrains Mono'; font-size: 9px; color: @lime@; }
QLabel#engineReady[paused="true"] { color: @error@; }
QLabel#statusText[error="true"] { color: @error@; }

/* ---------- page header ---------- */
QLabel#pageTitle { font-size: 24px; font-weight: 600; color: @text@; }
QLabel#pageSubtitle { font-size: 12px; color: @muted@; }
QLabel#unitsTag { font-family: 'JetBrains Mono'; font-size: 10px; color: @dim@; }
QLabel#sectionTitle { font-size: 13px; font-weight: 600; color: @amber@; }
QLabel#muted { color: @muted@; }
QLabel#caption { font-family: 'JetBrains Mono'; font-size: 9px; color: @dim@; }
QLabel#colourSwatch { border: 1px solid @border@; border-radius: 4px; min-height: 30px; }
QLabel#fieldLabel { font-size: 11px; color: @muted@; }
QLabel#hint { font-size: 11px; color: @dim@; }

/* ---------- panels ---------- */
QFrame#panel { background: @card@; border: 1px solid @border@; border-radius: 6px; }
QFrame#panelHeader { background: transparent; border: none; border-bottom: 1px solid @border@; border-radius: 0; }
QLabel#panelTitle { font-size: 12px; font-weight: 600; color: @text@; }
QLabel#panelMeta { font-family: 'JetBrains Mono'; font-size: 10px; color: @dim@; }
QFrame#card { background: @card@; border: 1px solid @border@; border-radius: 6px; }
QFrame#cardHot { background: @card@; border: 1px solid @border@; border-radius: 6px; }
QFrame#cardHot:hover { border-color: @amber@; }
QLabel#metricValue { font-family: 'JetBrains Mono'; font-size: 32px; font-weight: 400; color: @amber@; }
QLabel#metricTitle { font-family: 'JetBrains Mono'; font-size: 9px; font-weight: 700; color: @text@; }
QLabel#metricText { font-size: 11px; color: @muted@; }
QLabel#cardKicker { font-family: 'JetBrains Mono'; font-size: 9px; color: @dim@; }
QLabel#cardTitle { font-size: 16px; font-weight: 600; color: @text@; }
QLabel#cardText { font-size: 12px; color: @muted@; }
QLabel#cardResult { font-family: 'JetBrains Mono'; font-size: 10px; color: @sage@; }

/* ---------- fields ---------- */
QFrame#field { background: @input@; border: 1px solid @border@; border-radius: 4px; }
QFrame#field[focused="true"] { border: 2px solid @amber@; }
QFrame#field[error="true"] { border: 2px solid @error@; }
QFrame#field QLineEdit { background: transparent; border: none; padding: 0; color: @text@; font-family: 'JetBrains Mono'; font-size: 12px; selection-background-color: @amber@; selection-color: @bg@; }
QLabel#fieldError { color: @error@; font-size: 11px; }
QToolButton#exportMenu { background: transparent; border: 1px solid @border@; border-radius: 3px; padding: 7px 10px; }
QToolButton#exportMenu:hover { border-color: @amber@; background: @amberTint@; }
QToolButton#exportMenu::menu-indicator { image: none; }
QToolButton#browseExperiments { background: @input@; color: @text@; border: 1px solid @border@; border-radius: 3px; padding: 7px 10px; min-height: 30px; font-weight: 600; }
QToolButton#browseExperiments:hover { border-color: @amber@; background: @amberTint@; }
QMenu { background: @card@; color: @text@; border: 1px solid @border@; padding: 4px; }
QMenu::item { padding: 6px 20px 6px 14px; }
QMenu::item:selected { background: @amberTint@; color: @amber@; }
QToolButton#spinUp, QToolButton#spinDown { background: transparent; border: none; padding: 0; }
QToolButton#spinUp:hover, QToolButton#spinDown:hover { background: @cardHover@; border-radius: 2px; }
QLabel#unit { font-family: 'JetBrains Mono'; font-size: 11px; color: @dim@; }
QComboBox { background: @input@; color: @text@; border: 1px solid @border@; border-radius: 4px; padding: 0 10px; min-height: 30px;
    font-family: 'JetBrains Mono'; font-size: 12px; selection-background-color: @amber@; selection-color: @bg@; }
QComboBox:focus, QComboBox:on { border: 2px solid @amber@; padding: 0 9px; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow { image: url(@chevron@); width: 14px; height: 14px; }
QComboBox QAbstractItemView { background: @input@; color: @text@; border: 1px solid @border@; selection-background-color: @amber@; selection-color: @bg@; outline: 0; }
QLineEdit { background: @input@; color: @text@; border: 1px solid @border@; border-radius: 4px; padding: 5px 10px; selection-background-color: @amber@; selection-color: @bg@; }
QLineEdit:focus { border: 2px solid @amber@; padding: 4px 9px; }
QSpinBox, QDoubleSpinBox { background: @input@; color: @text@; border: 1px solid @border@; border-radius: 4px; padding: 3px 8px; font-family: 'JetBrains Mono'; }
QCheckBox { color: @text@; spacing: 8px; font-size: 12px; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid @border@; border-radius: 3px; background: @input@; }
QCheckBox::indicator:checked { background: @amber@; border-color: @amber@; image: url(:/icons/check.svg); }

)CSS";
    css += R"CSS(/* ---------- buttons ---------- */
QPushButton { background: @card@; color: @text@; border: 1px solid @border@; border-radius: 4px; padding: 6px 14px; font-size: 12px; }
QPushButton:hover { border-color: @amber@; }
QPushButton:pressed { background: @bg@; }
QPushButton:disabled { color: @dim@; background: @border@; }
QPushButton#primary { background: @amber@; color: #282828; border: 1px solid @amber@; font-weight: 600; font-size: 13px; min-height: 36px; }
QPushButton#primary:hover { background: @amberHover@; border-color: @amberHover@; }
QPushButton#primary:pressed { background: @amberPressed@; border-color: @amberPressed@; }
QPushButton#secondary { background: @input@; color: @text@; border: 1px solid @border@; font-weight: 600; font-size: 13px; min-height: 36px; }
QPushButton#secondary:hover { border-color: @amber@; }
QPushButton#outline { background: transparent; color: @text@; border: 1px solid @text@; font-weight: 500; font-size: 13px; min-height: 34px; padding: 0 16px; }
QPushButton#outline:hover { border-color: @amber@; color: @amber@; }
QPushButton#outlineAmber { background: transparent; color: @amber@; border: 1px solid @amber@; border-radius: 4px; font-weight: 600; font-size: 12px; }
QPushButton#outlineAmber:hover { background: @amberTint@; }
QPushButton#modeTab { background: @input@; color: @muted@; border: 1px solid @border@; border-radius: 4px; min-height: 30px; font-size: 12px; }
QPushButton#modeTab:hover { border-color: @amber@; }
QPushButton#modeTab:checked { background: @amberTint@; color: @amber@; border: 1px solid @amber@; font-weight: 500; }
QPushButton#flat { background: transparent; border: none; color: @dim@; font-family: 'JetBrains Mono'; font-size: 10px; padding: 2px 8px; }
QPushButton#flat:hover { color: @amber@; }
QPushButton#flat:checked { color: @amber@; }
QToolButton#fmt { background: transparent; border: 1px solid transparent; border-radius: 3px; padding: 3px 6px; color: @text@; font-family: 'JetBrains Mono'; font-weight: 700; }
QToolButton#fmt:hover { border-color: @border@; }
QToolButton#fmt:checked { background: @amberTint@; color: @amber@; }

/* ---------- calculator keypad ---------- */
QPushButton#key   { background: @card@;  color: @text@;  border: 1px solid @border@; border-radius: 4px; font-family: 'JetBrains Mono'; font-size: 15px; font-weight: 700; }
QPushButton#keyFn { background: @input@; color: @calcDim@;  border: 1px solid @border@; border-radius: 4px; font-family: 'JetBrains Mono'; font-size: 13px; }
QPushButton#calcMode { background: @input@; color: @calcDim@; border: 1px solid @border@; border-radius: 4px; padding: 5px 12px; font-family: 'JetBrains Mono'; font-size: 11px; font-weight: 700; }
QPushButton#calcMode:checked { border-color: @amber@; color: @calcDim@; }
QPushButton#keyOp { background: @cardHover@; color: @amber@; border: 1px solid @border@; border-radius: 4px; font-family: 'JetBrains Mono'; font-size: 15px; font-weight: 700; }
QPushButton#keyEq { background: @amber@; color: #282828; border: 1px solid @amber@; border-radius: 4px; font-family: 'JetBrains Mono'; font-size: 18px; font-weight: 700; }
QPushButton#key:hover, QPushButton#keyFn:hover, QPushButton#keyOp:hover { border-color: @amber@; }
QPushButton#key:pressed, QPushButton#keyFn:pressed, QPushButton#keyOp:pressed { background: @bg@; }
QPushButton#keyEq:hover { background: @amberHover@; border-color: @amberHover@; }
QPushButton#keyEq:pressed { background: @amberPressed@; }

/* ---------- Scientific input keyboard (raised keys) ---------- */
QFrame#sciKeyboard { background: @card@; border: 1px solid @border@; border-radius: 8px; }
QLabel#sciTitle { font-family: 'JetBrains Mono'; font-size: 10px; font-weight: 700; color: @amber@; }
QLabel#lcdKeyDisplay { font-size: 20px; font-weight: 700; color: @lcdText@; }
QToolButton#keyClose { background: transparent; border: none; color: @dim@; font-size: 16px; }
QToolButton#keyClose:hover { color: @amber@; }
QPushButton#rkey, QPushButton#rkeyCtl { background: @card@; color: @text@; border: 1px solid @border@; border-bottom: 3px solid @titleBar@; border-radius: 4px;
    font-family: 'JetBrains Mono'; font-size: 12px; padding: 2px 4px; }
QPushButton#rkeyCtl { color: @muted@; font-size: 11px; }
QPushButton#rkey:hover, QPushButton#rkeyCtl:hover { background: @cardHover@; }
QPushButton#rkey:pressed, QPushButton#rkeyCtl:pressed { background: @amber@; color: #282828; border-bottom: 1px solid @amber@; }
QPushButton#rkey:disabled { color: @dim@; }
QPushButton#rkeyCtl[latched="true"] { background: @amberTint@; color: @amber@; border-color: @amber@; }

/* ---------- study hub ---------- */
QLabel#studyTitle { font-size: 16px; font-weight: 600; color: @text@; }
QLabel#studyBody { font-size: 12px; color: @muted@; }
QFrame#ruleLine { border: none; border-top: 1px solid @border@; max-height: 1px; }
QListWidget#studyTopics { background: transparent; border: none; font-size: 12px; }
QListWidget#studyTopics::item { min-height: 30px; padding: 2px 6px; border-radius: 4px; }
QListWidget#studyTopics::item:selected { background: @amberTint@; color: @amber@; }

)CSS";
    css += R"CSS(/* ---------- notices ---------- */
QFrame#notice { background: @bg@; border: 1px solid @border@; border-radius: 6px; }
QFrame#notice[error="true"] { background: @errorBg@; border: 1px solid @error@; }
QLabel#noticeTitle { font-size: 12px; font-weight: 600; color: @amber@; }
QFrame#notice[error="true"] QLabel#noticeTitle { color: @error@; }
QLabel#noticeBody { font-size: 12px; color: @muted@; }
QLabel#mono { font-family: 'JetBrains Mono'; font-size: 11px; color: @sage@; }
QLabel#statusOk    { background: @bg@; color: @lime@;  border: 1px solid @lime@;  border-radius: 4px; padding: 7px 10px; font-weight: 600; }
QLabel#statusWarn  { background: @bg@; color: @amber@; border: 1px solid @amber@; border-radius: 4px; padding: 7px 10px; font-weight: 600; }
QLabel#statusError { background: @errorBg@; color: @error@; border: 1px solid @error@; border-radius: 4px; padding: 7px 10px; font-weight: 600; }

/* ---------- tables / lists / text ---------- */
QTableWidget { background: transparent; border: none; gridline-color: transparent; color: @text@; font-family: 'JetBrains Mono'; font-size: 11px; selection-background-color: transparent; }
QTableWidget::item { border-bottom: 1px solid @border@; padding: 0 4px; }
QHeaderView::section { background: transparent; color: @dim@; font-family: 'JetBrains Mono'; font-size: 9px; padding: 6px 4px; border: none; border-bottom: 1px solid @border@; text-align: left; }
QPlainTextEdit#mdEditor, QTextBrowser#mdPreview { background: @canvas@; color: @text@; border: 1px solid @border@; border-radius: 4px; padding: 8px; selection-background-color: @amber@; selection-color: @bg@; }
QPlainTextEdit#mdEditor:focus { border: 2px solid @amber@; padding: 7px; }
QToolButton#fmtAccent { background: @amberTint@; border: 1px solid @amber@; border-radius: 3px; padding: 3px 10px; color: @amber@; font-family: 'Inter'; font-weight: 600; }
QToolButton#fmtAccent { background: transparent; border: 1px solid @border@; border-radius: 3px; padding: 3px 10px; color: @text@; font-family: 'Inter'; font-weight: 600; }
QToolButton#fmtAccent:checked { background: @amberTint@; border-color: @amber@; color: @amber@; }
QToolButton#fmtAccent:hover { background: @amber@; color: @bg@; }
QToolButton#fmt::menu-indicator { image: none; }
QToolButton#notesKeyboard { background: transparent; border: 1px solid @border@; border-radius: 3px; padding: 4px 10px; color: @text@; font-family: 'Inter'; font-weight: 600; }
QToolButton#notesKeyboard:hover { background: @amber@; color: @bg@; }
QToolButton#notesKeyboard:checked { background: @amberTint@; border-color: @amber@; color: @amber@; }
QTableWidget#previewTable QHeaderView::section { color: @amber@; font-size: 10px; }
QTextBrowser, QTextEdit { background: transparent; color: @text@; border: none; selection-background-color: @amber@; selection-color: @bg@; }
QListWidget { background: @card@; color: @text@; border: 1px solid @border@; border-radius: 6px; outline: 0; }
QListWidget#diagramList, QListWidget#seriesList { padding: 4px; }
QListWidget#diagramList::item, QListWidget#seriesList::item { padding: 10px 12px; border-radius: 4px; }
QListWidget#diagramList::item:selected, QListWidget#seriesList::item:selected { background: @amber@; color: @bg@; font-weight: 600; }
QListWidget#diagramList::item:hover:!selected, QListWidget#seriesList::item:hover:!selected { background: @cardHover@; }
QTabWidget::pane { border: 1px solid @border@; border-radius: 6px; top: -1px; }
QTabBar::tab { background: @card@; color: @muted@; padding: 8px 14px; border: 1px solid @border@; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; }
QTabBar::tab:selected { background: @bg@; color: @amber@; font-weight: 600; }
QSlider::groove:horizontal { height: 6px; background: @border@; border-radius: 3px; }
QSlider::sub-page:horizontal { background: @amber@; border-radius: 3px; }
QSlider::handle:horizontal { background: @amber@; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }
QScrollArea { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @border@; border-radius: 5px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: @dim@; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: @border@; border-radius: 5px; min-width: 28px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* ---------- status bar ---------- */
QStatusBar { background: @titleBar@; color: @dim@; border-top: 1px solid @border@; }
QStatusBar::item { border: none; }
QLabel#statusText { font-family: 'JetBrains Mono'; font-size: 10px; color: @dim@; }

/* ---------- Casio LCD typography: shared by the calculator, solver and register displays ---------- */
QWidget#calcPage QLineEdit#lcdInput { font-size: 22px; font-weight: 700; padding: 10px 12px; }
QWidget#calcPage QLabel#lcdResult { font-size: 44px; font-weight: 700; padding: 8px 14px; }
QWidget#calcPage QTextBrowser#lcdScreen { font-size: 13px; font-weight: 700; }
QWidget#calcPage QLabel#lcdBadge { font-size: 11px; font-weight: 700; }
QWidget#calcPage QLabel#unitsTag { color: @calcDim@; }

/* ---------- Casio LCD ---------- */
QFrame#lcd { background: @lcdBg@; border: 3px solid @lcdFrame@; border-radius: 4px; }
QLabel#lcdTitle { font-size: 9px; font-weight: 700; color: @lcdText@; }
QLabel#lcdLine  { font-size: 11px; color: @lcdText@; }
QLabel#lcdCaption { font-size: 8px; color: @lcdText@; }
QLabel#lcdBig   { font-size: 26px; font-weight: 700; color: @lcdText@; }
QLabel#lcdFoot  { font-size: 8px; color: @lcdText@; }
QFrame#lcdDivider { background: @lcdDivider@; border: none; max-height: 1px; min-height: 1px; }
QLineEdit#lcdInput { background: @lcdBg@; color: @lcdText@; border: 3px solid @lcdFrame@; border-radius: 4px;
    font-size: 20px; font-weight: 700; padding: 8px 12px; selection-background-color: @lcdText@; selection-color: @lcdBg@; }
QLineEdit#lcdInput:focus { border: 3px solid @amber@; padding: 8px 12px; }
QTextBrowser#lcdScreen, QLabel#lcdScreen { background: @lcdBg@; color: @lcdText@; border: 3px solid @lcdFrame@; border-radius: 4px;
    font-size: 12px; font-weight: 700; padding: 8px; }
QLabel#lcdResult { background: @lcdBg@; color: @lcdText@; border: 3px solid @lcdFrame@; border-radius: 4px;
    font-size: 34px; font-weight: 700; padding: 6px 14px; }
QLabel#lcdBadge { background: @lcdText@; color: @lcdBg@; border-radius: 3px; padding: 2px 8px; font-size: 11px; font-weight: 700; }
)CSS";

    const std::pair<const char*, QColor> tokens[] = {
        {"@bg@", t.bg}, {"@sidebar@", t.sidebar}, {"@titleBar@", t.titleBar}, {"@card@", t.card}, {"@cardHover@", t.cardHover},
        {"@input@", t.input}, {"@border@", t.border}, {"@text@", t.text}, {"@muted@", t.muted}, {"@dim@", t.dim},
        {"@amberHover@", t.amberHover}, {"@amberPressed@", t.amberPressed}, {"@amber@", t.amber},
        {"@lime@", t.lime}, {"@errorBg@", t.errorBg}, {"@error@", t.error}, {"@sage@", t.sage},
        {"@canvas@", t.canvas}, {"@lcdBg@", t.lcdBg}, {"@lcdText@", t.lcdText}, {"@lcdFrame@", t.lcdFrame},
        {"@lcdDivider@", t.lcdDivider}, {"@calcDim@", t.dark ? QColor(Qt::white) : t.text}};
    for (const auto& [token, color] : tokens) css.replace(token, color.name());
    css.replace("@amberTint@", QString("rgba(%1,%2,%3,%4)").arg(t.amberTint.red()).arg(t.amberTint.green()).arg(t.amberTint.blue()).arg(t.amberTint.alpha()));
    css.replace("@chevron@", t.dark ? ":/icons/chevron-down-dark.svg" : ":/icons/chevron-down-light.svg");
    return css;
}

} // namespace phy104
