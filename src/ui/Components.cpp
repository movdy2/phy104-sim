#include "Components.h"
#include "ExpressionEvaluator.h"
#include "Icons.h"
#include "ThemeManager.h"

#include <QButtonGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <QLocale>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <cmath>

namespace phy104 {

void clearLayout(QLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) { w->hide(); w->deleteLater(); }
        else if (QLayout* child = item->layout()) clearLayout(child);   // a layout item IS the layout: delete it once, below
        delete item;
    }
}

QWidget* labeled(const QString& label, QWidget* field) {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(5);
    auto* cap = new QLabel(label);
    cap->setObjectName("fieldLabel");
    l->addWidget(cap);
    l->addWidget(field);
    if (auto* unitField = qobject_cast<UnitField*>(field)) {           // inline error text under the field
        auto* err = new QLabel;
        err->setObjectName("fieldError");
        err->setWordWrap(true);
        err->hide();
        l->addWidget(err);
        QObject::connect(unitField, &UnitField::errorTextChanged, err, [err](const QString& msg) {
            err->setText(msg);
            err->setVisible(!msg.isEmpty());
        });
    }
    return w;
}

// ---------------------------------------------------------------------------- Panel
Panel::Panel(const QString& title, const QString& meta, QWidget* parent) : QFrame(parent) {
    setObjectName("panel");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QFrame;
    header->setObjectName("panelHeader");
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 12, 0);
    m_title = new QLabel(title);
    m_title->setObjectName("panelTitle");
    m_meta = new QLabel(meta.toUpper());
    m_meta->setObjectName("panelMeta");
    hl->addWidget(m_title);
    hl->addStretch();
    hl->addWidget(m_meta);
    root->addWidget(header);

    auto* bodyHolder = new QWidget;
    m_body = new QVBoxLayout(bodyHolder);
    m_body->setContentsMargins(12, 12, 12, 12);
    m_body->setSpacing(8);
    root->addWidget(bodyHolder, 1);
}
void Panel::setMeta(const QString& meta) { m_meta->setText(meta.toUpper()); }
void Panel::setTitle(const QString& title) { m_title->setText(title); }

// ---------------------------------------------------------------------------- UnitField
UnitField::UnitField(const QString& unit, QWidget* parent) : QFrame(parent) {
    setObjectName("field");
    setFixedHeight(32);
    auto* l = new QHBoxLayout(this);
    l->setContentsMargins(10, 0, 4, 0);
    l->setSpacing(6);
    m_edit = new QLineEdit;
    m_edit->setFrame(false);
    m_edit->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_edit->installEventFilter(this);
    installEventFilter(this);
    m_unit = new QLabel(unit);
    m_unit->setObjectName("unit");
    m_unit->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    // clickable arrows ▲▼
    m_arrows = new QWidget;
    auto* al = new QVBoxLayout(m_arrows);
    al->setContentsMargins(0, 2, 0, 2);
    al->setSpacing(0);
    auto makeArrow = [&](const char* name, const char* icon, int dir) {
        auto* b = new QToolButton;
        b->setObjectName(name);
        b->setFixedSize(18, 13);
        b->setAutoRaise(true);
        b->setAutoRepeat(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
        b->setIconSize(QSize(12, 12));
        b->setProperty("iconName", icon);
        connect(b, &QToolButton::clicked, this, [this, dir] { step(dir); });
        al->addWidget(b);
        return b;
    };
    makeArrow("spinUp", "chevron-up", +1);
    makeArrow("spinDown", "chevron-down", -1);
    m_arrows->hide();

    l->addWidget(m_edit, 1);
    l->addWidget(m_unit);
    l->addWidget(m_arrows);

    m_liveTimer = new QTimer(this);                  // live update while typing, after a short pause
    m_liveTimer->setSingleShot(true);
    m_liveTimer->setInterval(350);
    connect(m_liveTimer, &QTimer::timeout, this, &UnitField::edited);
    connect(m_edit, &QLineEdit::textEdited, m_liveTimer, qOverload<>(&QTimer::start));
    connect(m_edit, &QLineEdit::editingFinished, this, [this] {
        if (m_liveTimer->isActive()) { m_liveTimer->stop(); emit edited(); }
    });

    auto refreshIcons = [this] {
        const QColor c = ThemeManager::instance().theme().dim;
        for (auto* b : m_arrows->findChildren<QToolButton*>())
            b->setIcon(Icons::get(b->property("iconName").toString(), c, ThemeManager::instance().theme().amber, 12));
    };
    refreshIcons();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, refreshIcons);
}

