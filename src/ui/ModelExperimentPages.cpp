#include "ModelExperimentPages.h"
#include "AppContext.h"
#include "Components.h"
#include "OpticsCanvases.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

namespace phy104 {
namespace WE = WaveExperiments;
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
}

// ============================================================================ ModelPage
ModelPage::ModelPage(const QString& title, const QString& subtitle, const QString& experiment, AppContext* ctx, QWidget* parent)
    : ExperimentPage(title, subtitle, experiment, ctx, parent) {
    useSaveExportHeader();
    m_controls = new Panel("Experiment controls");
    m_controls->body()->setSpacing(10);
    m_fieldLayout = m_controls->body();
}

UnitField* ModelPage::addField(const QString& id, const QString& label, const QString& unit, double value, int decimals, bool signedValue, bool spinner, double step) {
    auto* f = new UnitField(unit);
    f->setShowPlus(signedValue);
    if (spinner) f->setSpinner(true, step);
    f->setValue(value, decimals);
    bind(id, f);
    m_fieldLayout->addWidget(labeled(label, f));
    connect(f, &UnitField::edited, this, [this] { apply(false); });
    return f;
}

QComboBox* ModelPage::addCombo(const QString& label, const QStringList& items, const QList<double>& values) {
    auto* c = new QComboBox;
    for (int i = 0; i < items.size(); ++i) c->addItem(items[i], values.value(i, 0.0));
    m_fieldLayout->addWidget(labeled(label, c));
    connect(c, &QComboBox::activated, this, [this] { apply(false); });
    return c;
}

ModeTabs* ModelPage::addTabs(const QStringList& tabs) {
    m_tabs = new ModeTabs(tabs);
    m_fieldLayout->insertWidget(0, m_tabs);
    connect(m_tabs, &ModeTabs::currentChanged, this, [this] { apply(false); });
    return m_tabs;
}

void ModelPage::build(const QString& diagramTitle, QWidget* canvas, const QString& chartTitle, const QString& hint) {
    auto cols = addThreeColumns();
    buildButtons(m_fieldLayout, hint, true);
    cols.left->addWidget(m_controls, 1);

    m_diagram = new Panel(diagramTitle, "");
    m_diagram->body()->setContentsMargins(0, 0, 0, 0);
    m_diagram->body()->addWidget(canvas);
    m_chart = new LineChart;
    m_chartPanel = new Panel(chartTitle, "");
    m_chartPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_chartPanel->body()->addWidget(m_chart);
    cols.center->addWidget(m_diagram, 3);
    cols.center->addWidget(m_chartPanel, 2);

    m_reg1 = new LcdRegisterView;
    m_reg2 = new LcdRegisterView;
    m_interpretation = new NoticeCard;
    m_guard = new NoticeCard;
    cols.right->addWidget(m_reg1);
    cols.right->addWidget(m_reg2);
    cols.right->addWidget(m_interpretation);
    cols.right->addWidget(m_guard);
    cols.right->addStretch();
    setPanels(m_diagram, m_chartPanel, nullptr, "", "");
}

void ModelPage::retitle(const QString& diagramTitle, const QString& chartTitle) {
    m_diagramTitle = diagramTitle;
    if (m_diagram) m_diagram->setTitle(diagramTitle);
    if (m_chartPanel) m_chartPanel->setTitle(chartTitle);
}

void ModelPage::showChart(const ChartData& c) {
    ChartSeries curve, marker;
    curve.line = true;
    for (const auto& [x, y] : c.curve) curve.points << QPointF(x, y);
    marker.markers = true;
    marker.color = QColor();                                            // theme sage
    for (const auto& [x, y] : c.marker) marker.points << QPointF(x, y);
    m_chart->clearAnnotations();
    m_chart->setAxes(c.xMin, c.xMax, c.xTicks, c.xDecimals, q(c.xLabel), c.yMin, c.yMax, c.yTicks, c.yDecimals, q(c.yLabel));
    m_chart->setSeries({curve, marker});
    m_chart->setLegend({}, q(c.legend));
}

