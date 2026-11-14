#include "DiagramsPage.h"
#include "LatexRenderer.h"
#include "SvgDiagramView.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace phy104 {
namespace {

struct DiagramInfo {
    const char* title;
    const char* resource;
    const char* caption;
    QStringList equations;      // LaTeX
};

const QList<DiagramInfo>& diagrams() {
    static const QList<DiagramInfo> list = {
        {"Reflection & Refraction", ":/diagrams/refraction.svg",
         "The incident, reflected and refracted rays all lie in one plane with the normal. The reflected angle equals the "
         "incident angle; the refracted ray bends towards the normal when light enters a denser medium.",
         {"n=\\frac{c}{v}", "n_1\\sin\\theta_1=n_2\\sin\\theta_2", "\\theta_r=\\theta_1"}},
        {"Total Internal Reflection", ":/diagrams/tir.svg",
         "Going from glass to air, the refracted ray bends away from the normal. At the critical angle it travels along the "
         "surface; beyond it, all the light is reflected.",
         {"\\sin\\theta_c=\\frac{n_2}{n_1}", "n_1>n_2"}},
        {"Prism & Deviation", ":/diagrams/prism.svg",
         "The ray bends towards the base on entering and again on leaving. The angle between the original and the emergent "
         "direction is the deviation \u03b4.",
         {"\\delta=\\theta_1+\\theta_2-A", "\\sin\\left(\\frac{A+\\delta_{min}}{2}\\right)=n\\sin\\left(\\frac{A}{2}\\right)"}},
        {"Thin Lens", ":/diagrams/thin_lens.svg",
         "Three principal rays locate the image: one parallel to the axis (through F'), one through the lens centre "
         "(undeviated) and one through F (leaves parallel). Here the object is beyond 2F.",
         {"\\frac{1}{s}+\\frac{1}{s'}=\\frac{1}{f}", "m=-\\frac{s'}{s}", "P=\\frac{1}{f}"}},
        {"Spherical Mirror", ":/diagrams/spherical_mirror.svg",
         "For a concave mirror the focal point lies halfway between the vertex V and the centre of curvature C. An object "
         "beyond C gives a real, inverted, reduced image between C and F.",
         {"\\frac{1}{s}+\\frac{1}{s'}=\\frac{1}{f}", "f=\\frac{R}{2}", "m=-\\frac{s'}{s}"}},
        {"Young Double Slit", ":/diagrams/double_slit.svg",
         "Light from the two slits overlaps on the screen. Bright fringes appear where the path difference \u0394r = d sin \u03b8 "
         "is a whole number of wavelengths; the pattern is equally spaced.",
         {"d\\sin\\theta=m\\lambda", "\\Delta y=\\frac{\\lambda R}{d}", "y_m=\\frac{m\\lambda R}{d}"}},
    };
    return list;
}

} // namespace

DiagramsPage::DiagramsPage(QWidget* parent) : QWidget(parent) {
    auto* title = new QLabel("Optical Setups");
    title->setObjectName("pageTitle");

    m_list = new QListWidget;
    m_list->setObjectName("diagramList");
    m_list->setFixedWidth(230);
    for (const auto& d : diagrams()) m_list->addItem(d.title);

    m_view = new SvgDiagramView;
    m_title = new QLabel;
    m_title->setObjectName("sectionTitle");
    m_caption = new QLabel;
    m_caption->setWordWrap(true);
    m_equations = new QWidget;
    new QHBoxLayout(m_equations);

    auto* exportBtn = new QPushButton("Export PNG…");
    connect(exportBtn, &QPushButton::clicked, this, &DiagramsPage::exportPng);

    auto* right = new QVBoxLayout;
    right->addWidget(m_view, 1);
    right->addWidget(m_title);
    right->addWidget(m_caption);
    right->addWidget(m_equations);
    right->addWidget(exportBtn, 0, Qt::AlignRight);

    auto* body = new QHBoxLayout;
    body->addWidget(m_list);
    body->addLayout(right, 1);

    auto* root = new QVBoxLayout(this);
    root->addWidget(title);
    root->addLayout(body, 1);

    connect(m_list, &QListWidget::currentRowChanged, this, &DiagramsPage::showDiagram);
    m_list->setCurrentRow(0);
}

void DiagramsPage::showDiagram(int row) {
    if (row < 0 || row >= diagrams().size()) return;
    const DiagramInfo& d = diagrams()[row];
    m_view->load(d.resource);
    m_title->setText(d.title);
    m_caption->setText(d.caption);

    auto* layout = qobject_cast<QHBoxLayout*>(m_equations->layout());
    while (QLayoutItem* item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const QString& tex : d.equations) {
        auto* w = new LatexWidget;
        w->setLatex(tex);
        w->setPointSize(17);
        w->setLcdStyle(true);                 // formulas on a Casio LCD
        layout->addWidget(w);
    }
    layout->addStretch();
}

void DiagramsPage::exportPng() {
    const QString name = m_title->text().toLower().replace(" ", "_").replace("&", "and");
    const QString file = QFileDialog::getSaveFileName(this, "Export diagram", name + ".png", "PNG image (*.png)");
    if (file.isEmpty()) return;
    if (!m_view->exportPng(file, 2400))
        QMessageBox::warning(this, "Export", "Could not write " + file);
}

} // namespace phy104
