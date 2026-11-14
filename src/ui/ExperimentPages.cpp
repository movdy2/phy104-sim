#include "ExperimentPages.h"
#include "FieldIds.h"
#include "AppContext.h"
#include "Components.h"
#include "NumberFormat.h"
#include "OpticsCanvases.h"
#include "PhysicsCalculator.h"
#include "ThemeManager.h"
#include "Uncertainty.h"

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace phy104 {
namespace ER = ExperimentReports;
namespace PC = PhysicsCalculator;
namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }
QString signedText(double v, int dp = 2) { return (v > 0 ? "+" : "") + QString::number(v, 'f', dp); }

/// "Glass · crown" -> "Glass"
QString shortName(const QString& name) { return name.section(" · ", 0, 0); }

QComboBox* mediaCombo(const OpticsPresets& presets, bool includeAir) {
    auto* c = new QComboBox;
    for (const auto& m : presets.media) {
        if (!includeAir && m.id == "air") continue;
        c->addItem(q(m.name), q(m.id));
    }
    c->addItem("Custom", QString());
    return c;
}

/// Axis range with "nice" ticks: [lo, hi] extended so that `intervals` equal steps hit round numbers.
void niceRange(double lo, double hi, int intervals, double& outLo, double& outHi) {
    if (hi - lo < 1e-9) { lo -= 1; hi += 1; }
    const double raw = (hi - lo) / intervals;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    double step = mag;
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0}) { step = m * mag; if (step >= raw) break; }
    outLo = std::floor(lo / step) * step;
    outHi = outLo + step * intervals;
    while (outHi < hi - 1e-9) { outHi += step; outLo = outHi - step * intervals; }
}

QWidget* uncertaintyInputs(const std::vector<std::pair<QString, UnitField*>>& inputs) {
    auto* root = new QWidget;
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto* toggle = new QToolButton;
    toggle->setObjectName("uncertaintyToggle");
    toggle->setText("Measurement uncertainties · 1σ");
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setArrowType(Qt::RightArrow);
    toggle->setCheckable(true);
    auto* editor = new QWidget;
    editor->setObjectName("uncertaintyInputs");
    auto* fields = new QVBoxLayout(editor);
    fields->setContentsMargins(12, 2, 0, 0);
    fields->setSpacing(6);
    auto* hint = new QLabel("Independent standard uncertainties; enter 0 when a value is treated as exact.");
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    fields->addWidget(hint);
    for (const auto& [label, field] : inputs) {
        field->setToolTip("Standard uncertainty (1σ). Enter 0 to treat this quantity as exact.");
        fields->addWidget(labeled(label, field));
    }
    editor->hide();
    layout->addWidget(toggle);
    layout->addWidget(editor);
    QObject::connect(toggle, &QToolButton::toggled, editor, [toggle, editor](bool open) {
        toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        editor->setVisible(open);
    });
    return root;
}

} // namespace

// ============================================================================ ExperimentPage (shared)
ExperimentPage::ExperimentPage(const QString& title, const QString& subtitle, const QString& experiment, AppContext* ctx, QWidget* parent)
    : BasePage(title, subtitle, ctx, parent), m_experiment(experiment) {}

void ExperimentPage::bind(const QString& id, UnitField* field) { m_bindings.push_back({id, field, 0, false}); }

ExperimentPage::Binding* ExperimentPage::find(const QString& id) {
    for (Binding& b : m_bindings) if (b.id == id) return &b;
    return nullptr;
}

bool ExperimentPage::readFields() {
    bool allGood = true;
    for (Binding& b : m_bindings) {
        bool ok = false;
        b.field->value(&ok);
        b.field->setError(!ok, "Enter a number or expression.");
        allGood = allGood && ok;
    }
    if (!allGood) {                                                  // nothing can be calculated from unreadable text
        m_calc->setEnabled(false);
        emit m_ctx->message("Enter numbers only (for example 1.5 or 0.5*mm)");
    }
    return allGood;
}

void ExperimentPage::captureValid() {
    for (Binding& b : m_bindings) { bool ok = false; const double v = b.field->value(&ok); if (ok) { b.lastValid = v; b.hasValid = true; } }
}

QString ExperimentPage::lastRunLabel() const {
    for (auto it = m_ctx->log.runs().rbegin(); it != m_ctx->log.runs().rend(); ++it)
        if (it->experiment == m_experiment.toStdString()) return q(SessionLog::runLabel(it->id)).replace("#", "RUN ");
    return {};
}

ReportContext ExperimentPage::reportContext() const {
    return {m_haveLast ? &m_last : nullptr, lastRunLabel().toStdString()};
}

void ExperimentPage::setPanels(Panel* d, Panel* c, Panel* r, const QString& dm, const QString& cm) {
    m_diagramPanel = d; m_chartPanel = c; m_resultsPanel = r; m_diagramMeta = dm; m_chartMeta = cm;
}

void ExperimentPage::refreshMetas(bool paused) {
    const QString run = lastRunLabel();
    const QString number = run.section(' ', 1);
    if (m_diagramPanel) m_diagramPanel->setMeta(paused ? "LAST VALID" + (number.isEmpty() ? QString() : " · " + number) : m_diagramMeta);
    if (m_chartPanel) m_chartPanel->setMeta(paused ? "STALE" + (run.isEmpty() ? QString() : " · " + run) : m_chartMeta);
    if (m_resultsPanel) m_resultsPanel->setMeta(paused ? "PAUSED" : m_ctx->nextRunLabel());
}

void ExperimentPage::setNormalHint(const QString& text) {
    m_hintText = text;
    if (!m_restore->isVisible()) m_hint->setText(text);
}

QWidget* ExperimentPage::diagramCanvas() const {
    if (!m_diagramPanel) return nullptr;
    QVBoxLayout* body = m_diagramPanel->body();
    for (int i = 0; i < body->count(); ++i)
        if (QWidget* w = body->itemAt(i)->widget()) return w;      // the canvas inside the panel
    return nullptr;
}

QWidget* ExperimentPage::chartWidget() const {
    if (!m_chartPanel) return nullptr;
    QVBoxLayout* body = m_chartPanel->body();
    for (int i = 0; i < body->count(); ++i)
        if (QWidget* w = body->itemAt(i)->widget()) return w;      // the LineChart inside the panel
    return nullptr;
}

