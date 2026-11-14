#include "DashboardPage.h"
#include "AppContext.h"
#include "Components.h"
#include "Icons.h"
#include "OpticsCanvases.h"
#include "Plots.h"
#include "ThemeManager.h"

#include <QAction>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QMenu>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

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

struct ExperimentLink { const char* title; const char* description; const char* icon; int page; };
const ExperimentLink kAllExperiments[11] = {
    {"Refraction & TIR", "Snell's law, critical angle and total internal reflection.", "split", 1},
    {"Lenses & Mirrors", "Thin lenses, mirrors and principal-ray imaging.", "focus", 2},
    {"Prisms", "Minimum deviation, index and dispersion.", "triangle", 3},
    {"Wave Interference (Double-Slit)", "Young's double slit, fringe spacing and intensity.", "waves", 4},
    {"Polarization (Malus & Brewster)", "Malus' law, Fresnel reflection and Brewster angle.", "contrast", 5},
    {"Single-Slit Diffraction", "First minimum, screen geometry and two-slit envelope.", "slit", 6},
    {"Diffraction Grating", "Orders, resolving power and free spectral range.", "align-justify", 7},
    {"Thin-Film Interference", "Reflection, transmission and visible film colours.", "layers", 8},
    {"Michelson Interferometry", "Fringes, mirror motion and wavelength.", "git-merge", 9},
    {"Newton's Rings", "Ring radii, contact and wavelength.", "circle-dot", 10},
    {"Thick Lens & Aberration", "Principal planes, focal lengths and spherical aberration.", "aperture", 11}};

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
    QLabel* experimentCount = nullptr;
    metrics->addWidget(metricCard("11", "EXPERIMENTS", "Geometrical & wave optics", &experimentCount));
    experimentCount->setObjectName("experimentCount");
    metrics->addWidget(metricCard("00", "SAVED CALCULATIONS", "Current lab session", &m_savedValue));
    metrics->addWidget(metricCard("00", "DATA SERIES", "Ready for Excel / CSV", &m_seriesValue));
    content()->addLayout(metrics);

    auto* browseRow = new QHBoxLayout;
    browseRow->addStretch();
    m_browseExperiments = new QToolButton;
    m_browseExperiments->setObjectName("browseExperiments");
    m_browseExperiments->setText("Browse all 11");
    m_browseExperiments->setPopupMode(QToolButton::InstantPopup);
    m_browseExperiments->setToolTip("Open any of the 11 experiment screens");
    auto* experimentMenu = new QMenu(m_browseExperiments);
    experimentMenu->setObjectName("allExperimentsMenu");
    for (int i = 0; i < 11; ++i) {
        const ExperimentLink& experiment = kAllExperiments[i];
        const Theme& theme = ThemeManager::instance().theme();
        QAction* action = experimentMenu->addAction(
            Icons::get(experiment.icon, theme.text, theme.amber, 18),
            QString("%1 · %2").arg(i + 1, 2, 10, QChar('0')).arg(QString::fromUtf8(experiment.title)));
        action->setToolTip(QString::fromUtf8(experiment.description));
        const int page = experiment.page;
        connect(action, &QAction::triggered, this, [this, page] { emit openPageRequested(page); });
    }
    m_browseExperiments->setMenu(experimentMenu);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [experimentMenu] {
        const Theme& theme = ThemeManager::instance().theme();
        const auto actions = experimentMenu->actions();
        for (int i = 0; i < 11; ++i) {
            const ExperimentLink& experiment = kAllExperiments[i];
            actions[i]->setIcon(Icons::get(experiment.icon, theme.text, theme.amber, 18));
        }
    });
    browseRow->addWidget(m_browseExperiments);
    content()->addLayout(browseRow);

    // --- four featured experiment cards
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
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setMinimumHeight(150);

    // the same panel switches to "Compare": two saved runs overlaid on one chart
    m_recentMode = new ModeTabs({"Recent", "Compare"});
    m_recentStack = new QStackedWidget;
    m_recentStack->addWidget(m_table);
    auto* compareBox = new QWidget;
    auto* cmp = new QVBoxLayout(compareBox);
    cmp->setContentsMargins(0, 0, 0, 0);
    cmp->setSpacing(6);
    auto* pickers = new QHBoxLayout;
    pickers->setSpacing(8);
    m_cmpA = new QComboBox;
    m_cmpB = new QComboBox;
    pickers->addWidget(labeled("Run A", m_cmpA), 1);
    pickers->addWidget(labeled("Run B", m_cmpB), 1);
    cmp->addLayout(pickers);
    m_cmpChart = new LineChart;
    m_cmpChart->setMinimumHeight(130);
    cmp->addWidget(m_cmpChart, 1);
    m_cmpSummary = new QLabel;
    m_cmpSummary->setObjectName("hint");
    m_cmpSummary->setWordWrap(true);
    cmp->addWidget(m_cmpSummary);
    m_recentStack->addWidget(compareBox);

    m_deleteRun = new QPushButton("Delete selected run");
    m_deleteRun->setObjectName("secondary");
    m_deleteRun->setEnabled(false);
    recent->body()->setContentsMargins(12, 0, 12, 8);
    recent->body()->addWidget(m_recentMode);
    recent->body()->addWidget(m_recentStack, 1);
    recent->body()->addWidget(m_deleteRun);
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
    connect(exportData, &QPushButton::clicked, this, [this] { emit openPageRequested(m_exportPage); });
    connect(m_recentMode, &ModeTabs::currentChanged, this, [this](int index) {
        m_recentStack->setCurrentIndex(index);
        if (index == 1) refreshCompare();
    });
    connect(m_cmpA, &QComboBox::activated, this, [this] { refreshCompare(); });
    connect(m_cmpB, &QComboBox::activated, this, [this] { refreshCompare(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        const int row = m_table->currentRow();
        m_deleteRun->setEnabled(row >= 0 && m_table->item(row, 0) && m_table->item(row, 0)->data(Qt::UserRole).isValid());
    });
    connect(m_deleteRun, &QPushButton::clicked, this, &DashboardPage::deleteSelectedRun);
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
        run->setData(Qt::UserRole, r.id);
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
    const int selected = m_table->currentRow();
    m_deleteRun->setEnabled(selected >= 0 && m_table->item(selected, 0) && m_table->item(selected, 0)->data(Qt::UserRole).isValid());
    const QString when = m_ctx->lastSavedTime();
    m_lastSaved->setText((when == "restored" ? QString("Session restored") : "Last saved " + when) + " · " + m_ctx->sessionFile);
    refreshCompare();
}

