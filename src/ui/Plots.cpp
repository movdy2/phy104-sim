#include "Plots.h"
#include "ThemeManager.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace phy104 {

GridCanvas::GridCanvas(QWidget* parent) : QWidget(parent) {
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void GridCanvas::paintEvent(QPaintEvent*) {
    const Theme& t = ThemeManager::instance().theme();
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.fillRect(rect(), t.canvas);
    p.setPen(QPen(t.grid, 1));
    for (int x = 0; x < width(); x += 36) p.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += 36) p.drawLine(0, y, width(), y);
    p.save();
    paintDiagram(p, QRectF(rect()));
    p.restore();
}

// ---------------------------------------------------------------------------------- LineChart
LineChart::LineChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(170);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void LineChart::setAxes(double xMin, double xMax, int xTicks, int xDecimals, const QString& xLabel,
                        double yMin, double yMax, int yTicks, int yDecimals, const QString& yLabel) {
    m_xMin = xMin; m_xMax = xMax; m_xTicks = xTicks; m_xDec = xDecimals; m_xLabel = xLabel;
    m_yMin = yMin; m_yMax = yMax; m_yTicks = yTicks; m_yDec = yDecimals; m_yLabel = yLabel;
    update();
}

void LineChart::setLegend(const QStringList& items, const QString& caption) {
    m_legend = items;
    m_caption = caption;
    update();
}

void LineChart::setMarkerX(double x, bool enabled) {
    m_markerX = x;
    m_hasMarker = enabled;
    update();
}

void LineChart::setAnnotations(const QList<ChartVLine>& v, const QList<ChartHLine>& hl, const QList<ChartRegion>& r, const QList<ChartPoint>& p) {
    m_vlines = v; m_hlines = hl; m_regions = r; m_points = p;
    update();
}