void ExperimentPage::buildButtons(QVBoxLayout* fields, const QString& hint, bool withSend) {
    m_calc = new QPushButton("Calculate && Simulate");
    m_calc->setObjectName("primary");
    m_reset = new QPushButton("Reset parameters");
    m_reset->setObjectName("secondary");
    m_restore = new QPushButton;
    m_restore->setObjectName("primary");
    m_restore->hide();
    for (auto* b : {m_calc, m_reset, m_restore}) b->setCursor(Qt::PointingHandCursor);
    m_hint = new QLabel(hint);
    m_hint->setObjectName("hint");
    m_hint->setWordWrap(true);
    m_hintText = hint;
    fields->addSpacing(2);
    if (withSend) {                                                  // design: Calculate & Simulate | Send to Notes side by side
        auto* send = new QPushButton("Send to\nNotes");
        send->setObjectName("outlineAmber");
        send->setCursor(Qt::PointingHandCursor);
        send->setMinimumHeight(44);
        m_calc->setText("Calculate &&\nSimulate");
        m_calc->setMinimumHeight(44);
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        row->addWidget(m_calc);
        row->addWidget(send);
        fields->addLayout(row);
        connect(send, &QPushButton::clicked, this, [this] { sendToNotes(); });
    } else {
        fields->addWidget(m_calc);
    }
    fields->addWidget(m_reset);
    fields->addWidget(m_restore);
    fields->addWidget(m_hint);
    fields->addStretch();
    connect(m_calc, &QPushButton::clicked, this, [this] { apply(true); });
    connect(m_reset, &QPushButton::clicked, this, [this] { reset(); });
    connect(m_restore, &QPushButton::clicked, this, [this] {
        for (Binding& b : m_bindings)
            if (b.field->hasError() && b.hasValid) b.field->setValue(b.lastValid, b.field->decimals());
        apply(false);
    });
}

void ExperimentPage::applyIssues(const Report& r) {
    for (Binding& b : m_bindings) b.field->setError(false);
    const bool paused = r.paused;
    Binding* firstWithValue = nullptr;
    QString restoreLabel;
    for (const FieldIssue& issue : r.issues)
        if (Binding* b = find(q(issue.field))) {
            b->field->setError(true, q(issue.message));
            if (!firstWithValue && b->hasValid) { firstWithValue = b; restoreLabel = q(issue.label); }
        }
    m_calc->setEnabled(!paused);
    m_reset->setVisible(!paused);
    m_restore->setVisible(paused && firstWithValue);
    if (paused && firstWithValue) {
        QString unit = firstWithValue->field->unit();
        if (unit == "integer" || unit == "order" || unit == "slits") unit.clear();            // labels, not units
        const double v = firstWithValue->lastValid;
        m_restore->setText("Restore " + restoreLabel + " = " + (firstWithValue->field->showsPlus() && v > 0 ? "+" : "") +
                           QString::number(v, 'f', firstWithValue->field->decimals()) + (unit.isEmpty() ? "" : " " + unit));
    }
    m_hint->setText(paused ? (firstWithValue ? "Restore the last-valid value, then calculate again." : "Edit the highlighted field to continue.") : m_hintText);

    StatusInfo info;
    if (paused) {
        const QString text = q(r.statusText);
        QString tail = text.section(" · ", 1);
        if (!tail.isEmpty()) tail[0] = tail[0].toUpper();
        info = {"Paused", text.section(" · ", 0, 0), tail, true};
    } else if (!r.statusText.empty()) {
        info.mid = q(r.statusText);
    }
    setStatusInfo(info, paused);
    refreshMetas(paused);
}

void ExperimentPage::sendToNotes() {
    if (!m_haveLast) { emit m_ctx->message("Nothing to send yet: calculate first"); return; }
    QString md = "### " + m_experiment + " · " + q(m_last.headline) + "\n";
    for (const auto& [name, value] : m_last.parameters) md += "- " + q(name) + " = " + q(value) + "\n";
    for (const LcdRegister& r : m_last.registers) {
        md += "- **" + q(r.title) + "**: ";
        QStringList parts;
        for (const auto& l : r.lines) parts << q(l).trimmed();
        md += parts.join("; ");
        if (!r.resultValue.empty()) md += "; " + q(r.resultLabel) + " = " + q(r.resultValue);
        md += "\n";
    }
    m_ctx->appendNotes(md);
}

void ExperimentPage::showRegisters(LcdRegisterView* a, LcdRegisterView* b, const Report& r) {
    if (!r.registers.empty()) a->setRegister(r.registers[0]);
    if (r.registers.size() > 1) { b->setRegister(r.registers[1]); b->show(); } else b->hide();
}

// ============================================================================ Refraction & TIR
RefractionPage::RefractionPage(AppContext* ctx, QWidget* parent)
    : ExperimentPage("Refraction & TIR", "EXPERIMENT 01 / Snell's law and the critical angle", "Refraction & TIR", ctx, parent) {
    auto cols = addThreeColumns();
    auto* controls = new Panel("Experiment controls");
    controls->body()->setSpacing(10);
    cols.left->addWidget(controls, 1);

    m_mode = new ModeTabs({"Refraction", "TIR"});
    m_med1 = mediaCombo(ctx->presets, true);
    m_med2 = mediaCombo(ctx->presets, true);
    m_n1 = new UnitField;  m_n1->setSpinner(true, 0.01);
    m_n2 = new UnitField;  m_n2->setSpinner(true, 0.01);
    m_theta = new UnitField("°");
    m_sigmaN1 = new UnitField; m_sigmaN2 = new UnitField; m_sigmaTheta = new UnitField("°");
    m_sigmaN1->setObjectName(FieldIds::sigmaRefractiveIndex1);
    m_sigmaN2->setObjectName(FieldIds::sigmaRefractiveIndex2);
    m_sigmaTheta->setObjectName(FieldIds::sigmaIncidentAngle);
    bind(QString::fromLatin1(FieldIds::refractiveIndex1), m_n1); bind(QString::fromLatin1(FieldIds::refractiveIndex2), m_n2); bind(QString::fromLatin1(FieldIds::incidentAngle), m_theta);
    bind(QString::fromLatin1(FieldIds::sigmaRefractiveIndex1), m_sigmaN1); bind(QString::fromLatin1(FieldIds::sigmaRefractiveIndex2), m_sigmaN2);
    bind(QString::fromLatin1(FieldIds::sigmaIncidentAngle), m_sigmaTheta);
    auto* f = controls->body();
    f->addWidget(m_mode);
    f->addWidget(labeled("Incident medium", m_med1));
    f->addWidget(labeled("Refractive index · n₁", m_n1));
    f->addWidget(labeled("Transmitted medium", m_med2));
    f->addWidget(labeled("Refractive index · n₂", m_n2));
    f->addWidget(labeled("Incidence angle · θ₁", m_theta));
    f->addWidget(uncertaintyInputs({{"σ(n₁)", m_sigmaN1}, {"σ(n₂)", m_sigmaN2}, {"σ(θ₁)", m_sigmaTheta}}));
    buildButtons(f, "SI units · angles in degrees");

    m_canvasStack = new QStackedWidget;
    m_canvas = new RefractionCanvas;
    m_tirCanvas = new TirCanvas;
    m_canvasStack->addWidget(m_canvas);
    m_canvasStack->addWidget(m_tirCanvas);
    m_diagram = new Panel("Ray diagram / two incidence cases", "LIVE · 1:1");
    m_diagram->body()->setContentsMargins(0, 0, 0, 0);
    m_diagram->body()->addWidget(m_canvasStack);
    m_chart = new LineChart;
    m_chartPanel = new Panel("Regression analysis", "");
    m_chartPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_chartPanel->body()->addWidget(m_chart);
    cols.center->addWidget(m_diagram, 3);
    cols.center->addWidget(m_chartPanel, 2);

    m_results = new Panel("Results / Snell's law", ctx->nextRunLabel());
    m_reg1 = new LcdRegisterView;
    m_reg2 = new LcdRegisterView;
    m_results->body()->addWidget(m_reg1);
    m_results->body()->addWidget(m_reg2);
    m_notice = new NoticeCard;
    cols.right->addWidget(m_results);
    cols.right->addWidget(m_notice);
    cols.right->addStretch();
    setPanels(m_diagram, m_chartPanel, m_results, "LIVE · 1:1", "");

    connect(m_med1, &QComboBox::activated, this, [this] { onMediumChosen(m_med1, m_n1); });
    connect(m_med2, &QComboBox::activated, this, [this] { onMediumChosen(m_med2, m_n2); });
    connect(m_n1, &UnitField::edited, this, [this] { onIndexEdited(m_med1, m_n1); });
    connect(m_n2, &UnitField::edited, this, [this] { onIndexEdited(m_med2, m_n2); });
    connect(m_theta, &UnitField::edited, this, [this] { apply(false); });
    for (auto* field : {m_sigmaN1, m_sigmaN2, m_sigmaTheta}) connect(field, &UnitField::edited, this, [this] { apply(false); });
    connect(m_mode, &ModeTabs::currentChanged, this, [this] {
        const bool tir = m_mode->current() == 1;
        if (tir != m_lastModeTir) { m_theta->setValue(tir ? 50.0 : 30.0, 2); m_lastModeTir = tir; }   // the design's default angle per mode
        updateModeTexts();
        apply(false);
    });
    reset();
}