const RunRecord* DashboardPage::runById(int id) const {
    for (const RunRecord& r : m_ctx->log.runs())
        if (r.id == id) return &r;
    return nullptr;
}

void DashboardPage::refreshCompare() {
    const int keepA = m_cmpA->currentData().toInt();
    const int keepB = m_cmpB->currentData().toInt();
    QList<const RunRecord*> usable;
    for (const RunRecord& r : m_ctx->log.runs())
        if (!r.series.rows.empty() && !r.series.headers.empty()) usable << &r;

    {
        const QSignalBlocker blockA(m_cmpA), blockB(m_cmpB);
        m_cmpA->clear();
        m_cmpB->clear();
        for (const RunRecord* r : usable) {
            const QString label = QString::fromStdString(SessionLog::runLabel(r->id)) + " · " + QString::fromStdString(r->experiment);
            m_cmpA->addItem(label, r->id);
            m_cmpB->addItem(label, r->id);
        }
        const auto restore = [](QComboBox* box, int id, int fromEnd) {
            if (box->count() == 0) return;
            int index = -1;
            for (int i = 0; i < box->count(); ++i)
                if (box->itemData(i).toInt() == id) index = i;
            if (index < 0) index = std::max(0, box->count() - fromEnd);
            box->setCurrentIndex(index);
        };
        restore(m_cmpA, keepA, 2);                       // newest pair by default
        restore(m_cmpB, keepB, 1);
    }

    const RunRecord* a = runById(m_cmpA->currentData().toInt());
    const RunRecord* b = runById(m_cmpB->currentData().toInt());
    const Theme& t = ThemeManager::instance().theme();
    if (!a || !b) {
        m_cmpChart->setSeries({});
        m_cmpChart->setLegend({}, QString());
        m_cmpSummary->setText("Nothing to compare yet: press Calculate & Simulate on two settings and they appear here.");
        return;
    }

    const auto toCurve = [](const DataSeries& series, const QColor& color) {
        ChartSeries curve;
        curve.line = true;
        curve.color = color;
        for (const auto& row : series.rows)
            if (row.size() >= 2 && std::isfinite(row[0]) && std::isfinite(row[1])) curve.points << QPointF(row[0], row[1]);
        return curve;
    };
    ChartSeries curveA = toCurve(a->series, t.amber), curveB = toCurve(b->series, t.blue);
    double xMin = std::numeric_limits<double>::max(), xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max(), yMax = std::numeric_limits<double>::lowest();
    for (const ChartSeries* curve : {&curveA, &curveB}) {
        for (const QPointF& p : curve->points) {
            xMin = std::min(xMin, p.x());
            xMax = std::max(xMax, p.x());
            yMin = std::min(yMin, p.y());
            yMax = std::max(yMax, p.y());
        }
    }
    if (curveA.points.isEmpty() || curveB.points.isEmpty() || yMin > yMax) {
        m_cmpChart->setSeries({});
        m_cmpSummary->setText("These runs carry no sweep data (they were saved by an older version of the session file).");
        return;
    }
    if (xMax - xMin < 1e-12) xMax = xMin + 1.0;
    const double pad = yMax > yMin ? (yMax - yMin) * 0.08 : 0.5;
    const std::string& xName = a->series.headers.size() > 0 ? a->series.headers[0] : a->series.id;
    const std::string yName = a->series.headers.size() > 1 ? a->series.headers[1] : std::string("value");
    m_cmpChart->setAxes(xMin, xMax, 4, 2, QString::fromStdString(xName), yMin - pad, yMax + pad, 3, 3, QString::fromStdString(yName));
    m_cmpChart->setSeries({curveA, curveB});
    m_cmpChart->setLegend({"A · " + QString::fromStdString(a->experiment), "B · " + QString::fromStdString(b->experiment)},
                          a->id == b->id ? "same run on both sides" : "two saved sweeps");
    m_cmpSummary->setText(QString("RUN %1 (%2) · %3 points   vs   RUN %4 (%5) · %6 points%7")
                              .arg(QString::fromStdString(SessionLog::runLabel(a->id)), QString::fromStdString(a->experiment))
                              .arg(static_cast<int>(curveA.points.size()))
                              .arg(QString::fromStdString(SessionLog::runLabel(b->id)), QString::fromStdString(b->experiment))
                              .arg(static_cast<int>(curveB.points.size()))
                              .arg(a->id == b->id ? QString()
                                                  : (a->experiment == b->experiment ? "" : " · different experiments share one axis")));
}

void DashboardPage::deleteSelectedRun() {
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QTableWidgetItem* item = m_table->item(row, 0);
    if (!item) return;
    const int id = item->data(Qt::UserRole).toInt();
    if (id <= 0) return;
    if (m_ctx->deleteRun(id)) refresh();
}

} // namespace phy104

#include "DashboardPage.moc"
