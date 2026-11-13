#include "MainWindow.h"
#include "AppContext.h"
#include "BasePage.h"
#include "Icons.h"
#include "ThemeManager.h"
#include "VirtualKeyboard.h"

#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QShortcut>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace phy104 {
namespace {

/// Sidebar item. The background / border come from the style sheet; icon and label are painted here
/// so they are always aligned and vertically centred (also for the two-line "Wave Interference").
class NavButton : public QPushButton {
public:
    NavButton(const QString& title, const QString& icon, QWidget* parent = nullptr) : QPushButton(parent), m_title(title), m_icon(icon) {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAccessibleName(title);
        setToolTip(QString(title).replace('\n', ' '));
        refreshPixmaps();
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { refreshPixmaps(); update(); });
    }
    QString title() const { return m_title; }

protected:
    void paintEvent(QPaintEvent* e) override {
        QPushButton::paintEvent(e);                                   // style-sheet background and border
        const Theme& t = ThemeManager::instance().theme();
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        const int iconSize = 18, left = 14;
        p.drawPixmap(QRect(left, (height() - iconSize) / 2, iconSize, iconSize), isChecked() ? m_on : m_off);
        QFont f(ThemeManager::uiFont());
        f.setPixelSize(12);
        f.setWeight(isChecked() ? QFont::Medium : QFont::Normal);
        p.setFont(f);
        p.setPen(isChecked() ? t.amber : t.text);
        const int textLeft = left + iconSize + 12;
        p.drawText(QRect(textLeft, 0, width() - textLeft - 8, height()), Qt::AlignVCenter | Qt::AlignLeft, m_title);
    }

private:
    void refreshPixmaps() {
        const Theme& t = ThemeManager::instance().theme();
        m_off = Icons::pixmap(m_icon, t.text, 18);
        m_on = Icons::pixmap(m_icon, t.amber, 18);
    }
    QString m_title, m_icon;
    QPixmap m_off, m_on;
};

} // namespace

