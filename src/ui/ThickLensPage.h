#pragma once
#include "ExperimentPages.h"

namespace phy104 {

/// Experiment 11: a real glass element - two radii, a finite centre thickness, a finite aperture.
/// Tabs: "Thick lens" (power, principal planes, imaging, spherical aberration) and "Spectrum"
/// (the same element in red / green / blue through Cauchy's law).
class ThickLensPage : public ExperimentPage {
    Q_OBJECT
public:
    explicit ThickLensPage(AppContext* ctx, QWidget* parent = nullptr);

    /// "Biconvex (converging)" for the current radii - used by the interaction test.
    QString elementName() const;

protected:
    void apply(bool record) override;
    void reset() override;

private:
    void onTypeChosen();
    void updateFigures();

    UnitField *m_n, *m_nMedium, *m_r1, *m_r2, *m_thickness, *m_diameter, *m_object;
    QComboBox* m_type = nullptr;
    QLabel* m_typeLabel = nullptr;
    ModeTabs* m_tabs = nullptr;
    class ThickLensCanvas* m_canvas = nullptr;
    LineChart* m_chart = nullptr;
    Panel *m_diagram = nullptr, *m_chartPanel = nullptr, *m_results = nullptr;
    NoticeCard* m_notice = nullptr;
    LcdRegisterView *m_reg1 = nullptr, *m_reg2 = nullptr;
    QLabel* m_caption = nullptr;
};

} // namespace phy104