void ModelPage::reset() {
    resetFields();
    m_haveLast = false;
    apply(false);
}

void ModelPage::apply(bool record) {
    if (!readFields()) return;
    const ModelReport mr = compute(reportContext());
    const Report& r = mr.report;
    showRegisters(m_reg1, m_reg2, r);
    if (r.paused) {                                                    // the red "paused" card replaces the interpretation
        m_interpretation->hide();
        m_guard->setNotice(r.notices[0]);
    } else {
        m_interpretation->show();
        m_interpretation->setNotice(r.notices[0]);
        m_guard->setNotice(r.notices.size() > 1 ? r.notices[1] : r.notices[0]);
    }
    applyIssues(r);
    if (r.paused) return;

    m_last = r;
    m_haveLast = true;
    captureValid();
    updateCanvas();
    showChart(mr.chart);

    DataSeries series;
    series.id = m_experiment.toLower().replace("'", "").section(' ', 0, 0).toStdString() + "_sweep";
    series.name = m_experiment.toStdString() + " curve";
    series.description = r.headline;
    series.headers = {mr.chart.xLabel, mr.chart.yLabel};
    const std::size_t step = std::max<std::size_t>(1, mr.chart.curve.size() / 20);
    for (std::size_t i = 0; i < mr.chart.curve.size(); i += step) series.rows.push_back({mr.chart.curve[i].first, mr.chart.curve[i].second});
    m_ctx->publish({m_experiment.toStdString(), r.parameters, r.registers, r.headline, series, {}});
    if (record) {
        m_ctx->recordRun(m_experiment, runDescription(), q(r.headline), true);
        refreshMetas(false);
    }
}

// ============================================================================ Newton's rings
NewtonRingsPage::NewtonRingsPage(AppContext* ctx, QWidget* parent)
    : ModelPage("Newton's Rings", "EXPERIMENT 10 / Dark rings in a thin air film, reflected or transmitted", "Newton's Rings", ctx, parent) {
    addTabs({"Reflected", "Dark rings"});
    m_lambda = addField("lambda", "Vacuum wavelength · λ", "nm", 600, 2);
    m_radius = addField("R", "Radius of curvature · R", "m", 1.0, 4);
    m_order = addField("m", "Selected dark-ring order · m", "integer", 4, 0, false, true, 1);
    m_medium = addCombo("Film medium", {"Air · n = 1.0000", "Water · n = 1.3330", "Oil · n = 1.4000"}, {1.0, 1.333, 1.4});
    m_viewing = addCombo("Viewing mode", {"Reflected light", "Transmitted light"}, {0, 1});
    m_canvas = new NewtonCanvas;
    build("Lens on plate / concentric reflected rings", m_canvas, "Dark-ring radius squared vs order", "Reflected: m = 0 is the dark contact center. Transmitted: it is bright and the dark rings sit at half orders. t ≈ r²/(2R).");
    reset();
}
ModelReport NewtonRingsPage::compute(const ReportContext& ctx) {
    return WE::newtonRings({m_lambda->value(), m_radius->value(), m_order->value(), m_medium->currentData().toDouble(),
                            m_viewing->currentData().toDouble() > 0.5}, ctx);
}
void NewtonRingsPage::updateCanvas() {
    const bool transmitted = m_viewing->currentData().toDouble() > 0.5;
    m_canvas->setInput({m_lambda->value(), m_radius->value(), m_order->value(), m_medium->currentData().toDouble(), transmitted});
    retitle(transmitted ? "Lens on plate / transmitted rings" : "Lens on plate / reflected rings",
            transmitted ? "Transmitted dark-ring radius squared vs half order" : "Dark-ring radius squared vs order");
}
void NewtonRingsPage::resetFields() { m_lambda->setValue(600, 2); m_radius->setValue(1.0, 4); m_order->setValue(4, 0); m_medium->setCurrentIndex(0); m_viewing->setCurrentIndex(0); m_tabs->setCurrent(0); }
QString NewtonRingsPage::runDescription() const {
    return QString("m = %1 · λ %2 nm · R %3 m · %4").arg(m_order->text(), m_lambda->text(), m_radius->text(),
                                                         m_viewing->currentData().toDouble() > 0.5 ? "transmitted" : "reflected");
}

