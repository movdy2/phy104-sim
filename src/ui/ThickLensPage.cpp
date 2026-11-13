#include "ThickLensPage.h"

#include "AppContext.h"
#include "Components.h"
#include "OpticsCanvases.h"
#include "Plots.h"
#include "ThemeManager.h"
#include "ThickLens.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace phy104 {
namespace ER = ExperimentReports;
namespace TL = ThickLens;
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
}

ThickLensPage::ThickLensPage(AppContext* ctx, QWidget* parent)
    : ExperimentPage("Thick Lens & Aberration", "EXPERIMENT 11 / A real glass element: two radii, a finite thickness and aperture",
                     "Thick Lens & Aberration", ctx, parent) {
    auto cols = addThreeColumns();
    auto* controls = new Panel("Experiment controls");
    controls->body()->setSpacing(10);
    cols.left->addWidget(controls, 1);

    m_tabs = new ModeTabs({"Thick lens", "Spectrum"});
    m_type = new QComboBox;
    m_typeLabel = new QLabel("Element type");
    m_typeLabel->setObjectName("fieldLabel");
    auto* typeBox = new QWidget;
    auto* tl = new QVBoxLayout(typeBox);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(5);
    tl->addWidget(m_typeLabel);
    tl->addWidget(m_type);

    m_n = new UnitField;            m_n->setSpinner(true, 0.01);
    m_nMedium = new UnitField;      m_nMedium->setSpinner(true, 0.01);
    m_r1 = new UnitField("cm");     m_r1->setShowPlus(true);  m_r1->setSpinner(true, 1.0);
    m_r2 = new UnitField("cm");     m_r2->setShowPlus(true);  m_r2->setSpinner(true, 1.0);
    m_thickness = new UnitField("cm");
    m_diameter = new UnitField("cm");
    m_object = new UnitField("cm");
    bind("n", m_n); bind("nMedium", m_nMedium); bind("r1", m_r1); bind("r2", m_r2);
    bind("t", m_thickness); bind("D", m_diameter); bind("do", m_object);

    auto* f = controls->body();
    f->addWidget(m_tabs);
    f->addWidget(typeBox);
    f->addWidget(labeled("First radius · R₁", m_r1));
    f->addWidget(labeled("Second radius · R₂", m_r2));
    f->addWidget(labeled("Lens index · n", m_n));
    f->addWidget(labeled("Surrounding index · n₀", m_nMedium));
    f->addWidget(labeled("Centre thickness · t", m_thickness));
    f->addWidget(labeled("Clear aperture · D", m_diameter));
    f->addWidget(labeled("Object distance · dₒ (from V₁)", m_object));
    buildButtons(f, "Signed radii: positive is convex towards the light. Power, principal planes and the third-order aberration.");

    m_canvas = new ThickLensCanvas;
    m_diagram = new Panel("Thick element / principal planes", "SCALE: cm");
    m_diagram->body()->setContentsMargins(0, 0, 0, 0);
    m_diagram->body()->addWidget(m_canvas);
    m_chart = new LineChart;
    m_chartPanel = new Panel("Image distance vs object distance", "");
    m_chartPanel->body()->setContentsMargins(0, 0, 0, 0);
    m_chartPanel->body()->addWidget(m_chart);
    cols.center->addWidget(m_diagram, 3);
    cols.center->addWidget(m_chartPanel, 2);

    m_results = new Panel("Thick-lens calculation", ctx->nextRunLabel());
    m_reg1 = new LcdRegisterView;
    m_reg2 = new LcdRegisterView;
    m_results->body()->addWidget(m_reg1);
    m_results->body()->addWidget(m_reg2);
    m_notice = new NoticeCard;
    m_caption = new QLabel;
    m_caption->setObjectName("caption");
    m_caption->setWordWrap(true);
    cols.right->addWidget(m_results);
    cols.right->addWidget(m_notice);
    cols.right->addWidget(m_caption);
    cols.right->addStretch();
    setPanels(m_diagram, m_chartPanel, m_results, "SCALE: cm", "");

    connect(m_tabs, &ModeTabs::currentChanged, this, [this] { apply(false); });
    connect(m_type, &QComboBox::activated, this, [this] { onTypeChosen(); });
    for (auto* field : {m_n, m_nMedium, m_r1, m_r2, m_thickness, m_diameter, m_object}) {
        connect(field, &UnitField::edited, this, [this] { apply(false); });
    }
    reset();
}