void LineChart::paintEvent(QPaintEvent*) {
    const Theme& t = ThemeManager::instance().theme();
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.fillRect(rect(), t.canvas);

    QFont mono(ThemeManager::monoFont());
    mono.setPixelSize(9);
    p.setFont(mono);
    const QFontMetricsF fm(mono);

    const QRectF plot(46, 24, width() - 46 - 18, height() - 24 - 44);
    auto X = [&](double x) { return plot.left() + (x - m_xMin) / (m_xMax - m_xMin) * plot.width(); };
    auto Y = [&](double y) { return plot.bottom() - (y - m_yMin) / (m_yMax - m_yMin) * plot.height(); };

    p.setPen(t.dim);
    p.drawText(QPointF(8, 14), m_yLabel);
    p.drawText(QPointF(plot.right() - fm.horizontalAdvance(m_xLabel), plot.bottom() + 36), m_xLabel);

    for (const ChartRegion& r : m_regions) {                              // shaded regions first (under the grid)
        QColor fill = r.color.isValid() ? r.color : t.amber;
        fill.setAlpha(34);
        p.fillRect(QRectF(X(r.x0), plot.top(), X(r.x1) - X(r.x0), plot.height()), fill);
        if (!r.label.isEmpty()) {
            p.setPen(r.color.isValid() ? r.color : t.amber);
            p.drawText(QPointF(X(r.x0) + 8, plot.top() + 28), r.label);
        }
    }
    p.setPen(QPen(t.border, 1));
    for (int i = 0; i <= m_xTicks; ++i) {                                 // vertical grid + x tick labels
        const double v = m_xMin + (m_xMax - m_xMin) * i / m_xTicks;
        p.setPen(QPen(t.grid.lighter(115), 1));
        p.drawLine(QPointF(X(v), plot.top()), QPointF(X(v), plot.bottom()));
        p.setPen(t.dim);
        const QString s = QString::number(v, 'f', m_xDec);
        p.drawText(QPointF(X(v) - fm.horizontalAdvance(s) / 2, plot.bottom() + 14), s);
    }
    for (int i = 0; i <= m_yTicks; ++i) {                                 // horizontal grid + y tick labels
        const double v = m_yMin + (m_yMax - m_yMin) * i / m_yTicks;
        p.setPen(QPen(t.grid.lighter(115), 1));
        p.drawLine(QPointF(plot.left(), Y(v)), QPointF(plot.right(), Y(v)));
        p.setPen(t.dim);
        const QString s = QString::number(v, 'f', m_yDec);
        p.drawText(QPointF(plot.left() - 6 - fm.horizontalAdvance(s), Y(v) + 3), s);
    }
    p.setPen(QPen(t.border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(plot);

    p.save();
    p.setClipRect(plot.adjusted(-2, -2, 2, 2));
    if (m_hasMarker) {
        p.setPen(QPen(t.dim, 1, Qt::DotLine));
        p.drawLine(QPointF(X(m_markerX), plot.top()), QPointF(X(m_markerX), plot.bottom()));
    }
    for (const ChartHLine& h : m_hlines) {
        p.setPen(QPen(h.color.isValid() ? h.color : t.dim, 1, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), Y(h.y)), QPointF(plot.right(), Y(h.y)));
        p.setPen(t.muted);
        p.drawText(QPointF(plot.right() - 8 - fm.horizontalAdvance(h.label), Y(h.y) - 6), h.label);
    }
    for (const ChartVLine& v : m_vlines) {
        p.setPen(QPen(v.color.isValid() ? v.color : t.text, 1.4, Qt::DashLine));
        p.drawLine(QPointF(X(v.x), plot.top()), QPointF(X(v.x), plot.bottom()));
    }
    for (const ChartSeries& s : m_series) {
        if (s.points.isEmpty()) continue;
        if (s.line) {
            QPolygonF poly;
            for (const QPointF& pt : s.points) poly << QPointF(X(pt.x()), Y(pt.y()));
            p.setPen(QPen(s.color.isValid() ? s.color : t.amber, 1.8));
            p.drawPolyline(poly);
        }
        if (!s.xUncertainties.isEmpty() || !s.yUncertainties.isEmpty()) {
            QColor errorColor = s.color.isValid() ? s.color : (s.markers ? t.sage : t.amber);
            errorColor.setAlpha(210);
            p.setPen(QPen(errorColor, 1.0));
            p.setBrush(Qt::NoBrush);
            for (qsizetype i = 0; i < s.points.size(); ++i) {
                const QPointF& point = s.points[i];
                const double xSigma = i < s.xUncertainties.size() ? s.xUncertainties[i] : 0.0;
                const double ySigma = i < s.yUncertainties.size() ? s.yUncertainties[i] : 0.0;
                if (std::isfinite(xSigma) && xSigma > 0.0) {
                    const double left = X(point.x() - xSigma), right = X(point.x() + xSigma), y = Y(point.y());
                    p.drawLine(QPointF(left, y), QPointF(right, y));
                    p.drawLine(QPointF(left, y - 3), QPointF(left, y + 3));
                    p.drawLine(QPointF(right, y - 3), QPointF(right, y + 3));
                }
                if (std::isfinite(ySigma) && ySigma > 0.0) {
                    const double top = Y(point.y() + ySigma), bottom = Y(point.y() - ySigma), x = X(point.x());
                    p.drawLine(QPointF(x, top), QPointF(x, bottom));
                    p.drawLine(QPointF(x - 3, top), QPointF(x + 3, top));
                    p.drawLine(QPointF(x - 3, bottom), QPointF(x + 3, bottom));
                }
            }
        }
        if (s.markers) {
            p.setPen(Qt::NoPen);
            p.setBrush(s.color.isValid() ? s.color : t.sage);
            for (const QPointF& pt : s.points) p.drawEllipse(QPointF(X(pt.x()), Y(pt.y())), 2.6, 2.6);
        }
    }
    p.restore();

    for (const ChartVLine& v : m_vlines) {                                 // labels are drawn outside the clip
        if (v.label.isEmpty()) continue;
        p.setPen(v.color.isValid() ? v.color : t.text);
        const double lw = fm.horizontalAdvance(v.label);
        p.drawText(QPointF(std::min(plot.right() - lw, std::max(plot.left(), X(v.x) - lw / 2)), v.labelAtTop ? plot.top() - 6 : plot.bottom() - 18), v.label);
    }
    for (const ChartPoint& pt : m_points) {
        const QColor col = pt.color.isValid() ? pt.color : t.sage;
        if (pt.xUncertainty > 0.0 || pt.yUncertainty > 0.0) {
            p.save();
            p.setClipRect(plot.adjusted(-2, -2, 2, 2));
            p.setPen(QPen(col, 1.1));
            if (std::isfinite(pt.xUncertainty) && pt.xUncertainty > 0.0) {
                const double left = X(pt.x - pt.xUncertainty), right = X(pt.x + pt.xUncertainty), y = Y(pt.y);
                p.drawLine(QPointF(left, y), QPointF(right, y));
                p.drawLine(QPointF(left, y - 3), QPointF(left, y + 3));
                p.drawLine(QPointF(right, y - 3), QPointF(right, y + 3));
            }
            if (std::isfinite(pt.yUncertainty) && pt.yUncertainty > 0.0) {
                const double top = Y(pt.y + pt.yUncertainty), bottom = Y(pt.y - pt.yUncertainty), x = X(pt.x);
                p.drawLine(QPointF(x, top), QPointF(x, bottom));
                p.drawLine(QPointF(x - 3, top), QPointF(x + 3, top));
                p.drawLine(QPointF(x - 3, bottom), QPointF(x + 3, bottom));
            }
            p.restore();
        }
        if (pt.crosshair) {
            p.setPen(QPen(t.dim, 1, Qt::DashLine));
            p.drawLine(QPointF(X(pt.x), Y(pt.y)), QPointF(X(pt.x), plot.bottom()));
            p.drawLine(QPointF(plot.left(), Y(pt.y)), QPointF(X(pt.x), Y(pt.y)));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(QPointF(X(pt.x), Y(pt.y)), 4, 4);
        if (!pt.label.isEmpty()) {
            p.setPen(t.text);
            const double lw = fm.horizontalAdvance(pt.label);
            p.drawText(QPointF(std::min(plot.right() - lw - 4, X(pt.x) + 10), Y(pt.y) - 8), pt.label);
        }
    }

    // legend
    double x = plot.left();
    const double y = height() - 8;
    for (const QString& item : m_legend) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.sage);
        p.drawEllipse(QPointF(x + 3, y - 3), 3, 3);
        p.setPen(t.muted);
        p.drawText(QPointF(x + 12, y), item);
        x += 18 + fm.horizontalAdvance(item) + 14;
    }
    p.setPen(t.muted);
    p.drawText(QPointF(x, y), m_caption);
}

} // namespace phy104