// ============================================================================ Michelson
MichelsonPage::MichelsonPage(AppContext* ctx, QWidget* parent)
    : ModelPage("Michelson Interferometry", "EXPERIMENT 09 / Mirror displacement, fringe shift and fringe count", "Michelson Interferometry", ctx, parent) {
    addTabs({"Displacement", "Fringes"});
    m_lambda = addField("lambda", "Wavelength · λ", "nm", 632.8, 2);
    m_dx = addField("dx", "Mirror displacement · Δx", "µm", 0.75, 4, true, true, 0.05);
    m_medium = addCombo("Arm medium", {"Air · n = 1.0000", "Water cell · n = 1.3330"}, {1.0, 1.333});
    m_reference = addCombo("Reference arm", {"M1 fixed", "M1 on a piezo (static)"}, {0, 1});
    m_balance = addCombo("Beam balance", {"Equal beams · V = 1.00", "Unequal beams · V = 0.60"}, {1.0, 0.6});
    m_canvas = new MichelsonCanvas;
    build("Optical bench / detector fringes", m_canvas, "Detector intensity vs mirror displacement", "ΔOPD = 2nΔx. One fringe per λ/(2n) of mirror motion.");
    reset();
}
ModelReport MichelsonPage::compute(const ReportContext& ctx) {
    return WE::michelson({m_lambda->value(), m_dx->value(), m_medium->currentData().toDouble(), m_balance->currentData().toDouble(),
                          m_tabs->current() == 1, m_reference->currentData().toDouble() > 0.5}, ctx);
}
void MichelsonPage::updateCanvas() {
    const bool fringes = m_tabs->current() == 1;
    const bool piezo = m_reference->currentData().toDouble() > 0.5;
    m_canvas->setInput({m_lambda->value(), m_dx->value(), m_medium->currentData().toDouble(), m_balance->currentData().toDouble(), fringes, piezo});
    retitle(fringes ? (piezo ? "Optical bench / piezo scan of M1" : "Optical bench / fringe pattern") : "Optical bench / detector fringes",
            fringes ? "Intensity across a ±2 µm mirror sweep" : "Detector intensity vs mirror displacement");
}
void MichelsonPage::resetFields() { m_lambda->setValue(632.8, 2); m_dx->setValue(0.75, 4); m_medium->setCurrentIndex(0); m_reference->setCurrentIndex(0); m_balance->setCurrentIndex(0); m_tabs->setCurrent(0); }
QString MichelsonPage::runDescription() const {
    return QString("Δx %1 µm · λ %2 nm · %3").arg(m_dx->text(), m_lambda->text(), m_tabs->current() == 1 ? "fringes" : "displacement");
}

// ============================================================================ thin film
ThinFilmPage::ThinFilmPage(AppContext* ctx, QWidget* parent)
    : ModelPage("Thin-Film Interference", "EXPERIMENT 08 / Single film at normal incidence: reflection and transmission", "Thin-Film Interference", ctx, parent) {
    addTabs({"Reflection", "Transmission"});
    m_lambda = addField("lambda", "Vacuum wavelength · λ", "nm", 600, 2);
    m_n = addField("n", "Film index · n", "", 1.5, 4, false, true, 0.01);
    m_t = addField("t", "Film thickness · t", "nm", 100, 2, false, true, 5);
    m_canvas = new ThinFilmCanvas;
    build("Film layers / two reflected rays", m_canvas, "Reflected intensity vs thickness", "Reflection: one phase reversal, bright at 2nt = (q + ½)λ. Transmission: no net reversal, bright at 2nt = qλ.");
    reset();
}
ModelReport ThinFilmPage::compute(const ReportContext& ctx) {
    return WE::thinFilm({m_lambda->value(), m_n->value(), m_t->value(), m_tabs->current() == 1}, ctx);
}
void ThinFilmPage::updateCanvas() {
    const bool transmission = m_tabs->current() == 1;
    m_canvas->setInput({m_lambda->value(), m_n->value(), m_t->value(), transmission});
    retitle(transmission ? "Film layers / transmitted beam" : "Film layers / two reflected rays",
            transmission ? "Transmitted intensity vs thickness" : "Reflected intensity vs thickness");
}
void ThinFilmPage::resetFields() { m_lambda->setValue(600, 2); m_n->setValue(1.5, 4); m_t->setValue(100, 2); m_tabs->setCurrent(0); }
QString ThinFilmPage::runDescription() const {
    return QString("t %1 nm · n %2 · %3").arg(m_t->text(), m_n->text(), m_tabs->current() == 1 ? "transmission" : "reflection");
}