MainWindow::MainWindow(AppContext* ctx, const std::vector<KeyboardGroup>& keyboardLayout, QWidget* parent)
    : QMainWindow(parent), m_ctx(ctx) {
    setWindowTitle("PHY104 Optical Assistant — " + ctx->sessionFile);
    const auto applyWindowIcon = [this] {
        const Theme& t = ThemeManager::instance().theme();
        setWindowIcon(Icons::get("scan-line", t.amber, t.amber, 64));     // taskbar / alt-tab icon
    };
    applyWindowIcon();
    resize(1280, 800);
    setMinimumSize(1080, 700);

    auto* central = new QWidget;
    central->setObjectName("central");
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ------------------------------------------------------------------ sidebar
    auto* sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(224);
    auto* sl = new QVBoxLayout(sidebar);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);

    auto* brand = new QWidget;
    auto* bl = new QVBoxLayout(brand);
    bl->setContentsMargins(20, 22, 20, 18);
    bl->setSpacing(2);
    auto* logoRow = new QHBoxLayout;
    logoRow->setSpacing(8);
    auto* logo = new QLabel;
    logo->setPixmap(Icons::pixmap("scan-line", ThemeManager::instance().theme().amber, 28));
    logo->setAlignment(Qt::AlignVCenter);
    auto* name = new QLabel("PHY104");
    name->setObjectName("brandName");
    name->setAlignment(Qt::AlignVCenter);
    logoRow->addWidget(logo);
    logoRow->addWidget(name);
    logoRow->addStretch();
    auto* sub = new QLabel("Optical Assistant");
    sub->setObjectName("brandSub");
    auto* tag = new QLabel("COMPUTATIONAL OPTICS / v1.4");
    tag->setObjectName("brandTag");
    bl->addLayout(logoRow);
    bl->addSpacing(6);
    bl->addWidget(sub);
    bl->addWidget(tag);
    sl->addWidget(brand);

    auto* navHolder = new QWidget;
    m_modules = new QVBoxLayout(navHolder);
    m_modules->setContentsMargins(12, 6, 12, 0);
    m_modules->setSpacing(4);
    sl->addWidget(navHolder);

    m_toolsHeader = new QLabel("TOOLS");
    m_toolsHeader->setObjectName("navSection");
    sl->addWidget(m_toolsHeader);
    auto* toolsHolder = new QWidget;
    m_tools = new QVBoxLayout(toolsHolder);
    m_tools->setContentsMargins(12, 0, 12, 0);
    m_tools->setSpacing(2);
    sl->addWidget(toolsHolder);
    sl->addStretch(1);

    auto* session = new QWidget;
    auto* ssl = new QVBoxLayout(session);
    ssl->setContentsMargins(20, 0, 20, 16);
    ssl->setSpacing(4);
    auto* cap = new QLabel(QString("LAB SESSION / %1").arg(ctx->log.sessionNumber(), 2, 10, QChar('0')));
    cap->setObjectName("sessionCap");
    auto* what = new QLabel("Undergraduate physics lab");
    what->setObjectName("sessionText");
    m_engine = new QLabel("● ENGINE READY");
    m_engine->setObjectName("engineReady");
    auto* local = new QLabel("Local workspace · autosaved");
    local->setObjectName("sessionText");
    ssl->addWidget(cap);
    ssl->addWidget(what);
    ssl->addWidget(m_engine);
    ssl->addWidget(local);
    sl->addWidget(session);
    root->addWidget(sidebar);

    // ------------------------------------------------------------------ pages + keyboard
    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(0);
    m_stack = new QStackedWidget;
    right->addWidget(m_stack, 1);
    m_keyboard = new VirtualKeyboard(keyboardLayout);
    m_keyboard->setVisible(false);
    m_keyboard->setFallbackProvider([this] { return fallbackField(); });
    auto* keyboardFrame = new QFrame;
    keyboardFrame->setObjectName("card");
    auto* kl = new QVBoxLayout(keyboardFrame);
    kl->setContentsMargins(12, 8, 12, 8);
    kl->addWidget(m_keyboard);
    keyboardFrame->setVisible(false);
    right->addWidget(keyboardFrame);
    root->addLayout(right, 1);
    setCentralWidget(central);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    // ------------------------------------------------------------------ status bar
    auto plain = [](const QString& s) { auto* l = new QLabel(s); l->setObjectName("statusText"); return l; };
    m_statusLeft = plain("Ready");
    m_statusMid = plain("Double precision");
    m_statusRight = plain("Angles measured from normal");
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_statusLeft);
    statusBar()->addWidget(plain("|"));
    statusBar()->addWidget(m_statusMid);
    statusBar()->addWidget(plain("|"));
    statusBar()->addWidget(m_statusRight);
    m_keyboardButton = new QPushButton("⌨ Keyboard");
    m_keyboardButton->setObjectName("flat");
    m_keyboardButton->setCheckable(true);
    m_keyboardButton->setFocusPolicy(Qt::NoFocus);
    m_themeButton = new QPushButton("☾ Dark");
    m_themeButton->setObjectName("flat");
    m_themeButton->setFocusPolicy(Qt::NoFocus);
    statusBar()->addPermanentWidget(m_keyboardButton);
    statusBar()->addPermanentWidget(m_themeButton);
    statusBar()->addPermanentWidget(plain("Qt 6 · Offline"));
    statusBar()->addPermanentWidget(plain("|"));
    statusBar()->addPermanentWidget(plain("PHY104 / Semester 02"));

    m_messageTimer = new QTimer(this);
    m_messageTimer->setSingleShot(true);
    m_messageTimer->setInterval(4500);
    connect(m_messageTimer, &QTimer::timeout, this, [this] { m_transient.clear(); refreshStatus(); });
    connect(ctx, &AppContext::message, this, &MainWindow::showMessage);
    connect(m_keyboardButton, &QPushButton::toggled, this, [this, keyboardFrame](bool on) {
        keyboardFrame->setVisible(on);
        m_keyboard->setVisible(on);
    });
    connect(m_themeButton, &QPushButton::clicked, this, [this] { emit themeToggled(!ThemeManager::instance().isDark()); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this, applyWindowIcon] {
        refreshIcons();
        applyWindowIcon();
        m_themeButton->setText(ThemeManager::instance().isDark() ? "☾ Dark" : "☀ Light");
    });
    connect(m_stack, &QStackedWidget::currentChanged, this, [this] { refreshStatus(); });
    auto* fullScreen = new QShortcut(QKeySequence(Qt::Key_F11), this);            // F11: toggle full screen
    connect(fullScreen, &QShortcut::activated, this, [this] { isFullScreen() ? showMaximized() : showFullScreen(); });
    installShortcuts();
}

void MainWindow::installShortcuts() {
    const auto add = [this](const QKeySequence& key, auto slot) {
        auto* shortcut = new QShortcut(key, this);
        connect(shortcut, &QShortcut::activated, this, slot);
        m_shortcutKeys << key.toString(QKeySequence::NativeText);
    };
    add(QKeySequence(QKeySequence::Save), [this] {                                           // Ctrl+S: write the .phy session
        if (m_ctx->saveSession()) showMessage("Session saved · " + m_ctx->sessionFile);
    });
    add(QKeySequence(QKeySequence::Print), [this] { openReportPage(); });                     // Ctrl+P: the PDF report screen
    add(QKeySequence(Qt::CTRL | Qt::Key_Return), [this] { triggerPrimaryAction(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Enter), [this] { triggerPrimaryAction(); });    // numeric keypad
    for (int i = 1; i <= 9; ++i)                                              // Ctrl+1..9: the first nine pages
        add(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + i)), [this, i] { setCurrentPage(i - 1); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Tab), [this] {                        // Ctrl+Tab: next page
        setCurrentPage((currentPage() + 1) % pageCount());
    });
}