QString RefractionPage::mediumName(QComboBox* combo) const {
    static const std::pair<const char*, const char*> names[] = {{"crown", "Crown glass"}, {"flint", "Flint glass"}, {"acrylic", "Acrylic"},
                                                                {"water", "Water"}, {"diamond", "Diamond"}, {"air", "Air"}};
    const QString id = combo->currentData().toString();
    for (const auto& [key, label] : names) if (id == key) return label;
    return id.isEmpty() ? "Medium" : shortName(combo->currentText());
}

void RefractionPage::updateModeTexts() {
    const bool tir = m_mode->current() == 1;
    m_canvasStack->setCurrentIndex(tir ? 1 : 0);
    m_diagram->setTitle(tir ? "Total internal reflection / ray diagram" : "Ray diagram / two incidence cases");
    m_chartPanel->setTitle(tir ? "Critical-angle sweep" : "Regression analysis");
    m_results->setTitle(tir ? "Results / total internal reflection" : "Results / Snell's law");
    setNormalHint(tir ? "SI units · angles in degrees\n\nTIR requires n₁ > n₂\nand θ₁ > θc." : "SI units · angles in degrees");
}

void RefractionPage::onMediumChosen(QComboBox* combo, UnitField* field) {
    if (const auto* m = m_ctx->presets.findMedium(combo->currentData().toString().toStdString())) field->setValue(m->n);
    apply(false);
}

void RefractionPage::onIndexEdited(QComboBox* combo, UnitField* field) {
    bool ok = false;
    const double v = field->value(&ok);
    const auto* m = m_ctx->presets.findMedium(combo->currentData().toString().toStdString());
    if (ok && (!m || std::fabs(m->n - v) > 1e-9)) combo->setCurrentIndex(combo->count() - 1);    // "Custom"
    apply(false);
}

void RefractionPage::reset() {
    m_lastModeTir = false;
    m_mode->setCurrent(0);
    m_med1->setCurrentIndex(m_med1->findData("crown"));
    m_med2->setCurrentIndex(m_med2->findData("air"));
    m_n1->setValue(1.5);
    m_n2->setValue(1.0);
    m_theta->setValue(30.0, 2);
    m_sigmaN1->setValue(0.0, 4); m_sigmaN2->setValue(0.0, 4); m_sigmaTheta->setValue(0.0, 4);
    m_haveLast = false;
    updateModeTexts();
    apply(false);
}

