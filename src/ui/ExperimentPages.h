#pragma once
#include "BasePage.h"
#include "ExperimentReports.h"

#include <QList>
#include <vector>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace phy104 {

class LcdRegisterView;
class LensCanvas;
class LineChart;
class MirrorCanvas;
class ModeTabs;
class NoticeCard;
class Panel;
class PrismCanvas;
class RefractionCanvas;
class TirCanvas;
class UnitField;
class WaveCanvas;

/// Shared behaviour of the four experiment screens: bound input fields, inline errors, the
/// "paused" pattern (Calculate disabled, Restore button, last valid result retained), panel meta text.
class ExperimentPage : public BasePage {
    Q_OBJECT
public:
    /// The experiment name the session uses ("Thin-Film Interference", "Refraction & TIR", ...).
    QString experimentName() const { return m_experiment; }
    /// The live diagram canvas and chart, captured into the printable report.
    QWidget* diagramCanvas() const;
    QWidget* chartWidget() const;

protected:
    ExperimentPage(const QString& title, const QString& subtitle, const QString& experiment, AppContext* context, QWidget* parent);

    struct Binding { QString id; UnitField* field; double lastValid = 0; bool hasValid = false; };
    void bind(const QString& id, UnitField* field);
    bool readFields();                          // parses all bound fields; marks bad ones; false on failure
    void captureValid();                        // remembers the current field values as "last valid"
    void applyIssues(const Report& report);     // inline errors, buttons, status bar, engine light
    void showRegisters(LcdRegisterView* first, LcdRegisterView* second, const Report& report);
    ReportContext reportContext() const;
    QString lastRunLabel() const;               // "RUN 021" for this experiment, or ""
    void setPanels(Panel* diagram, Panel* chart, Panel* results, const QString& diagramMeta, const QString& chartMeta);
    void refreshMetas(bool paused);
    void buildButtons(QVBoxLayout* fields, const QString& hint, bool sendToNotes = false);   // Calculate [+ Send to Notes] / Reset / Restore + hint
    void sendToNotes();                                            // appends the last valid result to the session notes
    void setNormalHint(const QString& text);
    virtual void apply(bool record) = 0;
    virtual void reset() = 0;

    QString m_experiment;
    std::vector<Binding> m_bindings;
    Report m_last;
    bool m_haveLast = false;
    QPushButton *m_calc = nullptr, *m_reset = nullptr, *m_restore = nullptr;
    QLabel* m_hint = nullptr;

private:
    Binding* find(const QString& id);
    Panel *m_diagramPanel = nullptr, *m_chartPanel = nullptr, *m_resultsPanel = nullptr;
    QString m_diagramMeta, m_chartMeta, m_hintText;
};

/// Experiment 01: Snell's law (two cases) and the single-case total internal reflection screen.
class RefractionPage : public ExperimentPage {
    Q_OBJECT
public:
    explicit RefractionPage(AppContext* context, QWidget* parent = nullptr);
protected:
    void apply(bool record) override;
    void reset() override;
private:
    void onMediumChosen(QComboBox* combo, UnitField* field);
    void onIndexEdited(QComboBox* combo, UnitField* field);
    QString mediumName(QComboBox* combo) const;
    void updateModeTexts();

    ModeTabs* m_mode;
    QComboBox *m_med1, *m_med2;
    UnitField *m_n1, *m_n2, *m_theta;
    UnitField *m_sigmaN1, *m_sigmaN2, *m_sigmaTheta;
    QStackedWidget* m_canvasStack;
    RefractionCanvas* m_canvas;
    TirCanvas* m_tirCanvas;
    LineChart* m_chart;
    Panel *m_diagram, *m_chartPanel, *m_results;
    LcdRegisterView *m_reg1, *m_reg2;
    NoticeCard* m_notice;
    bool m_lastModeTir = false;
};

/// Experiment 02: thin lens (two diagrams) and spherical mirror (one detailed diagram + chart).
class LensesPage : public ExperimentPage {
    Q_OBJECT
public:
    explicit LensesPage(AppContext* context, QWidget* parent = nullptr);
protected:
    void apply(bool record) override;
    void reset() override;
private:
    void updateModeTexts();
    void onTypeChosen();
    void onFocalEdited();
    void onRadiusEdited();

    ModeTabs* m_mode;
    QComboBox* m_type;
    QLabel* m_typeLabel;
    QWidget* m_radiusBlock;
    UnitField *m_f, *m_r, *m_s, *m_h;
    UnitField *m_sigmaF, *m_sigmaS, *m_sigmaH;
    QStackedWidget* m_centerStack;
    LensCanvas* m_lens;
    MirrorCanvas *m_mirrorSmall, *m_mirrorBig;
    LineChart* m_chart;
    Panel *m_lensPanel, *m_mirrorSmallPanel, *m_mirrorBigPanel, *m_chartPanel, *m_results;
    LcdRegisterView *m_reg1, *m_reg2;
    NoticeCard* m_notice;
    QLabel* m_caption;
    bool m_syncing = false;
};

/// Experiment 03: refraction through a prism, minimum deviation, dispersion.
class PrismsPage : public ExperimentPage {
    Q_OBJECT
public:
    explicit PrismsPage(AppContext* context, QWidget* parent = nullptr);
protected:
    void apply(bool record) override;
    void reset() override;
private:
    double indexOf(QComboBox* combo, double wavelengthNm) const;

    ModeTabs* m_mode;
    QComboBox *m_material, *m_surround;
    QLabel *m_materialLabel, *m_surroundLabel;
    UnitField *m_apex, *m_incidence, *m_lambda;
    UnitField *m_sigmaNPrism, *m_sigmaNMedium, *m_sigmaApex, *m_sigmaIncidence, *m_sigmaLambda;
    PrismCanvas* m_canvas;
    LineChart* m_chart;
    Panel *m_diagram, *m_chartPanel, *m_results;
    LcdRegisterView* m_reg;
    NoticeCard *m_noticeInline, *m_noticeExit;
    QLabel* m_footnote;
};

/// Experiment 04: Young's double slit.
class WavePage : public ExperimentPage {
    Q_OBJECT
public:
    explicit WavePage(AppContext* context, QWidget* parent = nullptr);
protected:
    void apply(bool record) override;
    void reset() override;
private:
    void onPresetChosen();
    void onWavelengthEdited();

    QComboBox* m_source;
    UnitField *m_lambda, *m_d, *m_L, *m_order;
    UnitField *m_sigmaLambda, *m_sigmaD, *m_sigmaL;
    ModeTabs* m_view;
    WaveCanvas* m_canvas;
    LineChart* m_chart;
    Panel *m_diagram, *m_chartPanel, *m_results;
    LcdRegisterView *m_reg1, *m_reg2;
    NoticeCard* m_notice;
};

} // namespace phy104