double UnitField::value(bool* ok) const {
    QString t = m_edit->text().trimmed();
    t.replace(',', '.');
    bool good = false;
    double v = QLocale::c().toDouble(t, &good);                       // plain number: fast path
    if (!good && !t.isEmpty()) {                                      // expression: 0.5*mm, 1/3, sqrt(2), π ...
        try {
            const AngleUnit unit = m_unit->text() == "°" ? AngleUnit::Degrees : AngleUnit::Radians;
            v = ExpressionEvaluator::evaluate(t.toStdString(), {}, unit);
            good = std::isfinite(v);
        } catch (const std::exception&) {
            good = false;
        }
    }
    if (ok) *ok = good;
    return good ? v : 0.0;
}

void UnitField::setValue(double v, int decimals) {
    m_decimals = decimals;
    QString text = QString::number(v, 'f', decimals);
    if (m_plus && v > 0) text.prepend('+');
    m_edit->setText(text);
}
QString UnitField::text() const { return m_edit->text(); }
void UnitField::setText(const QString& t) { m_edit->setText(t); }
void UnitField::setUnit(const QString& u) { m_unit->setText(u); }
QString UnitField::unit() const { return m_unit->text(); }

void UnitField::setSpinner(bool on, double stepSize) {
    m_spinnerOn = on;
    m_step = stepSize;
    m_arrows->setVisible(on);
}

void UnitField::step(int direction) {
    bool ok = false;
    const double v = value(&ok);
    m_liveTimer->stop();
    setValue((ok ? v : 0.0) + direction * m_step, m_decimals);
    emit edited();
}

void UnitField::setError(bool on, const QString& message) {
    const bool changed = on != m_error;
    m_error = on;
    setProperty("error", on);
    if (changed) repolish();
    emit errorTextChanged(on ? message : QString());
}

void UnitField::repolish() {
    style()->unpolish(this);
    style()->polish(this);
}