void RefractionPage::apply(bool record) {
    if (!readFields()) return;
    RefractionInput in;
    in.n1 = m_n1->value(); in.n2 = m_n2->value(); in.theta1 = m_theta->value();
    in.sigmaN1 = m_sigmaN1->value(); in.sigmaN2 = m_sigmaN2->value(); in.sigmaTheta1 = m_sigmaTheta->value();
    in.tirMode = m_mode->current() == 1;
    in.name1 = mediumName(m_med1).toStdString();
    in.name2 = mediumName(m_med2).toStdString();

    const Report r = ER::refraction(in, reportContext());
    showRegisters(m_reg1, m_reg2, r);
    m_notice->setNotice(r.notices[0]);
    applyIssues(r);
    if (r.paused) return;                                         // keep the last valid diagram and chart

    m_last = r;
    m_haveLast = true;
    captureValid();
    m_canvas->setInput(in, shortName(q(in.name1)), shortName(q(in.name2)));
    m_tirCanvas->setInput(in);

    if (in.tirMode) {
        const auto crit = PC::criticalAngleDeg(in.n1, in.n2);
        const auto critError = propagateUncertainty({in.n1, in.n2}, {in.sigmaN1, in.sigmaN2}, [](const std::vector<double>& v) {
            const double ratio = v[1] / v[0];
            return ratio >= 0.0 && ratio < 1.0 ? std::asin(ratio) * 180.0 / std::numbers::pi : kNaN;
        });
        ChartSeries curve;
        curve.line = true;
        const double top = crit ? *crit : 90.0;
        for (double a = 0; a <= top + 1e-9; a += 0.5) {
            const double s = in.n1 * std::sin(a * std::numbers::pi / 180.0) / in.n2;
            if (std::fabs(s) <= 1.0) curve.points << QPointF(a, std::asin(s) * 180.0 / std::numbers::pi);
        }
        const Theme& t = ThemeManager::instance().theme();
        QList<ChartVLine> v{{in.theta1, QString::number(in.theta1, 'g', 5) + "° input", t.text, false}};
        QList<ChartRegion> reg;
        QList<ChartPoint> pts;
        if (crit) {
            v.prepend({*crit, "θc = " + QString::number(*crit, 'f', 4) + "°", t.sage, true});
            reg << ChartRegion{*crit, 90, t.amber, "TIR / no real θ₂"};
            ChartPoint criticalPoint{*crit, 90, "", t.sage, false};
            criticalPoint.xUncertainty = critError.valid ? critError.standardUncertainty : 0.0;
            pts << criticalPoint;
        }
        ChartSeries samples;
        samples.markers = true;
        for (const auto& row : ER::tirSweep(in).rows) if (std::isfinite(row[1])) {
            samples.points << QPointF(row[0], row[1]);
            samples.xUncertainties << in.sigmaTheta1;
            samples.yUncertainties << row[2];
        }
        m_chart->setAxes(0, 90, 3, 0, "θ₁ / degrees", 0, 90, 2, 0, "θ₂ / degrees");
        m_chart->setSeries({curve, samples});
        m_chart->setAnnotations(v, {}, reg, pts);
        m_chart->setLegend({"Samples · error bars = 1σ"}, "θ₂ = asin(" + QString::number(in.n1 / in.n2, 'g', 5) + " sin θ₁), 0 ≤ θ₁ ≤ θc only");
    } else {
        const DataSeries sweep = ER::refractionSweep(in);
        ChartSeries samples, fitLine;
        std::vector<double> xs, ys, ySigmas;
        for (const auto& row : sweep.rows)
            if (std::isfinite(row[1])) {
                xs.push_back(row[2]); ys.push_back(row[3]); ySigmas.push_back(row[6]);
                samples.points << QPointF(row[2], row[3]);
                samples.xUncertainties << row[5]; samples.yUncertainties << row[6];
            }
        samples.markers = true;
        const auto fit = ER::linearFit(xs, ys, ySigmas);
        if (fit.ok) { fitLine.line = true; fitLine.points << QPointF(0, fit.intercept) << QPointF(0.6, fit.slope * 0.6 + fit.intercept); }
        m_chart->clearAnnotations();
        m_chart->setAxes(0, 0.6, 4, 2, "sin θ₁", 0, 1.0, 2, 1, "sin θ₂");
        m_chart->setSeries({fitLine, samples});
        const QString fitCaption = fit.ok ? QString("%1 fit (1σ) · m %2 ± %3 · b %4 ± %5 · R² %6")
            .arg(fit.weighted ? QStringLiteral("Weighted") : QStringLiteral("OLS"))
            .arg(fit.slope, 0, 'f', 4).arg(fit.slopeUncertainty, 0, 'g', 3)
            .arg(fit.intercept, 0, 'f', 4).arg(fit.interceptUncertainty, 0, 'g', 3).arg(fit.r2, 0, 'f', 4) : QString();
        m_chart->setLegend({"Samples · 1σ bars"}, fitCaption);
    }

    Snapshot snap{"Refraction & TIR", r.parameters, r.registers, r.headline, in.tirMode ? ER::tirSweep(in) : ER::refractionSweep(in), {}};
    m_ctx->publish(snap);
    if (record) {
        const QString n1 = shortName(m_med1->currentText()), n2 = shortName(m_med2->currentText());
        const QString desc = m_med1->currentText() == "Custom" || m_med2->currentText() == "Custom"
            ? QString("n₁ %1 → n₂ %2 / %3°").arg(in.n1).arg(in.n2).arg(in.theta1)
            : QString("%1 → %2 / %3°").arg(n1, n2).arg(in.theta1);
        m_ctx->recordRun("Refraction & TIR", (in.tirMode ? "TIR · " : "") + desc, q(r.headline), true);
        refreshMetas(false);
    }
}

// ============================================================================ Lenses & Mirrors
LensesPage::LensesPage(AppContext* ctx, QWidget* parent)
    : ExperimentPage("Lenses & Mirrors", "EXPERIMENT 02 / Gaussian optics · real-is-positive sign convention", "Lenses & Mirrors", ctx, parent) {
    auto cols = addThreeColumns();
    auto* controls = new Panel("Experiment controls");
    controls->body()->setSpacing(10);
    cols.left->addWidget(controls, 1);

    m_mode = new ModeTabs({"Thin lens", "Mirror"});
    m_type = new QComboBox;
    m_f = new UnitField("cm");  m_f->setShowPlus(true);
    m_r = new UnitField("cm");  m_r->setShowPlus(true);
    m_s = new UnitField("cm");  m_s->setShowPlus(true);
    m_h = new UnitField("cm");  m_h->setShowPlus(true);
    m_sigmaF = new UnitField("cm"); m_sigmaS = new UnitField("cm"); m_sigmaH = new UnitField("cm");
    m_sigmaF->setObjectName(FieldIds::sigmaFocalLength);
    m_sigmaS->setObjectName(FieldIds::sigmaObjectDistance);
    m_sigmaH->setObjectName(FieldIds::sigmaObjectHeight);
    bind(QString::fromLatin1(FieldIds::focalLength), m_f); bind(QString::fromLatin1(FieldIds::objectDistance), m_s); bind(QString::fromLatin1(FieldIds::objectHeight), m_h);
    bind(QString::fromLatin1(FieldIds::sigmaFocalLength), m_sigmaF); bind(QString::fromLatin1(FieldIds::sigmaObjectDistance), m_sigmaS);
    bind(QString::fromLatin1(FieldIds::sigmaObjectHeight), m_sigmaH);
    m_typeLabel = new QLabel("Lens type");
    m_typeLabel->setObjectName("fieldLabel");
    auto* typeBox = new QWidget;
    auto* tl = new QVBoxLayout(typeBox);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(5);
    tl->addWidget(m_typeLabel);
    tl->addWidget(m_type);
    m_radiusBlock = labeled("Radius of curvature · R = 2f", m_r);
    auto* f = controls->body();
    f->addWidget(m_mode);
    f->addWidget(typeBox);
    f->addWidget(labeled("Focal length · f", m_f));
    f->addWidget(m_radiusBlock);
    f->addWidget(labeled("Object distance · dₒ", m_s));
    f->addWidget(labeled("Object height · hₒ", m_h));
    f->addWidget(uncertaintyInputs({{"σ(f)", m_sigmaF}, {"σ(dₒ)", m_sigmaS}, {"σ(hₒ)", m_sigmaH}}));
    buildButtons(f, "Real-is-positive. Distances measured from the optical element.");

    // centre: lens mode = lens + mirror comparison, mirror mode = one detailed mirror + chart
    m_centerStack = new QStackedWidget;
    auto* lensPage = new QWidget;
    auto* lp = new QVBoxLayout(lensPage);
    lp->setContentsMargins(0, 0, 0, 0);
    lp->setSpacing(12);
    m_lens = new LensCanvas;
    m_lensPanel = new Panel("Thin lens / principal rays", "SCALE: cm");
    m_lensPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_lensPanel->body()->addWidget(m_lens);
    m_mirrorSmall = new MirrorCanvas;
    m_mirrorSmallPanel = new Panel("Spherical mirror / concave", "");
    m_mirrorSmallPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_mirrorSmallPanel->body()->addWidget(m_mirrorSmall);
    lp->addWidget(m_lensPanel, 1);
    lp->addWidget(m_mirrorSmallPanel, 1);

    auto* mirrorPage = new QWidget;
    auto* mp = new QVBoxLayout(mirrorPage);
    mp->setContentsMargins(0, 0, 0, 0);
    mp->setSpacing(12);
    m_mirrorBig = new MirrorCanvas;
    m_mirrorBig->setDetailed(true);
    m_mirrorBigPanel = new Panel("Convex spherical mirror / principal rays", "SCALE: cm");
    m_mirrorBigPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_mirrorBigPanel->body()->addWidget(m_mirrorBig);
    m_chart = new LineChart;
    m_chartPanel = new Panel("Image distance vs object distance", "");
    m_chartPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_chartPanel->body()->addWidget(m_chart);
    mp->addWidget(m_mirrorBigPanel, 3);
    mp->addWidget(m_chartPanel, 2);
    m_centerStack->addWidget(lensPage);
    m_centerStack->addWidget(mirrorPage);
    cols.center->addWidget(m_centerStack, 1);

    m_results = new Panel("Image calculation", ctx->nextRunLabel());
    m_reg1 = new LcdRegisterView;
    m_reg2 = new LcdRegisterView;
    m_results->body()->addWidget(m_reg1);
    m_results->body()->addWidget(m_reg2);
    m_notice = new NoticeCard;
    m_caption = new QLabel;
    m_caption->setObjectName("caption");
    cols.right->addWidget(m_results);
    cols.right->addWidget(m_notice);
    cols.right->addWidget(m_caption);
    cols.right->addStretch();
    setPanels(m_lensPanel, m_chartPanel, m_results, "SCALE: cm", "");

    connect(m_mode, &ModeTabs::currentChanged, this, [this] { updateModeTexts(); apply(false); });
    connect(m_type, &QComboBox::activated, this, [this] { onTypeChosen(); });
    connect(m_f, &UnitField::edited, this, [this] { onFocalEdited(); });
    connect(m_r, &UnitField::edited, this, [this] { onRadiusEdited(); });
    for (auto* field : {m_s, m_h, m_sigmaF, m_sigmaS, m_sigmaH}) connect(field, &UnitField::edited, this, [this] { apply(false); });
    reset();
}