void ThickLensPage::updateFigures() {
    // The type dropdown only sets the SIGN of both radii; the magnitudes stay what the user typed.
    const int index = m_type->currentIndex();
    {
        const QSignalBlocker block(m_type);
        m_type->clear();
        m_type->addItems({"Biconvex · converging", "Biconcave · diverging", "Plano-convex", "Plano-concave", "Meniscus"});
        m_type->setCurrentIndex(index < 0 ? 0 : index);
    }
    m_typeLabel->setText("Element type");
}

QString ThickLensPage::elementName() const {
    const auto r = TL::analyse({m_n->value(), m_nMedium->value(), m_r1->value(), m_r2->value(),
                                m_thickness->value(), m_diameter->value(), m_object->value()});
    return QString::fromStdString(r.element);
}

void ThickLensPage::onTypeChosen() {
    bool ok1 = false, ok2 = false;
    double r1 = m_r1->value(&ok1), r2 = m_r2->value(&ok2);
    r1 = ok1 ? std::fabs(r1) : 10.0;
    r2 = ok2 ? std::fabs(r2) : 10.0;
    if (r1 < 0.5) r1 = 10.0;
    if (r2 < 0.5) r2 = 10.0;
    double newR1 = +r1, newR2 = -r2;
    switch (m_type->currentIndex()) {
    case 0: newR1 = +r1; newR2 = -r2; break;                  // biconvex: first surface convex, second concave
    case 1: newR1 = -r1; newR2 = +r2; break;                  // biconcave
    case 2: newR1 = +r1; newR2 = 1.0e6; break;                // plano-convex: flat back surface
    case 3: newR1 = -r1; newR2 = -1.0e6; break;               // plano-concave
    case 4: newR1 = +r1; newR2 = -r2 * 0.6; break;            // meniscus: both curve the same way
    default: break;
    }
    m_r1->setValue(newR1, 2);
    m_r2->setValue(newR2, 2);
    apply(false);
}

void ThickLensPage::reset() {
    m_tabs->setCurrent(0);
    updateFigures();
    m_type->setCurrentIndex(0);
    m_n->setValue(1.5, 4);
    m_nMedium->setValue(1.0, 4);
    m_r1->setValue(10.0, 2);
    m_r2->setValue(-10.0, 2);
    m_thickness->setValue(2.0, 2);
    m_diameter->setValue(5.0, 2);
    m_object->setValue(30.0, 2);
    m_haveLast = false;
    apply(false);
}

