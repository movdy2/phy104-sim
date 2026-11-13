#include "DashboardPage.h"
#include "AppContext.h"
#include "Components.h"
#include "Icons.h"
#include "OpticsCanvases.h"
#include "ThemeManager.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace phy104 {
namespace {

/// A card that behaves like a button.
class ClickableCard : public QFrame {
    Q_OBJECT
public:
    explicit ClickableCard(QWidget* parent = nullptr) : QFrame(parent) {
        setObjectName("cardHot");
        setCursor(Qt::PointingHandCursor);
    }
signals:
    void clicked();
protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos())) emit clicked();
    }
};

struct CardInfo {
    const char* kicker; const char* title; const char* text;
    PreviewIcon::Kind kind; const char* experiment; int page;
};

const CardInfo kCards[4] = {
    {"01 / GEOMETRICAL OPTICS", "Refraction & TIR", "Snell's law, critical angle and total internal reflection.", PreviewIcon::Refraction, "Refraction & TIR", 1},
    {"02 / IMAGE FORMATION", "Lenses & Mirrors", "Thin lenses, spherical mirrors and principal-ray tracing.", PreviewIcon::Lens, "Lenses & Mirrors", 2},
    {"03 / MINIMUM DEVIATION", "Prisms", "Trace an optical path and determine the refractive index.", PreviewIcon::Prism, "Prisms", 3},
    {"04 / WAVE OPTICS", "Wave Interference (Double-Slit)", "Young's experiment, fringe spacing and intensity profiles.", PreviewIcon::Waves, "Wave Interference", 4}};

QWidget* metricCard(const QString& value, const QString& title, const QString& text, QLabel** valueOut = nullptr) {
    auto* card = new QFrame;
    card->setObjectName("card");
    auto* l = new QHBoxLayout(card);
    l->setContentsMargins(16, 14, 16, 14);
    l->setSpacing(14);
    auto* v = new QLabel(value);
    v->setObjectName("metricValue");
    auto* box = new QVBoxLayout;
    box->setSpacing(2);
    auto* t = new QLabel(title);
    t->setObjectName("metricTitle");
    auto* d = new QLabel(text);
    d->setObjectName("metricText");
    box->addStretch();
    box->addWidget(t);
    box->addWidget(d);
    box->addStretch();
    l->addWidget(v);
    l->addLayout(box, 1);
    if (valueOut) *valueOut = v;
    return card;
}

} // namespace