void LensesPage::updateModeTexts() {
    const bool mirror = m_mode->current() == 1;
    const int sign = m_type->currentIndex();
    QSignalBlocker block(m_type);
    m_type->clear();
    m_type->addItems(mirror ? QStringList{"Concave · spherical", "Convex · spherical"} : QStringList{"Converging · biconvex", "Diverging · biconcave"});
    m_type->setCurrentIndex(std::max(0, sign));
    m_typeLabel->setText(mirror ? "Mirror type" : "Lens type");
    m_radiusBlock->setVisible(mirror);
    m_centerStack->setCurrentIndex(mirror ? 1 : 0);
    setPanels(mirror ? m_mirrorBigPanel : m_lensPanel, m_chartPanel, m_results, "SCALE: cm", "");
    setNormalHint(mirror ? "Real-is-positive. For a convex mirror, f and R are negative: the focus and center lie behind the mirror."
                         : "Real-is-positive. Distances measured from the optical element.");
}

void LensesPage::onTypeChosen() {                          // the type decides the sign of f
    bool ok = false;
    const double f = m_f->value(&ok);
    const double magnitude = ok && f != 0 ? std::fabs(f) : 10.0;
    m_syncing = true;
    m_f->setValue(m_type->currentIndex() == 1 ? -magnitude : magnitude, 2);
    m_r->setValue(2 * m_f->value(), 2);
    m_syncing = false;
    apply(false);
}

void LensesPage::onFocalEdited() {                         // the sign of f decides the type; R follows
    if (m_syncing) return;
    bool ok = false;
    const double f = m_f->value(&ok);
    if (ok && f != 0) {
        m_syncing = true;
        m_type->setCurrentIndex(f < 0 ? 1 : 0);
        m_r->setValue(2 * f, 2);
        m_syncing = false;
    }
    apply(false);
}

void LensesPage::onRadiusEdited() {                        // R = 2f
    if (m_syncing) return;
    bool ok = false;
    const double r = m_r->value(&ok);
    if (ok && r != 0) {
        m_syncing = true;
        m_f->setValue(r / 2, 2);
        m_type->setCurrentIndex(r < 0 ? 1 : 0);
        m_syncing = false;
    }
    apply(false);
}

void LensesPage::reset() {
    m_mode->setCurrent(0);
    updateModeTexts();
    m_type->setCurrentIndex(0);
    m_syncing = true;
    m_f->setValue(10.0, 2);
    m_r->setValue(20.0, 2);
    m_s->setValue(30.0, 2);
    m_h->setValue(3.0, 2);
    m_sigmaF->setValue(0.0, 4); m_sigmaS->setValue(0.0, 4); m_sigmaH->setValue(0.0, 4);
    m_syncing = false;
    m_haveLast = false;
    apply(false);
}

void LensesPage::apply(bool record) {
    if (m_syncing || !readFields()) return;
    ImagingInput in;
    in.f = m_f->value(); in.s = m_s->value(); in.h = m_h->value();
    in.sigmaF = m_sigmaF->value(); in.sigmaS = m_sigmaS->value(); in.sigmaH = m_sigmaH->value();
    in.mirrorMode = m_mode->current() == 1;

    const Report r = ER::imaging(in, reportContext());
    showRegisters(m_reg1, m_reg2, r);
    m_notice->setNotice(r.notices[0]);
    m_caption->setText(q(r.footnote));
    m_caption->setVisible(!r.footnote.empty());
    applyIssues(r);
    if (r.paused) return;

    m_last = r;
    m_haveLast = true;
    captureValid();
    if (std::fabs(m_r->value() - 2 * in.f) > 1e-9) { QSignalBlocker b(m_r); m_r->setValue(2 * in.f, 2); }
    m_lens->setInput(in);
    m_mirrorSmall->setInput(in);
    m_mirrorBig->setInput(in);
    const bool concave = in.f > 0;
    m_mirrorSmallPanel->setTitle(concave ? "Spherical mirror / concave" : "Spherical mirror / convex");
    m_mirrorSmallPanel->setMeta(QString("R = 2f = %1 cm").arg(QString::number(2 * in.f, 'g', 6)));
    m_mirrorBigPanel->setTitle(concave ? "Concave spherical mirror / principal rays" : "Convex spherical mirror / principal rays");
    if (in.mirrorMode) {
        StatusInfo info;
        info.mid = q(r.statusText);
        setStatusInfo(info, false);
    }

    if (in.mirrorMode) {                                     // di(do) curve with the current sample
        const double af = std::fabs(in.f);
        QList<ChartSeries> series;
        ChartSeries branch;
        branch.line = true;
        double lo = 1e9, hi = -1e9;
        for (double so = 0.5; so <= 60.0 + 1e-9; so += 0.25) {
            const auto img = PC::imageFormation(so, in.f);
            if (img.atInfinity || std::fabs(so - in.f) < 0.3 || std::fabs(img.imageDistance) > 4 * af + 1e-9 * 0) {
                if (!branch.points.isEmpty()) { series << branch; branch = ChartSeries(); branch.line = true; }
                continue;
            }
            branch.points << QPointF(so, img.imageDistance);
            lo = std::min(lo, img.imageDistance);
            hi = std::max(hi, img.imageDistance);
        }
        if (!branch.points.isEmpty()) series << branch;
        const double cur = PC::imageFormation(in.s, in.f).imageDistance;
        lo = std::min({lo, cur, in.f < 0 ? in.f : 0.0});
        hi = std::max({hi, cur, in.f < 0 ? 0.0 : in.f});
        double y0, y1;
        niceRange(std::min(lo, 0.0), std::max(hi, 0.0), 2, y0, y1);
        const Theme& t = ThemeManager::instance().theme();
        m_chart->setAxes(0, 60, 4, 0, "dₒ (cm)", y0, y1, 2, 0, "dᵢ / cm");
        m_chart->setSeries(series);
        const auto imageError = propagateUncertainty({in.f, in.s}, {in.sigmaF, in.sigmaS}, [](const std::vector<double>& v) {
            const double denominator = 1.0 / v[0] - 1.0 / v[1];
            return std::fabs(denominator) > 1e-15 ? 1.0 / denominator : kNaN;
        });
        ChartPoint currentPoint{in.s, cur, QString("Current: (%1, %2) cm").arg(signedText(in.s, 0), QString::number(cur, 'g', 4)), t.sage, true};
        currentPoint.xUncertainty = in.sigmaS;
        currentPoint.yUncertainty = imageError.valid ? imageError.standardUncertainty : 0.0;
        m_chart->setAnnotations({{in.s, "", t.dim, false}}, {{in.f, "f = " + QString::number(in.f, 'g', 5) + " cm", t.dim}}, {}, {currentPoint});
        const QString fText = QString::number(in.f, 'g', 5);
        m_chart->setLegend({}, QString("▬ dᵢ = %1dₒ/(dₒ %2 %3)   ● Current · error bars = 1σ").arg(fText, in.f < 0 ? "+" : "−").arg(QString::number(af, 'g', 5)));
    }

    m_ctx->publish({"Lenses & Mirrors", r.parameters, r.registers, r.headline, ER::imagingSweep(in), {}});
    if (record) {
        m_ctx->recordRun("Lenses & Mirrors", QString("%1 · f %2 cm").arg(in.mirrorMode ? (concave ? "Concave mirror" : "Convex mirror") : (concave ? "Converging lens" : "Diverging lens"))
                                                 .arg(QString::number(in.f, 'g', 5)), q(r.headline), true);
        refreshMetas(false);
    }
}

