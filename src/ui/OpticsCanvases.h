#pragma once
#include "ExperimentReports.h"
#include "WaveExperiments.h"
#include "Plots.h"

#include <QColor>
#include <QList>
#include <QString>

namespace phy104 {

/// Two incidence cases on one bench: A refraction and B total internal reflection.
class RefractionCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit RefractionCanvas(QWidget* parent = nullptr);
    void setInput(const RefractionInput& in, const QString& medium1, const QString& medium2);
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    RefractionInput m_in;
    QString m_m1 = "Glass", m_m2 = "Air";
};

/// The single-case screen of the TIR mode: one large ray diagram with all annotations.
class TirCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit TirCanvas(QWidget* parent = nullptr);
    void setInput(const RefractionInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    RefractionInput m_in;
};

/// Thin lens with the three principal rays.
class LensCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit LensCanvas(QWidget* parent = nullptr);
    void setInput(const ImagingInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    ImagingInput m_in;
};

/// Spherical mirror (concave / convex) with two principal rays.
class MirrorCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit MirrorCanvas(QWidget* parent = nullptr);
    void setInput(const ImagingInput& in) { m_in = in; update(); }
    /// Detailed mode (mirror screen): real / virtual space labels, dimension lines, object and image labels.
    void setDetailed(bool detailed) { m_detailed = detailed; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    ImagingInput m_in;
    bool m_detailed = false;
};

/// One wavelength through the prism; one path per visible spectral colour is drawn in dispersion mode.
struct PrismPath {
    QColor color;
    double wavelengthNm = 589.3;
    double nPrism = 1.5;
    double nMedium = 1.0;
};

class PrismCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit PrismCanvas(QWidget* parent = nullptr);
    void setInput(const PrismInput& in, const QList<PrismPath>& extraPaths = {});
    /// Current wavelength-resolved paths, empty when Single ray mode is active.
    const QList<PrismPath>& spectrumPaths() const { return m_extra; }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    PrismInput m_in;
    QList<PrismPath> m_extra;
};

/// Schematic double slit: plane wave, slits, wavefronts, screen with bright fringes.
class WaveCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit WaveCanvas(QWidget* parent = nullptr);
    void setInput(const SlitInput& in, bool wavefronts);
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    SlitInput m_in;
    bool m_wavefronts = false;
};

// ---- the six diagrams of the second Figma batch (each is a function of the same inputs as its report) ----
class NewtonCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit NewtonCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const NewtonInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    NewtonInput m_in;
};
class MichelsonCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit MichelsonCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const MichelsonInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    MichelsonInput m_in;
};
class ThinFilmCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit ThinFilmCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const ThinFilmInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    ThinFilmInput m_in;
};
class GratingCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit GratingCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const GratingInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    GratingInput m_in;
};
class SingleSlitCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit SingleSlitCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const SingleSlitInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    SingleSlitInput m_in;
};
class PolarizationCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit PolarizationCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const PolarizationInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    PolarizationInput m_in;
};

/// Thick lens: the real glass shape with both vertices, the principal planes H1/H2, the focal points and
/// the marginal-ray spherical aberration (the paraxial focus and the marginal crossing are both marked).
class ThickLensCanvas : public GridCanvas {
    Q_OBJECT
public:
    explicit ThickLensCanvas(QWidget* p = nullptr) : GridCanvas(p) { setMinimumSize(300, 330); }
    void setInput(const ThickLensInput& in) { m_in = in; update(); }
protected:
    void paintDiagram(QPainter& p, const QRectF& area) override;
private:
    ThickLensInput m_in;
};

/// Small amber line drawing for the dashboard cards.
class PreviewIcon : public QWidget {
    Q_OBJECT
public:
    enum Kind { Refraction, Lens, Prism, Waves };
    explicit PreviewIcon(Kind kind, QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return QSize(130, 132); }
private:
    Kind m_kind;
};

} // namespace phy104