// ============================================================================ diffraction grating
GratingPage::GratingPage(AppContext* ctx, QWidget* parent)
    : ModelPage("Diffraction Grating", "EXPERIMENT 07 / Principal maxima of a transmission grating", "Diffraction Grating", ctx, parent) {
    addTabs({"Transmission", "Orders"});
    m_density = addField("N", "Line density · N", "lines/cm", 5000, 0, false, true, 100);
    m_lambda = addField("lambda", "Wavelength · λ", "nm", 600, 2);
    m_order = addField("m", "Selected order · m", "order", 1, 0, true, true, 1);
    m_slits = addField("M", "Illuminated slit count · M", "slits", 20, 0, false, true, 1);
    m_incidence = addCombo("Incidence", {"Normal · 0.00°", "15.00°", "30.00°"}, {0, 15, 30});
    m_canvas = new GratingCanvas;
    build("Grating / angular observation screen", m_canvas, "Diffracted intensity vs angle", "N is line density, not slit count. Transmission: intensity fan. Orders: every peak with its angle. M = 20 controls sharpness.");
    reset();
}
ModelReport GratingPage::compute(const ReportContext& ctx) {
    return WE::grating({m_density->value(), m_lambda->value(), m_order->value(), m_slits->value(), m_incidence->currentData().toDouble(),
                        m_tabs->current() == 1}, ctx);
}
void GratingPage::updateCanvas() {
    const bool orders = m_tabs->current() == 1;
    m_canvas->setInput({m_density->value(), m_lambda->value(), m_order->value(), m_slits->value(), m_incidence->currentData().toDouble(), orders});
    retitle(orders ? "Grating / every propagating order" : "Grating / angular observation screen",
            orders ? "Order angles and where every peak sits" : "Diffracted intensity vs angle");
}
void GratingPage::resetFields() { m_density->setValue(5000, 0); m_lambda->setValue(600, 2); m_order->setValue(1, 0); m_slits->setValue(20, 0); m_incidence->setCurrentIndex(0); m_tabs->setCurrent(0); }
QString GratingPage::runDescription() const {
    return QString("order %1 · N %2 /cm · %3").arg(m_order->text(), m_density->text(), m_tabs->current() == 1 ? "orders" : "transmission");
}