// ============================================================================ Prisms
PrismsPage::PrismsPage(AppContext* ctx, QWidget* parent)
    : ExperimentPage("Prisms", "EXPERIMENT 03 / Refraction through a prism · minimum deviation", "Prisms", ctx, parent) {
    auto cols = addThreeColumns();
    auto* controls = new Panel("Experiment controls");
    controls->body()->setSpacing(10);
    cols.left->addWidget(controls, 1);

    m_mode = new ModeTabs({"Single ray", "Dispersion"});
    m_material = mediaCombo(ctx->presets, false);
    m_surround = mediaCombo(ctx->presets, true);
    m_apex = new UnitField("°");
    m_incidence = new UnitField("°");
    m_lambda = new UnitField("nm");
    m_sigmaNPrism = new UnitField; m_sigmaNMedium = new UnitField;
    m_sigmaApex = new UnitField("°"); m_sigmaIncidence = new UnitField("°"); m_sigmaLambda = new UnitField("nm");
    m_sigmaNPrism->setObjectName(FieldIds::sigmaPrismIndex);
    m_sigmaNMedium->setObjectName(FieldIds::sigmaMediumIndex);
    m_sigmaApex->setObjectName(FieldIds::sigmaPrismApex);
    m_sigmaIncidence->setObjectName(FieldIds::sigmaPrismIncidence);
    m_sigmaLambda->setObjectName(FieldIds::sigmaWavelength);
    bind(QString::fromLatin1(FieldIds::prismApex), m_apex); bind(QString::fromLatin1(FieldIds::incidenceAngle), m_incidence); bind(QString::fromLatin1(FieldIds::wavelength), m_lambda);
    bind(QString::fromLatin1(FieldIds::sigmaPrismIndex), m_sigmaNPrism); bind(QString::fromLatin1(FieldIds::sigmaMediumIndex), m_sigmaNMedium);
    bind(QString::fromLatin1(FieldIds::sigmaPrismApex), m_sigmaApex); bind(QString::fromLatin1(FieldIds::sigmaPrismIncidence), m_sigmaIncidence);
    bind(QString::fromLatin1(FieldIds::sigmaWavelength), m_sigmaLambda);
    m_materialLabel = new QLabel;
    m_materialLabel->setObjectName("fieldLabel");
    m_surroundLabel = new QLabel;
    m_surroundLabel->setObjectName("fieldLabel");
    auto holder = [](QLabel* label, QWidget* field) {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(5);
        l->addWidget(label);
        l->addWidget(field);
        return w;
    };
    auto* f = controls->body();
    f->addWidget(m_mode);
    f->addWidget(holder(m_materialLabel, m_material));
    f->addWidget(holder(m_surroundLabel, m_surround));
    f->addWidget(labeled("Apex angle · A", m_apex));
    f->addWidget(labeled("Incidence angle · i₁", m_incidence));
    f->addWidget(labeled("Wavelength · λ", m_lambda));
    f->addWidget(uncertaintyInputs({{"σ(n prism)", m_sigmaNPrism}, {"σ(n medium)", m_sigmaNMedium},
                                    {"σ(A)", m_sigmaApex}, {"σ(i₁)", m_sigmaIncidence}, {"σ(λ)", m_sigmaLambda}}));
    buildButtons(f, "SI units · angles in degrees");

    m_canvas = new PrismCanvas;
    m_diagram = new Panel("Prism ray path", "SYMMETRIC PASSAGE");
    m_diagram->body()->setContentsMargins(0, 0, 0, 0);
    m_diagram->body()->addWidget(m_canvas);
    m_chart = new LineChart;
    m_chartPanel = new Panel("Deviation vs. incidence angle", "");
    m_chartPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_chartPanel->body()->addWidget(m_chart);
    cols.center->addWidget(m_diagram, 3);
    cols.center->addWidget(m_chartPanel, 2);

    m_results = new Panel("Deviation register", ctx->nextRunLabel());
    m_reg = new LcdRegisterView;
    m_noticeInline = new NoticeCard;
    m_footnote = new QLabel;
    m_footnote->setObjectName("mono");
    m_results->body()->addWidget(m_reg);
    m_results->body()->addWidget(m_noticeInline);
    m_results->body()->addWidget(m_footnote);
    m_noticeExit = new NoticeCard;
    cols.right->addWidget(m_results);
    cols.right->addWidget(m_noticeExit);
    cols.right->addStretch();
    setPanels(m_diagram, m_chartPanel, m_results, "SYMMETRIC PASSAGE", "");

    connect(m_mode, &ModeTabs::currentChanged, this, [this] { apply(false); });
    connect(m_material, &QComboBox::activated, this, [this] { apply(false); });
    connect(m_surround, &QComboBox::activated, this, [this] { apply(false); });
    for (auto* field : {m_apex, m_incidence, m_lambda, m_sigmaNPrism, m_sigmaNMedium, m_sigmaApex, m_sigmaIncidence, m_sigmaLambda})
        connect(field, &UnitField::edited, this, [this] { apply(false); });
    reset();
}

