#pragma once
#include "ExperimentPages.h"
#include "WaveExperiments.h"

namespace phy104 {

/// Shared body of the six screens of the second Figma batch (header with Save / Export, tabs + fields on the
/// left, diagram + chart in the middle, two LCD registers + Interpretation / Input guard on the right).
/// A subclass builds its fields in the constructor and implements compute() / updateCanvas() / resetFields().
class ModelPage : public ExperimentPage {
    Q_OBJECT
protected:
    ModelPage(const QString& title, const QString& subtitle, const QString& experiment, AppContext* context, QWidget* parent);

    UnitField* addField(const QString& id, const QString& label, const QString& unit, double value, int decimals, bool signedValue = false, bool spinner = false, double step = 1);
    /// Dropdown whose items carry a number (item data).
    QComboBox* addCombo(const QString& label, const QStringList& items, const QList<double>& values);
    ModeTabs* addTabs(const QStringList& tabs);
    /// Builds the three columns; call once, after the fields were added.
    void build(const QString& diagramTitle, QWidget* canvas, const QString& chartTitle, const QString& hint);
    /// Re-captions the diagram and chart panels (the second tabs show a different picture).
    void retitle(const QString& diagramTitle, const QString& chartTitle);

    virtual ModelReport compute(const ReportContext& ctx) = 0;
    virtual void updateCanvas() = 0;
    virtual void resetFields() = 0;
    virtual QString runDescription() const = 0;

    void apply(bool record) override;
    void reset() override;

    QVBoxLayout* m_fieldLayout = nullptr;
    ModeTabs* m_tabs = nullptr;

private:
    void showChart(const ChartData& c);
    Panel *m_controls, *m_diagram, *m_chartPanel;
    LineChart* m_chart;
    LcdRegisterView *m_reg1, *m_reg2;
    NoticeCard *m_interpretation, *m_guard;
    QString m_diagramTitle;
    QList<QWidget*> m_pending;                 // controls added before build()
};

class NewtonRingsPage : public ModelPage {
    Q_OBJECT
public:
    explicit NewtonRingsPage(AppContext* ctx, QWidget* parent = nullptr);
protected:
    ModelReport compute(const ReportContext& ctx) override;
    void updateCanvas() override;
    void resetFields() override;
    QString runDescription() const override;
private:
    UnitField *m_lambda, *m_radius, *m_order;
    QComboBox *m_medium, *m_viewing;
    class NewtonCanvas* m_canvas;
};

class MichelsonPage : public ModelPage {
    Q_OBJECT
public:
    explicit MichelsonPage(AppContext* ctx, QWidget* parent = nullptr);
protected:
    ModelReport compute(const ReportContext& ctx) override;
    void updateCanvas() override;
    void resetFields() override;
    QString runDescription() const override;
private:
    UnitField *m_lambda, *m_dx;
    QComboBox *m_medium, *m_reference, *m_balance;
    class MichelsonCanvas* m_canvas;
};

class ThinFilmPage : public ModelPage {
    Q_OBJECT
public:
    explicit ThinFilmPage(AppContext* ctx, QWidget* parent = nullptr);
    /// The swatch of the "Colours" tab ("#rrggbb"), empty on the other tabs.
    QString colourSwatch();
protected:
    ModelReport compute(const ReportContext& ctx) override;
    void updateCanvas() override;
    void resetFields() override;
    QString runDescription() const override;
private:
    UnitField *m_lambda, *m_n, *m_t;
    QLabel* m_swatch = nullptr;
    class ThinFilmCanvas* m_canvas;
};

class GratingPage : public ModelPage {
    Q_OBJECT
public:
    explicit GratingPage(AppContext* ctx, QWidget* parent = nullptr);
protected:
    ModelReport compute(const ReportContext& ctx) override;
    void updateCanvas() override;
    void resetFields() override;
    QString runDescription() const override;
private:
    UnitField *m_density, *m_lambda, *m_order, *m_slits;
    QComboBox* m_incidence;
    class GratingCanvas* m_canvas;
};

class SingleSlitPage : public ModelPage {
    Q_OBJECT
public:
    explicit SingleSlitPage(AppContext* ctx, QWidget* parent = nullptr);
protected:
    ModelReport compute(const ReportContext& ctx) override;
    void updateCanvas() override;
    void resetFields() override;
    QString runDescription() const override;
private:
    UnitField *m_width, *m_lambda, *m_screen, *m_separation;
    QComboBox *m_wave, *m_scale;
    class SingleSlitCanvas* m_canvas;
};

class PolarizationPage : public ModelPage {
    Q_OBJECT
public:
    explicit PolarizationPage(AppContext* ctx, QWidget* parent = nullptr);
protected:
    ModelReport compute(const ReportContext& ctx) override;
    void updateCanvas() override;
    void resetFields() override;
    QString runDescription() const override;
private:
    UnitField *m_intensity, *m_angle, *m_n1, *m_n2;
    QComboBox* m_beam;
    class PolarizationCanvas* m_canvas;
};

} // namespace phy104