bool UnitField::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_edit) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) {
            setProperty("focused", event->type() == QEvent::FocusIn);
            repolish();
        } else if (m_spinnerOn && event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Up || key == Qt::Key_Down) { step(key == Qt::Key_Up ? +1 : -1); return true; }
        }
    }
    if (m_spinnerOn && event->type() == QEvent::Wheel && (watched == m_edit || watched == this) && m_edit->hasFocus()) {
        step(static_cast<QWheelEvent*>(event)->angleDelta().y() > 0 ? +1 : -1);
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------- ModeTabs
ModeTabs::ModeTabs(const QStringList& labels, QWidget* parent) : QWidget(parent), m_group(new QButtonGroup(this)) {
    auto* l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(8);
    int id = 0;
    for (const QString& label : labels) {
        auto* b = new QPushButton(QString(label).replace("&", "&&"));
        b->setObjectName("modeTab");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_group->addButton(b, id++);
        l->addWidget(b, 1);
    }
    m_group->setExclusive(true);
    if (m_group->button(0)) m_group->button(0)->setChecked(true);
    connect(m_group, &QButtonGroup::idClicked, this, &ModeTabs::currentChanged);
}
int ModeTabs::current() const { return m_group->checkedId(); }
void ModeTabs::setCurrent(int index) {
    if (auto* b = m_group->button(index)) b->setChecked(true);
}

// ---------------------------------------------------------------------------- LcdRegisterView
LcdRegisterView::LcdRegisterView(QWidget* parent) : QFrame(parent) {
    setObjectName("lcd");
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(10, 8, 10, 8);
    m_layout->setSpacing(4);
}

void LcdRegisterView::setRegister(const LcdRegister& reg) {
    clearLayout(m_layout);

    auto* top = new QHBoxLayout;
    auto* title = new QLabel(QString::fromStdString(reg.title));
    title->setObjectName("lcdTitle");
    auto* mode = new QLabel(reg.isError ? "● ERR" : "● " + QString::fromStdString(reg.mode));
    mode->setObjectName("lcdTitle");
    top->addWidget(title);
    top->addStretch();
    top->addWidget(mode);
    m_layout->addLayout(top);

    auto addDivider = [this](int width = -1) {
        auto* d = new QFrame;
        d->setObjectName("lcdDivider");
        if (width > 0) d->setMaximumWidth(width);
        m_layout->addWidget(d);
    };
    addDivider();
    m_layout->addSpacing(2);

    for (const std::string& line : reg.lines) {
        auto* l = new QLabel(QString::fromStdString(line).toHtmlEscaped().replace("  ", "&nbsp;&nbsp;").replace(" ", "&nbsp;"));
        l->setObjectName("lcdLine");
        l->setTextFormat(Qt::RichText);
        m_layout->addWidget(l);
    }
    if (!reg.resultValue.empty()) {
        m_layout->addSpacing(2);
        auto* cap = new QLabel(QString::fromStdString(reg.resultLabel));
        cap->setObjectName("lcdCaption");
        auto* big = new QLabel(QString::fromStdString(reg.resultValue));
        big->setObjectName("lcdBig");
        m_layout->addWidget(cap);
        m_layout->addWidget(big);
    }
    m_layout->addSpacing(4);
    addDivider(125);
    auto* foot = new QLabel(QString::fromStdString(reg.footer).replace(" ", "\u00a0"));
    foot->setObjectName("lcdFoot");
    m_layout->addWidget(foot);
}

// ---------------------------------------------------------------------------- NoticeCard
NoticeCard::NoticeCard(QWidget* parent) : QFrame(parent) {
    setObjectName("notice");
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(12, 10, 12, 10);
    l->setSpacing(4);
    m_title = new QLabel;
    m_title->setObjectName("noticeTitle");
    m_title->setWordWrap(true);
    m_body = new QLabel;
    m_body->setObjectName("noticeBody");
    m_body->setWordWrap(true);
    l->addWidget(m_title);
    l->addWidget(m_body);
}

void NoticeCard::setNotice(const QString& title, const QString& body, bool error) {
    m_title->setText(title);
    m_body->setText(body);
    setProperty("error", error);
    style()->unpolish(this);
    style()->polish(this);
}
void NoticeCard::setNotice(const Notice& n) {
    setNotice(QString::fromStdString(n.title), QString::fromStdString(n.body), n.error);
}

// ---------------------------------------------------------------------------- CalculatorKeypad
CalculatorKeypad::CalculatorKeypad(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);

    struct Key { const char* label; const char* insert; const char* kind; };   // insert: text to type, or "" for special keys
    const Key rows[6][6] = {
        {{"sin", "sin(", "keyFn"}, {"cos", "cos(", "keyFn"}, {"tan", "tan(", "keyFn"}, {"ln", "ln(", "keyFn"}, {"log", "log(", "keyFn"}, {"√", "√(", "keyFn"}},
        {{"asin", "asin(", "keyFn"}, {"acos", "acos(", "keyFn"}, {"atan", "atan(", "keyFn"}, {"abs", "abs(", "keyFn"}, {"^", "^", "keyFn"}, {"x²", "^2", "keyFn"}},
        {{"7", "7", "key"}, {"8", "8", "key"}, {"9", "9", "key"}, {"÷", "÷", "keyOp"}, {"DEL", "", "keyOp"}, {"AC", "", "keyOp"}},
        {{"4", "4", "key"}, {"5", "5", "key"}, {"6", "6", "key"}, {"×", "×", "keyOp"}, {"(", "(", "keyOp"}, {")", ")", "keyOp"}},
        {{"1", "1", "key"}, {"2", "2", "key"}, {"3", "3", "key"}, {"−", "-", "keyOp"}, {"Ans", "Ans", "keyFn"}, {"π", "π", "keyFn"}},
        {{"0", "0", "key"}, {".", ".", "key"}, {"e", "e", "keyFn"}, {"+", "+", "keyOp"}, {"=", "", "keyEq"}, {"", "", ""}}};
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            const Key& k = rows[r][c];
            if (!*k.label) continue;
            auto* b = new QPushButton(k.label);
            b->setObjectName(k.kind);
            b->setFocusPolicy(Qt::NoFocus);                       // the LCD input keeps the cursor
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumSize(54, 38);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            const QString label = k.label, text = k.insert;
            connect(b, &QPushButton::clicked, this, [this, label, text] {
                if (label == "DEL") emit backspace();
                else if (label == "AC") emit clearAll();
                else if (label == "=") emit equals();
                else emit insertText(text);
            });
            if (label == "=") grid->addWidget(b, r, c, 1, 2);     // wide equals key
            else grid->addWidget(b, r, c);
        }
    }
    for (int c = 0; c < 6; ++c) grid->setColumnStretch(c, 1);
}

} // namespace phy104