double PrismsPage::indexOf(QComboBox* combo, double lambda) const {
    const auto* m = m_ctx->presets.findMedium(combo->currentData().toString().toStdString());
    if (!m) return 1.5;
    return (m->cauchyA != 0.0 || m->cauchyB != 0.0) ? PC::cauchyIndex(m->cauchyA, m->cauchyB, lambda) : m->n;
}

void PrismsPage::reset() {
    m_mode->setCurrent(0);
    m_material->setCurrentIndex(m_material->findData("crown"));
    m_surround->setCurrentIndex(m_surround->findData("air"));
    m_apex->setValue(60.0, 2);
    m_incidence->setValue(48.5904, 4);
    m_lambda->setValue(589.3, 1);
    m_sigmaNPrism->setValue(0.0, 5); m_sigmaNMedium->setValue(0.0, 5);
    m_sigmaApex->setValue(0.0, 4); m_sigmaIncidence->setValue(0.0, 4); m_sigmaLambda->setValue(0.0, 4);
    m_haveLast = false;
    apply(false);
}

void PrismsPage::apply(bool record) {
    if (!readFields()) return;
    PrismInput in;
    in.apex = m_apex->value(); in.incidence = m_incidence->value(); in.wavelengthNm = m_lambda->value();
    in.sigmaNPrism = m_sigmaNPrism->value(); in.sigmaNMedium = m_sigmaNMedium->value();
    in.sigmaApex = m_sigmaApex->value(); in.sigmaIncidence = m_sigmaIncidence->value(); in.sigmaWavelengthNm = m_sigmaLambda->value();
    in.dispersion = m_mode->current() == 1;
    const double lambda = std::max(1.0, in.wavelengthNm);
    in.nPrism = indexOf(m_material, lambda);
    in.nMedium = indexOf(m_surround, lambda);
    auto wavelengthSlope = [this, lambda](QComboBox* combo) {
        const auto* medium = m_ctx->presets.findMedium(combo->currentData().toString().toStdString());
        if (!medium || medium->cauchyB == 0.0) return 0.0;
        return -2.0 * medium->cauchyB * 1e6 / (lambda * lambda * lambda);
    };
    in.dnPrismDlambda = wavelengthSlope(m_material);
    in.dnMediumDlambda = wavelengthSlope(m_surround);
    m_materialLabel->setText(QString("Prism material · n = %1").arg(in.nPrism, 0, 'f', 3));
    m_surroundLabel->setText(QString("Surrounding medium · n = %1").arg(in.nMedium, 0, 'f', 3));
    m_diagram->setTitle(in.dispersion ? "Prism ray path · visible spectrum" : "Prism ray path");
    setPanels(m_diagram, m_chartPanel, m_results,
              in.dispersion ? "7 COLOURS · 400–700 NM" : "SYMMETRIC PASSAGE", "");

    const Report r = ER::prism(in, reportContext());
    m_reg->setRegister(r.registers[0]);
    if (r.notices.size() >= 2) { m_noticeInline->setNotice(r.notices[0]); m_noticeInline->show(); m_noticeExit->setNotice(r.notices[1]); }
    else { m_noticeInline->hide(); m_noticeExit->setNotice(r.notices[0]); }
    m_footnote->setText(q(r.footnote));
    m_footnote->setVisible(!r.footnote.empty());
    applyIssues(r);
    if (r.paused) return;

    m_last = r;
    m_haveLast = true;
    captureValid();
    QList<PrismPath> extra;
    if (in.dispersion) {
        struct SpectrumSample { double wavelengthNm; const char* color; };
        static constexpr SpectrumSample spectrum[] = {
            {700.0, "#e53935"}, // red
            {620.0, "#fb8c00"}, // orange
            {580.0, "#fdd835"}, // yellow
            {540.0, "#43a047"}, // green
            {490.0, "#1e88e5"}, // blue
            {450.0, "#3f51b5"}, // indigo
            {400.0, "#8e24aa"}  // violet
        };
        extra.reserve(7);
        for (const SpectrumSample& sample : spectrum) {
            extra.append({QColor(sample.color), sample.wavelengthNm,
                          indexOf(m_material, sample.wavelengthNm),
                          indexOf(m_surround, sample.wavelengthNm)});
        }
    }
    m_canvas->setInput(in, extra);

    ChartSeries curve;
    double lo = 1e9, hi = -1e9;
    for (double a = 35; a <= 65.001; a += 0.5) {
        const auto p = PC::prism(in.nPrism, in.nMedium, in.apex, a);
        if (!p.valid || p.entryTir || p.exitTir) continue;
        curve.points << QPointF(a, p.deviation);
        lo = std::min(lo, p.deviation);
        hi = std::max(hi, p.deviation);
    }
    curve.line = true;
    ChartSeries samples;
    samples.markers = true;
    for (const auto& row : ER::prismSweep(in).rows) if (std::isfinite(row[1])) {
        samples.points << QPointF(row[0], row[1]);
        samples.xUncertainties << row[2]; samples.yUncertainties << row[3];
    }
    if (!curve.points.isEmpty()) {
        int y0 = int(std::floor(lo)) - 1, y1 = int(std::ceil(hi)) + 1;
        y1 = y0 + static_cast<int>(std::ceil((y1 - y0) / 4.0)) * 4;
        m_chart->setAxes(35, 65, 4, 1, "i₁ (°)", y0, y1, 2, 0, "δ (°)");
    }
    m_chart->setSeries({curve, samples});
    const auto imin = PC::minimumDeviationIncidenceDeg(in.nPrism, in.nMedium, in.apex);
    m_chart->setMarkerX(imin.value_or(0), imin.has_value());
    m_chart->setLegend({"Sweep samples · error bars = 1σ"}, imin ? QString("Ray-traced deviation · minimum at %1°").arg(*imin, 0, 'f', 2) : "");

    m_ctx->publish({"Prisms", r.parameters, r.registers, r.headline, ER::prismSweep(in), {}});
    if (record) {
        m_ctx->recordRun("Prisms", QString("Prism A %1° · i₁ %2°").arg(in.apex).arg(in.incidence, 0, 'f', 2), q(r.headline), true);
        refreshMetas(false);
    }
}