void MainWindow::triggerPrimaryAction() {
    QWidget* page = m_stack->currentWidget();
    if (!page) return;
    for (QPushButton* b : page->findChildren<QPushButton*>()) {
        if (!b->isVisibleTo(page) || !b->isEnabled()) continue;
        const QString text = b->text();
        if (text.startsWith("Calculate") || text == "Solve" || text == "Evaluate") { b->click(); return; }
    }
}

void MainWindow::openReportPage() {
    for (int i = 0; i < m_stack->count(); ++i) {
        const QString name = QString::fromLatin1(m_stack->widget(i)->metaObject()->className());
        if (name.endsWith("ReportPage")) { setCurrentPage(i); return; }
    }
}

QString MainWindow::statusLeftText() const { return m_statusLeft->text(); }

/// First usable input field of the current page (for the on-screen keyboard when nothing is focused).
QLineEdit* MainWindow::fallbackField() const {
    QWidget* page = m_stack->currentWidget();
    if (!page) return nullptr;
    QLineEdit* any = nullptr;
    for (QLineEdit* e : page->findChildren<QLineEdit*>()) {
        if (!e->isVisibleTo(page) || !e->isEnabled() || e->isReadOnly()) continue;
        if (qobject_cast<QAbstractSpinBox*>(e->parentWidget()) || qobject_cast<QComboBox*>(e->parentWidget())) continue;
        if (e->objectName() == "lcdInput") return e;
        if (!any) any = e;
    }
    return any;
}

void MainWindow::refreshStatus() {
    BasePage::StatusInfo info;
    bool paused = false;
    if (auto* page = qobject_cast<BasePage*>(m_stack->currentWidget())) { info = page->statusInfo(); paused = page->isPaused(); }
    m_statusLeft->setText(m_transient.isEmpty() ? info.left : m_transient);
    m_statusMid->setText(info.mid);
    m_statusRight->setText(info.right);
    for (QLabel* l : {m_statusLeft, m_statusMid, m_statusRight}) {
        l->setProperty("error", info.error);
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
    m_engine->setText(paused ? "● CALCULATION PAUSED" : "● ENGINE READY");
    m_engine->setProperty("paused", paused);
    m_engine->style()->unpolish(m_engine);
    m_engine->style()->polish(m_engine);
}

void MainWindow::showMessage(const QString& text) {
    m_transient = text;
    m_messageTimer->start();
    refreshStatus();
}

int MainWindow::pageCount() const { return m_stack->count(); }
int MainWindow::currentPage() const { return m_stack->currentIndex(); }
QWidget* MainWindow::page(int index) const { return m_stack->widget(index); }
int MainWindow::indexOf(QWidget* page) const { return m_stack->indexOf(page); }

QList<QPushButton*> MainWindow::navButtons() const {
    QList<QPushButton*> out;
    for (const NavEntry& e : m_nav) out << e.button;
    return out;
}

int MainWindow::addPage(const QString& title, const QString& icon, QWidget* page, int section) {
    const int index = m_stack->addWidget(page);
    if (auto* base = qobject_cast<BasePage*>(page)) connect(base, &BasePage::stateChanged, this, [this] { refreshStatus(); });
    addNavButton(title, icon, index, section);
    return index;
}

void MainWindow::addNavAlias(const QString& title, const QString& icon, int pageIndex, int section) {
    addNavButton(title, icon, pageIndex, section);
}

void MainWindow::addNavButton(const QString& title, const QString& icon, int pageIndex, int section) {
    QString label = title;
    const bool wraps = label.contains(" (") && label.size() > 22;
    if (wraps) label.replace(" (", "\n(");
    auto* b = new NavButton(label, icon);
    b->setObjectName(section == 0 ? "navItem" : "navItemSmall");
    if (wraps) b->setMinimumHeight(54);
    m_navGroup->addButton(b);
    (section == 0 ? m_modules : m_tools)->addWidget(b);
    m_nav.push_back({b, icon, pageIndex});
    connect(b, &QPushButton::clicked, this, [this, pageIndex] { m_stack->setCurrentIndex(pageIndex); });
    if (m_nav.size() == 1) b->setChecked(true);
}

void MainWindow::setCurrentPage(int index) {
    if (index < 0 || index >= m_stack->count()) return;
    m_stack->setCurrentIndex(index);
    for (const NavEntry& e : m_nav)
        if (e.page == index) { e.button->setChecked(true); break; }
}

void MainWindow::refreshIcons() {
    for (const NavEntry& e : m_nav) e.button->update();
}

} // namespace phy104