void ThickLensPage::apply(bool record) {
    if (!readFields()) return;
    ThickLensInput in;
    in.n = m_n->value(); in.nMedium = m_nMedium->value();
    in.r1 = m_r1->value(); in.r2 = m_r2->value();
    in.thickness = m_thickness->value(); in.diameter = m_diameter->value();
    in.objectDistance = m_object->value();
    in.spectrum = m_tabs->current() == 1;

    const Report r = ER::thickLens(in, reportContext());
    showRegisters(m_reg1, m_reg2, r);
    if (!r.notices.empty()) m_notice->setNotice(r.notices[0]);
    m_caption->setText(q(r.footnote));
    m_caption->setVisible(!r.footnote.empty());
    applyIssues(r);
    if (r.paused) return;

    m_last = r;
    m_haveLast = true;
    captureValid();
    m_canvas->setInput(in);
    m_diagram->setTitle(in.spectrum ? "Thick element / three colours" : "Thick element / principal planes");
    m_chartPanel->setTitle(in.spectrum ? "Focal length per wavelength" : "Image distance vs object distance");

    const Theme& t = ThemeManager::instance().theme();
    m_chart->clearAnnotations();
    if (in.spectrum) {
        // three markers: the focal length in the red, green and blue line (same Cauchy pairs as the report)
        struct Line { const char* name; double nm; QColor colour; };
        const Line lines[] = {{"Red 656 nm", 656.3, QColor("#e05c4b")}, {"Green 588 nm", 587.6, QColor("#7bc96f")}, {"Blue 486 nm", 486.1, QColor("#5aa9e6")}};
        const auto cauchy = [&] {
            if (in.n < 1.45) return std::pair<double, double>{1.4750, 0.0140};
            if (in.n < 1.62) return std::pair<double, double>{1.5046, 0.0042};
            if (in.n < 1.75) return std::pair<double, double>{1.6030, 0.0100};
            return std::pair<double, double>{1.7150, 0.0150};
        }();
        double lo = 1e9, hi = -1e9;
        QList<ChartPoint> points;
        for (const Line& line : lines) {
            ThickLensInput s = in;
            s.n = TL::cauchyIndex(cauchy.first, cauchy.second, line.nm);
            const auto res = TL::analyse(s);
            if (!res.valid) continue;
            lo = std::min(lo, res.focalLength); hi = std::max(hi, res.focalLength);
            points << ChartPoint{line.nm, res.focalLength, QString::fromLatin1(line.name), line.colour, true};
        }
        if (lo > hi) { lo = 0; hi = 1; }
        if (hi - lo < 1e-9) { lo -= 0.5; hi += 0.5; }
        m_chart->setAxes(400, 700, 3, 0, "λ (nm)", lo - (hi - lo) * 0.2, hi + (hi - lo) * 0.2, 3, 3, "f (cm)");
        m_chart->setAnnotations({}, {}, {}, points);
        m_chart->setSeries({});
        m_chart->setLegend({}, "f(λ) of one element · dispersion splits the foci");
    } else {
        const auto curve = TL::imageCurve(in);
        ChartSeries line;
        line.line = true;
        double xMax = 0, yMin = 1e9, yMax = -1e9;
        for (const auto& [dObj, dImg] : curve) {
            line.points << QPointF(dObj, dImg);
            xMax = std::max(xMax, dObj);
            yMin = std::min(yMin, dImg);
            yMax = std::max(yMax, dImg);
        }
        if (xMax <= 0) xMax = 1;
        if (yMin > yMax) { yMin = 0; yMax = 1; }
        const auto res = TL::analyse(in);
        const bool haveSample = res.valid && !res.atInfinity && std::isfinite(res.imageFromH2);
        if (haveSample) { yMin = std::min(yMin, res.imageFromH2); yMax = std::max(yMax, res.imageFromH2); }
        const double pad = std::max(0.5, (yMax - yMin) * 0.08);
        m_chart->setAxes(0, xMax, 4, 0, "dₒ from V₁ (cm)", std::min(0.0, yMin - pad), yMax + pad, 3, 1, "dᵢ (cm)");
        m_chart->setSeries({line});
        m_chart->setLegend({}, QString("dᵢ measured from H₂ · %1").arg(QString::fromStdString(TL::analyse(in).element)));
        if (haveSample)
            m_chart->setAnnotations({}, {}, {},
                                    {{in.objectDistance, res.imageFromH2,
                                      QString("Current: (%1, %2) cm").arg(in.objectDistance, 0, 'f', 1).arg(res.imageFromH2, 0, 'f', 3), t.sage, true}});
    }

    m_ctx->publish({"Thick Lens & Aberration", r.parameters, r.registers, r.headline, ER::thickLensSweep(in), {}});
    if (record) {
        m_ctx->recordRun("Thick Lens & Aberration",
                         QString("%1 · R₁ %2 · R₂ %3 · t %4").arg(elementName(), m_r1->text(), m_r2->text(), m_thickness->text()),
                         q(r.headline), true);
        refreshMetas(false);
    }
}

} // namespace phy104