// ============================================================================ Wave interference
WavePage::WavePage(AppContext* ctx, QWidget* parent)
    : ExperimentPage("Wave Interference (Double-Slit)", "EXPERIMENT 04 / Young's double-slit experiment · coherent monochromatic light", "Wave Interference", ctx, parent) {
    auto cols = addThreeColumns();
    auto* controls = new Panel("Experiment controls");
    controls->body()->setSpacing(10);
    cols.left->addWidget(controls, 1);

    m_source = new QComboBox;
    for (const auto& s : ctx->presets.sources) m_source->addItem(q(s.name), s.wavelengthNm);
    m_source->addItem("Custom", -1.0);
    m_lambda = new UnitField("nm");
    m_d = new UnitField("mm");
    m_L = new UnitField("m");
    m_order = new UnitField;  m_order->setSpinner(true, 1.0);
    m_sigmaLambda = new UnitField("nm"); m_sigmaD = new UnitField("mm"); m_sigmaL = new UnitField("m");
    m_sigmaLambda->setObjectName(FieldIds::sigmaWavelength);
    m_sigmaD->setObjectName(FieldIds::sigmaSlitSeparation);
    m_sigmaL->setObjectName(FieldIds::sigmaScreenDistance);
    bind(QString::fromLatin1(FieldIds::wavelength), m_lambda); bind(QString::fromLatin1(FieldIds::slitSeparation), m_d); bind(QString::fromLatin1(FieldIds::screenDistance), m_L); bind(QString::fromLatin1(FieldIds::order), m_order);
    bind(QString::fromLatin1(FieldIds::sigmaWavelength), m_sigmaLambda); bind(QString::fromLatin1(FieldIds::sigmaSlitSeparation), m_sigmaD);
    bind(QString::fromLatin1(FieldIds::sigmaScreenDistance), m_sigmaL);
    m_view = new ModeTabs({"Intensity", "Wavefronts"});
    auto* f = controls->body();
    f->addWidget(labeled("Source preset", m_source));
    f->addWidget(labeled("Wavelength · λ", m_lambda));
    f->addWidget(labeled("Slit separation · d", m_d));
    f->addWidget(labeled("Screen distance · L", m_L));
    f->addWidget(labeled("Fringe order · m", m_order));
    f->addWidget(uncertaintyInputs({{"σ(λ)", m_sigmaLambda}, {"σ(d)", m_sigmaD}, {"σ(L)", m_sigmaL}}));
    f->addWidget(m_view);
    buildButtons(f, "Small angles: y ≈ Lθ. Ideal narrow slits.");

    m_canvas = new WaveCanvas;
    m_diagram = new Panel("Wavefronts and interference pattern", "SCHEMATIC · NOT TO SCALE");
    m_diagram->body()->setContentsMargins(0, 0, 0, 0);
    m_diagram->body()->addWidget(m_canvas);
    m_chart = new LineChart;
    m_chartPanel = new Panel("Screen intensity profile", "");
    m_chartPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_chartPanel->body()->addWidget(m_chart);
    cols.center->addWidget(m_diagram, 3);
    cols.center->addWidget(m_chartPanel, 2);

    m_results = new Panel("Fringe calculation", ctx->nextRunLabel());
    m_reg1 = new LcdRegisterView;
    m_reg2 = new LcdRegisterView;
    m_results->body()->addWidget(m_reg1);
    m_results->body()->addWidget(m_reg2);
    m_notice = new NoticeCard;
    cols.right->addWidget(m_results);
    cols.right->addWidget(m_notice);
    cols.right->addStretch();
    setPanels(m_diagram, m_chartPanel, m_results, "SCHEMATIC · NOT TO SCALE", "");

    connect(m_source, &QComboBox::activated, this, [this] { onPresetChosen(); });
    connect(m_lambda, &UnitField::edited, this, [this] { onWavelengthEdited(); });
    for (auto* field : {m_d, m_L, m_order, m_sigmaLambda, m_sigmaD, m_sigmaL})
        connect(field, &UnitField::edited, this, [this] { apply(false); });
    connect(m_view, &ModeTabs::currentChanged, this, [this] { apply(false); });
    reset();
}

void WavePage::onPresetChosen() {
    const double w = m_source->currentData().toDouble();
    if (w > 0) m_lambda->setValue(w, 1);
    apply(false);
}

void WavePage::onWavelengthEdited() {
    bool ok = false;
    const double v = m_lambda->value(&ok);
    if (ok && std::fabs(m_source->currentData().toDouble() - v) > 1e-9) m_source->setCurrentIndex(m_source->count() - 1);
    apply(false);
}

void WavePage::reset() {
    m_source->setCurrentIndex(0);
    m_lambda->setValue(532.0, 1);
    m_d->setValue(0.25, 3);
    m_L->setValue(1.5, 3);
    m_order->setValue(2, 0);
    m_sigmaLambda->setValue(0.0, 4); m_sigmaD->setValue(0.0, 4); m_sigmaL->setValue(0.0, 4);
    m_view->setCurrent(0);
    m_haveLast = false;
    apply(false);
}

void WavePage::apply(bool record) {
    if (!readFields()) return;
    SlitInput in;
    in.wavelengthNm = m_lambda->value(); in.separationMm = m_d->value(); in.screenM = m_L->value();
    in.sigmaWavelengthNm = m_sigmaLambda->value(); in.sigmaSeparationMm = m_sigmaD->value(); in.sigmaScreenM = m_sigmaL->value();
    in.order = static_cast<int>(std::lround(m_order->value()));

    const Report r = ER::doubleSlit(in, reportContext());
    showRegisters(m_reg1, m_reg2, r);
    m_notice->setNotice(r.notices[0]);
    applyIssues(r);
    if (r.paused) return;                                    // the diagram and chart keep showing the last valid run

    m_last = r;
    m_haveLast = true;
    captureValid();
    m_canvas->setInput(in, m_view->current() == 1);

    ChartSeries curve;
    const double lam = in.wavelengthNm * 1e-9, d = in.separationMm * 1e-3;
    for (int k = -200; k <= 200; ++k) {
        const double y = k * 0.05;                           // mm
        curve.points << QPointF(y, PC::relativeIntensity(y * 1e-3, lam, in.screenM, d));
    }
    curve.line = true;
    ChartSeries errorSamples;
    errorSamples.markers = true;
    for (const auto& row : ER::doubleSlitSweep(in).rows) if (std::isfinite(row[1])) {
        errorSamples.points << QPointF(row[0], row[1]);
        errorSamples.xUncertainties << 0.0; errorSamples.yUncertainties << row[2];
    }
    m_chart->setAxes(-10, 10, 4, 0, "y (mm)", 0, 1.0, 2, 1, "I / I₀");
    m_chart->setSeries({curve, errorSamples});
    m_chart->setLegend({"Samples · error bars = 1σ"}, QString("cos²(πdy / λL) · d = %1 mm").arg(QString::number(in.separationMm, 'f', 3)));

    m_ctx->publish({"Wave Interference", r.parameters, r.registers, r.headline, ER::doubleSlitSweep(in), {}});
    if (record) {
        m_ctx->recordRun("Wave Interference", QString("Double-slit · %1 nm · d %2 mm").arg(in.wavelengthNm).arg(in.separationMm), q(r.headline), true);
        refreshMetas(false);
    }
}

} // namespace phy104
