#pragma once
#include <QColor>
#include <QList>
#include <QPointF>
#include <QWidget>

namespace phy104 {

/// Dark "optical bench" background with a faint grid; subclasses draw the diagram.
class GridCanvas : public QWidget {
    Q_OBJECT
public:
    explicit GridCanvas(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
    virtual void paintDiagram(class QPainter& painter, const QRectF& area) = 0;
};

struct ChartSeries {
    QList<QPointF> points;
    bool line = false;
    bool markers = false;
    QColor color;                 // invalid colour = theme amber for lines, sage for markers
    QList<double> xUncertainties{};  // per-point independent 1σ error bars
    QList<double> yUncertainties{};
};

/// Overlays of the design: dotted vertical marker, horizontal asymptote, shaded region, highlighted sample.
struct ChartVLine { double x = 0; QString label; QColor color; bool labelAtTop = true; };
struct ChartHLine { double y = 0; QString label; QColor color; };
struct ChartRegion { double x0 = 0, x1 = 0; QColor color; QString label; };
struct ChartPoint {
    double x = 0, y = 0;
    QString label;
    QColor color;
    bool crosshair = false;
    double xUncertainty = 0, yUncertainty = 0;
};

/// Simple line / scatter chart (ticks, grid, legend) drawn with QPainter.
class LineChart : public QWidget {
    Q_OBJECT
public:
    explicit LineChart(QWidget* parent = nullptr);

    void setAxes(double xMin, double xMax, int xTicks, int xDecimals, const QString& xLabel,
                 double yMin, double yMax, int yTicks, int yDecimals, const QString& yLabel);
    void setSeries(const QList<ChartSeries>& series) { m_series = series; update(); }
    /// legend items: marker text; `caption` is drawn after them ("Fit: slope 1.5000  R² = 1.0000")
    void setLegend(const QStringList& items, const QString& caption);
    void setMarkerX(double x, bool enabled);     // optional vertical dotted marker (e.g. minimum deviation)
    void setAnnotations(const QList<ChartVLine>& vlines, const QList<ChartHLine>& hlines,
                        const QList<ChartRegion>& regions, const QList<ChartPoint>& points);
    void clearAnnotations() { setAnnotations({}, {}, {}, {}); }

protected:
    void paintEvent(QPaintEvent*) override;
    QSize sizeHint() const override { return QSize(460, 190); }

private:
    double m_xMin = 0, m_xMax = 1, m_yMin = 0, m_yMax = 1;
    int m_xTicks = 4, m_yTicks = 2, m_xDec = 2, m_yDec = 1;
    QString m_xLabel, m_yLabel, m_caption;
    QStringList m_legend;
    QList<ChartSeries> m_series;
    bool m_hasMarker = false;
    double m_markerX = 0;
    QList<ChartVLine> m_vlines;
    QList<ChartHLine> m_hlines;
    QList<ChartRegion> m_regions;
    QList<ChartPoint> m_points;
};

} // namespace phy104