DashboardPage::DashboardPage(AppContext* ctx, QWidget* parent)
    : BasePage("Dashboard", "Your optical bench / choose an experiment or resume a saved run", ctx, parent) {

    // --- metrics
    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(12);
    metrics->addWidget(metricCard("04", "EXPERIMENT MODULES", "Geometrical & wave optics"));
    metrics->addWidget(metricCard("00", "SAVED CALCULATIONS", "Current lab session", &m_savedValue));
    metrics->addWidget(metricCard("00", "DATA SERIES", "Ready for Excel / CSV", &m_seriesValue));
    content()->addLayout(metrics);

    // --- experiment cards
    auto* grid = new QGridLayout;
    grid->setSpacing(12);
    for (int i = 0; i < 4; ++i) {
        const CardInfo& info = kCards[i];
        auto* card = new ClickableCard;
        auto* l = new QHBoxLayout(card);
        l->setContentsMargins(15, 15, 15, 15);
        l->setSpacing(15);
        l->addWidget(new PreviewIcon(info.kind), 0, Qt::AlignTop);
        auto* text = new QVBoxLayout;
        text->setSpacing(6);
        auto* kicker = new QLabel(info.kicker);
        kicker->setObjectName("cardKicker");
        auto* title = new QLabel(info.title);
        title->setObjectName("cardTitle");
        title->setWordWrap(true);
        auto* desc = new QLabel(info.text);
        desc->setObjectName("cardText");
        desc->setWordWrap(true);
        auto* result = new QLabel("—");
        result->setObjectName("cardResult");
        m_results << result;
        text->addWidget(kicker);
        text->addWidget(title);
        text->addWidget(desc);
        text->addWidget(result);
        text->addStretch();
        auto* arrow = new QLabel;
        arrow->setPixmap(Icons::pixmap("arrow-up-right", ThemeManager::instance().theme().amber, 14));
        l->addLayout(text, 1);
        auto* arrowCol = new QVBoxLayout;
        arrowCol->addStretch();
        arrowCol->addWidget(arrow);
        l->addLayout(arrowCol);
        const int page = info.page;
        connect(card, &ClickableCard::clicked, this, [this, page] { emit openPageRequested(page); });
        grid->addWidget(card, i / 2, i % 2);
    }
    content()->addLayout(grid, 1);

    // --- recent calculations + notebook
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(12);
    auto* recent = new Panel("Recent calculations", "SESSION " + QString::number(ctx->log.sessionNumber()).rightJustified(2, '0'));
    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels({"RUN", "EXPERIMENT", "RESULT", "STATUS"});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setMinimumHeight(150);
    recent->body()->setContentsMargins(12, 0, 12, 8);
    recent->body()->addWidget(m_table);
    bottom->addWidget(recent, 3);

    auto* notebook = new Panel("Lab notebook", "AUTOSAVED");
    auto* blurb = new QLabel("Review formula references, record observations and export your measured data.");
    blurb->setObjectName("cardText");
    blurb->setWordWrap(true);
    auto* openNotes = new QPushButton("Open notes");
    openNotes->setObjectName("secondary");
    auto* exportData = new QPushButton("Export data");
    exportData->setObjectName("primary");
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(openNotes);
    buttons->addWidget(exportData);
    m_lastSaved = new QLabel;
    m_lastSaved->setObjectName("hint");
    notebook->body()->addWidget(blurb);
    notebook->body()->addLayout(buttons);
    notebook->body()->addWidget(m_lastSaved);
    notebook->body()->addStretch();
    bottom->addWidget(notebook, 2);
    content()->addLayout(bottom);

    connect(openNotes, &QPushButton::clicked, this, [this] { emit openPageRequested(m_notesPage); });
    connect(exportData, &QPushButton::clicked, this, [this] { emit openPageRequested(m_notesPage); });
    connect(ctx, &AppContext::snapshotChanged, this, &DashboardPage::refresh);
    connect(ctx, &AppContext::runsChanged, this, &DashboardPage::refresh);
    connect(ctx, &AppContext::notesSaved, this, &DashboardPage::refresh);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &DashboardPage::refresh);
    refresh();
}

void DashboardPage::refresh() {
    const Theme& t = ThemeManager::instance().theme();
    m_savedValue->setText(QString::number(m_ctx->log.savedCalculations()).rightJustified(2, '0'));

    int series = 0;
    for (int i = 0; i < 4; ++i) {
        const Snapshot* snap = m_ctx->log.snapshot(kCards[i].experiment);
        m_results[i]->setText(snap ? QString::fromStdString(snap->headline) : "—");
        if (snap && !snap->series.rows.empty()) ++series;
    }
    m_seriesValue->setText(QString::number(series).rightJustified(2, '0'));

    const auto recent = m_ctx->log.recent(3);
    m_table->setRowCount(recent.empty() ? 1 : static_cast<int>(recent.size()));
    if (recent.empty()) {
        m_table->setItem(0, 0, new QTableWidgetItem("—"));
        m_table->setItem(0, 1, new QTableWidgetItem("No saved calculations yet: press Calculate & Simulate"));
        m_table->item(0, 1)->setForeground(t.dim);
    }
    int row = 0;
    for (const RunRecord& r : recent) {
        auto* run = new QTableWidgetItem(QString::fromStdString(SessionLog::runLabel(r.id)));
        run->setForeground(t.dim);
        auto* exp = new QTableWidgetItem(QString::fromStdString(r.description));
        exp->setForeground(t.text);
        QFont f = exp->font(); f.setFamily(ThemeManager::uiFont()); f.setPixelSize(12); f.setWeight(QFont::Normal); exp->setFont(f);
        auto* res = new QTableWidgetItem(QString::fromStdString(r.result));
        res->setForeground(t.sage);
        auto* st = new QTableWidgetItem(r.valid ? "● Valid" : "● Invalid");
        st->setForeground(r.valid ? t.lime : t.error);
        m_table->setItem(row, 0, run); m_table->setItem(row, 1, exp); m_table->setItem(row, 2, res); m_table->setItem(row, 3, st);
        ++row;
    }
    for (int i = 0; i < m_table->rowCount(); ++i) m_table->setRowHeight(i, 32);
    const QString when = m_ctx->lastSavedTime();
    m_lastSaved->setText((when == "restored" ? QString("Session restored") : "Last saved " + when) + " · " + m_ctx->sessionFile);
}

} // namespace phy104

#include "DashboardPage.moc"