// ============================================================================ single slit
SingleSlitPage::SingleSlitPage(AppContext* ctx, QWidget* parent)
    : ModelPage("Single-Slit Diffraction", "EXPERIMENT 06 / First minimum and central maximum width", "Single-Slit Diffraction", ctx, parent) {
    addTabs({"Intensity", "Geometry"});
    m_width = addField("a", "Slit width · a", "mm", 0.1, 4, false, true, 0.01);
    m_lambda = addField("lambda", "Wavelength · λ", "nm", 500, 2);
    m_screen = addField("L", "Screen distance · L", "m", 1.0, 4);
    m_wave = addCombo("Incident wave", {"Plane · coherent"}, {0});
    m_scale = addCombo("Intensity scale", {"Normalized · I/I₀"}, {0});
    m_canvas = new SingleSlitCanvas;
    build("Slit / screen diffraction pattern", m_canvas, "Single-slit intensity vs angle", "Minima: a sin θ = mλ. Intensity: the envelope. Geometry: where each minimum lands, y ≈ m λL/a. Small-angle widths: w ≈ 2λL/a.");
    reset();
}
ModelReport SingleSlitPage::compute(const ReportContext& ctx) {
    return WE::singleSlit({m_width->value(), m_lambda->value(), m_screen->value(), m_tabs->current() == 1}, ctx);
}
void SingleSlitPage::updateCanvas() {
    const bool geometry = m_tabs->current() == 1;
    m_canvas->setInput({m_width->value(), m_lambda->value(), m_screen->value(), geometry});
    retitle(geometry ? "Slit / where the minima land" : "Slit / screen diffraction pattern",
            geometry ? "Minimum position vs order" : "Single-slit intensity vs angle");
}
void SingleSlitPage::resetFields() { m_width->setValue(0.1, 4); m_lambda->setValue(500, 2); m_screen->setValue(1.0, 4); m_wave->setCurrentIndex(0); m_scale->setCurrentIndex(0); m_tabs->setCurrent(0); }
QString SingleSlitPage::runDescription() const {
    return QString("a %1 mm · λ %2 nm · %3").arg(m_width->text(), m_lambda->text(), m_tabs->current() == 1 ? "geometry" : "intensity");
}

// ============================================================================ Malus + Brewster
PolarizationPage::PolarizationPage(AppContext* ctx, QWidget* parent)
    : ModelPage("Malus's law & Brewster's angle", "EXPERIMENT 05 / Polarizers and the Brewster angle", "Polarization (Malus & Brewster)", ctx, parent) {
    addTabs({"Malus", "Brewster"});
    m_beam = addCombo("Incident beam", {"Unpolarized", "Polarized"}, {1, 0});
    m_intensity = addField("I", "Before polarizer · Iᵢₙ", "W/m²", 100, 2);
    m_angle = addField("theta", "Relative axis angle · θ", "°", 60, 2, false, true, 1);
    m_n1 = addField("n1", "Incident index · n₁ (air)", "", 1.0, 4, false, true, 0.01);
    m_n2 = addField("n2", "Transmitted index · n₂", "", 1.5, 4, false, true, 0.01);
    m_canvas = new PolarizationCanvas;
    build("Two polarizers / independent Brewster reference", m_canvas, "Intensity vs relative angle", "Malus: θ range 0–90°, the first polarizer transmits half of unpolarized light. Brewster: tan θB = n₂/n₁, R_p = 0 at θB.");
    reset();
}
ModelReport PolarizationPage::compute(const ReportContext& ctx) {
    return WE::polarization({m_intensity->value(), m_angle->value(), m_n1->value(), m_n2->value(), m_beam->currentData().toDouble() > 0.5,
                             m_tabs->current() == 1}, ctx);
}
void PolarizationPage::updateCanvas() {
    const bool brewster = m_tabs->current() == 1;
    m_canvas->setInput({m_intensity->value(), m_angle->value(), m_n1->value(), m_n2->value(), m_beam->currentData().toDouble() > 0.5, brewster});
    retitle(brewster ? "Interface at Brewster / reflected beam s-polarized" : "Two polarizers / independent Brewster reference",
            brewster ? "p-polarized reflectance vs incidence" : "Intensity vs relative angle");
}
void PolarizationPage::resetFields() { m_beam->setCurrentIndex(0); m_intensity->setValue(100, 2); m_angle->setValue(60, 2); m_n1->setValue(1.0, 4); m_n2->setValue(1.5, 4); m_tabs->setCurrent(0); }
QString PolarizationPage::runDescription() const {
    return QString("θ %1° · Iᵢₙ %2 W/m² · %3").arg(m_angle->text(), m_intensity->text(), m_tabs->current() == 1 ? "brewster" : "malus");
}

} // namespace phy104
