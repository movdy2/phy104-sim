#include "OpticsCanvases.h"
#include "ThickLens.h"
#include "PhysicsCalculator.h"
#include "ThemeManager.h"

#include <QFontMetricsF>
#include <QImage>
#include <functional>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace phy104 {
namespace PC = PhysicsCalculator;
namespace TL = ThickLens;
namespace {

constexpr double kDeg = std::numbers::pi / 180.0;
const Theme& th() { return ThemeManager::instance().theme(); }

QFont monoFont(int px, bool bold = false) {
    QFont f(ThemeManager::monoFont());
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

/// Text anchored at `pos`: Qt::AlignLeft / AlignRight / AlignHCenter, vertically centred.
void text(QPainter& p, QPointF pos, const QString& s, const QColor& c, int px = 10, Qt::Alignment a = Qt::AlignLeft, bool bold = false) {
    const QFont f = monoFont(px, bold);
    const QFontMetricsF fm(f);
    const double w = fm.horizontalAdvance(s);
    double x = pos.x();
    if (a & Qt::AlignRight) x -= w; else if (a & Qt::AlignHCenter) x -= w / 2;
    p.setFont(f);
    p.setPen(c);
    p.drawText(QPointF(x, pos.y() + fm.ascent() / 2 - 1), s);
}

QPointF unit(QPointF v) {
    const double n = std::hypot(v.x(), v.y());
    return n < 1e-9 ? QPointF(1, 0) : v / n;
}

void head(QPainter& p, QPointF tip, QPointF dir, const QColor& c, double size = 7) {
    dir = unit(dir);
    const QPointF base = tip - dir * size, nrm(-dir.y(), dir.x());
    QPolygonF poly;
    poly << tip << base + nrm * size * 0.45 << base - nrm * size * 0.45;
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(poly);
}

/// Line a->b with a small arrow head in the middle (rays) when `arrowAt` >= 0.
void ray(QPainter& p, QPointF a, QPointF b, const QColor& c, double w = 1.6, double arrowAt = 0.5, Qt::PenStyle style = Qt::SolidLine) {
    p.setPen(QPen(c, w, style, Qt::RoundCap));
    p.drawLine(a, b);
    if (arrowAt >= 0 && style == Qt::SolidLine) head(p, a + (b - a) * arrowAt + unit(b - a) * 4, b - a, c, 7);
}

double angleOf(QPointF d) { return std::atan2(-d.y(), d.x()) / kDeg; }

/// Arc around `center` from direction u to direction v (screen coordinates).
void arcDirs(QPainter& p, QPointF center, QPointF u, QPointF v, double r, const QColor& c, double w = 1.3) {
    double a0 = angleOf(u), span = angleOf(v) - a0;
    while (span > 180) span -= 360;
    while (span < -180) span += 360;
    p.setPen(QPen(c, w));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(center.x() - r, center.y() - r, 2 * r, 2 * r), int(a0 * 16), int(span * 16));
}

void legendItem(QPainter& p, QPointF pos, const QString& s, const QColor& c, bool dashed = false) {
    p.setPen(QPen(c, 2, dashed ? Qt::DotLine : Qt::SolidLine));
    p.drawLine(pos + QPointF(0, 0), pos + QPointF(8, 0));
    text(p, pos + QPointF(12, 0), s, th().muted, 10);
}

/// Extends the ray a->b until it leaves the canvas on the right (or left).
QPointF extendTo(QPointF a, QPointF b, double xLimit) {
    const QPointF d = b - a;
    if (std::fabs(d.x()) < 1e-9) return b;
    const double t = (xLimit - a.x()) / d.x();
    return t > 1 ? a + d * t : b;
}

QString describeImage(const PC::ImageResult& img) {
    const double m = std::fabs(img.magnification);
    return QString(img.real ? "Real" : "Virtual") + " · " + (img.upright ? "upright" : "inverted") + " · " +
           (m > 1.0001 ? "enlarged" : (m < 0.9999 ? "diminished" : "same size"));
}

QString num(double v, int dp = 4) { return QString::number(v, 'f', dp); }

} // namespace

// ============================================================================ Refraction & TIR
RefractionCanvas::RefractionCanvas(QWidget* parent) : GridCanvas(parent) { setMinimumSize(300, 330); }

void RefractionCanvas::setInput(const RefractionInput& in, const QString& m1, const QString& m2) {
    m_in = in; m_m1 = m1; m_m2 = m2;
    update();
}

void RefractionCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double thetaA = m_in.tirMode ? 30.0 : m_in.theta1;
    const double thetaB = m_in.tirMode ? m_in.theta1 : ExperimentReports::defaultTirAngle(m_in.n1, m_in.n2);

    auto drawCase = [&](const QRectF& r, const QString& title, double theta, bool labelRefracted) {
        const auto res = PC::refraction(m_in.n1, m_in.n2, theta);
        const QPointF O(r.center().x() + 10, r.top() + r.height() * 0.52);
        const double L = std::min(82.0, r.height() * 0.46);
        const double t1 = theta * kDeg, t2 = res.refractedDeg * kDeg;

        text(p, QPointF(14, r.top() + 16), title, t.amber, 10);
        p.setPen(QPen(t.sage, 1.6));
        p.drawLine(QPointF(20, O.y()), QPointF(a.right() - 20, O.y()));                  // interface
        p.setPen(QPen(t.dim, 1, Qt::DashLine));
        p.drawLine(QPointF(O.x(), O.y() - L - 8), QPointF(O.x(), O.y() + L + 8));        // normal
        text(p, QPointF(20, O.y() - 12), m_m1 + " · n₁ " + num(m_in.n1), t.text, 9);
        text(p, QPointF(20, O.y() + 14), m_m2 + " · n₂ " + num(m_in.n2), t.text, 9);

        const QPointF inc(-std::sin(t1), -std::cos(t1)), refl(std::sin(t1), -std::cos(t1));
        ray(p, O + inc * L, O, t.amber, 1.8);
        ray(p, O, O + refl * L, t.text, 1.5);
        arcDirs(p, O, QPointF(0, -1), inc, 26, t.text);
        text(p, O + unit(QPointF(0, -1) + inc) * 40, QString::number(theta, 'g', 4) + "°", t.text, 9, Qt::AlignHCenter);
        if (res.totalInternalReflection) {
            arcDirs(p, O, QPointF(0, -1), refl, 26, t.text);
            text(p, O + unit(QPointF(0, -1) + refl) * 40, QString::number(theta, 'g', 4) + "°", t.text, 9, Qt::AlignHCenter);
            text(p, O + QPointF(40, 30), "No transmitted ray", t.muted, 10);
        } else {
            const QPointF rd(std::sin(t2), std::cos(t2));
            ray(p, O, O + rd * L, t.amber, 1.8);
            arcDirs(p, O, QPointF(0, 1), rd, 30, t.text);
            text(p, O + unit(QPointF(0, 1) + rd) * 48, num(res.refractedDeg, 2) + "°", t.text, 9, Qt::AlignHCenter);
            if (labelRefracted) text(p, O + rd * L + QPointF(6, 4), "Refracted ray", t.text, 9);
        }
    };

    drawCase(QRectF(a.left(), a.top(), a.width(), a.height() * 0.47), "A / REFRACTION", thetaA, true);
    drawCase(QRectF(a.left(), a.top() + a.height() * 0.47, a.width(), a.height() * 0.45), "B / TOTAL INTERNAL REFLECTION", thetaB, false);

    const double ly = a.bottom() - 14;
    legendItem(p, QPointF(14, ly), "Incident / refracted", t.amber);
    legendItem(p, QPointF(14 + 150, ly), "Reflected", t.text);
    legendItem(p, QPointF(14 + 250, ly), "Normal", t.dim, true);
}

// ============================================================================ TIR (single case)
TirCanvas::TirCanvas(QWidget* parent) : GridCanvas(parent) { setMinimumSize(300, 330); }

void TirCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const auto res = PC::refraction(m_in.n1, m_in.n2, m_in.theta1);
    const auto crit = PC::criticalAngleDeg(m_in.n1, m_in.n2);
    const QPointF O(a.center().x() - 6, a.top() + a.height() * 0.58);
    const double L = std::min(a.height() * 0.50, a.width() * 0.34);
    const double t1 = m_in.theta1 * kDeg;

    text(p, QPointF(14, a.top() + 18), res.totalInternalReflection ? "TOTAL INTERNAL REFLECTION" : "REFRACTION AND REFLECTION", t.amber, 10);
    text(p, QPointF(18, a.top() + 44), QString::fromStdString(m_in.name1) + " · n₁ = " + num(m_in.n1), t.muted, 10);
    text(p, QPointF(18, O.y() + 30), QString::fromStdString(m_in.name2) + " · n₂ = " + num(m_in.n2), t.muted, 10);

    p.setPen(QPen(t.dim, 1, Qt::DashLine));                                         // normal
    p.drawLine(QPointF(O.x(), a.top() + 28), QPointF(O.x(), a.bottom() - 40));
    text(p, QPointF(O.x() + 12, a.top() + 46), "Normal", t.dim, 10);
    p.setPen(QPen(t.sage, 1.6));                                                    // interface
    p.drawLine(QPointF(20, O.y()), QPointF(a.right() - 20, O.y()));
    text(p, QPointF(a.right() - 24, O.y() + 14), "Interface", t.sage, 10, Qt::AlignRight);

    const QPointF inc(-std::sin(t1), -std::cos(t1)), refl(std::sin(t1), -std::cos(t1));
    ray(p, O + inc * L, O, t.amber, 2.0);
    ray(p, O, O + refl * L, t.text, 1.8);
    text(p, O + inc * L * 0.62 + QPointF(-70, 22), "Incident ray", t.amber, 10);
    text(p, O + refl * L * 0.62 + QPointF(26, 6), "Reflected ray", t.text, 10);
    arcDirs(p, O, QPointF(0, -1), inc, 56, t.text);
    arcDirs(p, O, QPointF(0, -1), refl, 56, t.text);
    text(p, O + unit(QPointF(0, -1) + inc) * 78, QString::number(m_in.theta1, 'g', 5) + "°", t.text, 10, Qt::AlignHCenter);
    text(p, O + unit(QPointF(0, -1) + refl) * 78, QString::number(m_in.theta1, 'g', 5) + "°", t.text, 10, Qt::AlignHCenter);

    if (res.totalInternalReflection) {
        text(p, O + QPointF(24, 74), "No propagating transmitted ray", t.text, 10);
    } else {
        const double t2 = res.refractedDeg * kDeg;
        const QPointF rd(std::sin(t2), std::cos(t2));
        ray(p, O, O + rd * L * 0.8, t.amber, 2.0);
        text(p, O + rd * L * 0.8 + QPointF(8, 8), "Refracted ray", t.amber, 10);
    }
    if (crit)
        text(p, QPointF(18, a.bottom() - 46), "θ₁ " + QString(m_in.theta1 > *crit ? ">" : "≤") + " θc = " + num(*crit), t.amber, 10);
    else
        text(p, QPointF(18, a.bottom() - 46), "n₁ ≤ n₂: no critical angle", t.amber, 10);
    const double ly = a.bottom() - 16;
    legendItem(p, QPointF(14, ly), "Incident", t.amber);
    legendItem(p, QPointF(14 + 90, ly), "Reflected", t.text);
    legendItem(p, QPointF(14 + 190, ly), "Normal", t.dim, true);
    text(p, QPointF(14 + 270, ly), "/   angles from normal", t.muted, 10);
}

// ============================================================================ thin lens
LensCanvas::LensCanvas(QWidget* parent) : GridCanvas(parent) { setMinimumSize(300, 280); }

void LensCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double f = m_in.f, af = std::fabs(f);
    const bool diverging = f < 0;
    const auto img = PC::imageFormation(m_in.s, f);
    const double axisY = a.top() + a.height() * 0.50, lx = a.center().x();
    const double extent = std::max({m_in.s, 2 * af, img.atInfinity ? 0.0 : std::fabs(img.imageDistance)});
    const double sc = (a.width() / 2 - 44) / std::max(extent, 1e-6);
    const double H = std::min(a.height() * 0.36, 110.0);

    p.setPen(QPen(t.dim, 1));
    p.drawLine(QPointF(24, axisY), QPointF(a.right() - 24, axisY));
    for (double k : {-2.0, -1.0, 1.0, 2.0}) {                                  // 2F, F, F', 2F'
        const double x = lx + k * af * sc;
        p.setPen(QPen(t.dim, 1));
        p.drawLine(QPointF(x, axisY - 4), QPointF(x, axisY + 4));
        text(p, QPointF(x, axisY + 18), std::fabs(k) == 2 ? "2F" : "F", t.muted, 9, Qt::AlignHCenter);
    }
    QPainterPath lens;                                                         // lens outline
    if (!diverging) {
        lens.moveTo(lx, axisY - H); lens.quadTo(lx + 16, axisY, lx, axisY + H); lens.quadTo(lx - 16, axisY, lx, axisY - H);
    } else {
        lens.moveTo(lx - 10, axisY - H); lens.quadTo(lx + 3, axisY, lx - 10, axisY + H);
        lens.lineTo(lx + 10, axisY + H); lens.quadTo(lx - 3, axisY, lx + 10, axisY - H); lens.closeSubpath();
    }
    p.setPen(QPen(t.sage, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(lens);

    const double hp = std::min(m_in.h * 18.0, H * 0.8);
    const QPointF base(lx - m_in.s * sc, axisY), tip(base.x(), axisY - hp);
    p.setPen(QPen(t.amber, 2));
    p.drawLine(base, tip);
    head(p, tip, QPointF(0, -1), t.amber, 8);

    if (img.atInfinity) {
        text(p, QPointF(a.center().x(), a.top() + 22), "Image at infinity (object in the focal plane)", t.error, 10, Qt::AlignHCenter);
    } else {
        const QPointF T(lx + img.imageDistance * sc, axisY - img.magnification * hp);
        const QPointF hits[3] = {QPointF(lx, tip.y()), QPointF(lx, axisY), QPointF(lx, T.y())};
        for (const QPointF& h : hits) {
            ray(p, tip, h, t.amber, 1.3);
            if (img.real) {
                ray(p, h, extendTo(h, T, a.right() - 10), t.amber, 1.3);
            } else {
                const QPointF away = h + unit(h - T) * 1000;
                ray(p, h, extendTo(h, away, a.right() - 10), t.amber, 1.3);
                ray(p, h, T, t.muted, 1, -1, Qt::DashLine);                    // virtual extension
            }
        }
        p.setPen(QPen(img.real ? t.text : t.muted, 2, img.real ? Qt::SolidLine : Qt::DashLine));
        p.drawLine(QPointF(T.x(), axisY), T);
        head(p, T, QPointF(0, T.y() < axisY ? -1 : 1), t.text, 8);
        text(p, QPointF(T.x(), T.y() + (T.y() < axisY ? -12 : 16)), "Image", t.text, 10, Qt::AlignHCenter);
        text(p, QPointF(T.x() + 8, a.top() + a.height() * 0.84), "dᵢ = " + QString::number(std::fabs(img.imageDistance), 'g', 6) + " cm", t.amber, 10);
        text(p, QPointF(12, a.bottom() - 14), describeImage(img) + "     m = " + num(img.magnification), t.text, 10);
    }
    text(p, QPointF(base.x() + 4, a.top() + a.height() * 0.84), "dₒ = " + QString::number(m_in.s, 'g', 6) + " cm", t.text, 10);
}

// ============================================================================ spherical mirror
MirrorCanvas::MirrorCanvas(QWidget* parent) : GridCanvas(parent) { setMinimumSize(300, 280); }

void MirrorCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double f = m_in.f, af = std::fabs(f);                      // concave: +f, convex: -f
    const auto img = PC::imageFormation(m_in.s, f);
    const bool concave = f > 0;
    const double axisY = a.top() + a.height() * (m_detailed ? 0.50 : 0.46);

    // horizontal layout: everything must fit, including the space BEHIND a convex mirror
    const double di = img.atInfinity ? 0.0 : img.imageDistance;
    const double leftExt = std::max({m_in.s, concave ? 2 * af : 0.0, di > 0 ? di : 0.0});
    const double rightExt = concave ? 0.05 * leftExt : std::max(2 * af, std::fabs(di));
    const double sc = (a.width() - 100) / std::max(leftExt + rightExt, 1e-6);
    const double vx = 44 + leftExt * sc;                             // pole of the mirror
    const double R = 2 * af * sc;
    const double cx = concave ? vx - R : vx + R;                      // centre of curvature

    text(p, QPointF(14, a.top() + 16), QString(concave ? "CONCAVE" : "CONVEX") + " / " +
         (m_detailed ? "PARAXIAL PRINCIPAL RAYS" : (img.atInfinity ? "NO IMAGE" : (img.real ? "REAL IMAGE" : "VIRTUAL IMAGE"))), t.amber, 10);
    if (m_detailed) {
        text(p, QPointF(18, a.top() + 42), "In front · real space", t.muted, 10);
        text(p, QPointF(a.right() - 16, a.top() + 42), "Behind · virtual space", t.muted, 10, Qt::AlignRight);
    }
    p.setPen(QPen(t.dim, 1));
    p.drawLine(QPointF(24, axisY), QPointF(a.right() - 24, axisY));

    p.setPen(QPen(t.sage, 2.2));                                    // mirror arc around the pole
    p.setBrush(Qt::NoBrush);
    const double halfAngle = std::min(36.0, std::asin(std::min(1.0, a.height() * 0.40 / std::max(R, 1.0))) / kDeg);
    p.drawArc(QRectF(cx - R, axisY - R, 2 * R, 2 * R), int((concave ? -halfAngle : 180 - halfAngle) * 16), int(2 * halfAngle * 16));
    if (m_detailed) {                                                // hatching on the back of the mirror
        p.setPen(QPen(t.sage, 1));
        for (int k = -4; k <= 4; ++k) {
            const double ang = k * halfAngle / 4.5 * kDeg;
            const QPointF onArc(cx + (concave ? 1 : -1) * R * std::cos(ang), axisY + R * std::sin(ang));
            p.drawLine(onArc, onArc + QPointF(concave ? 9 : -9, 7));
        }
    }

    const double fx = concave ? vx - af * sc : vx + af * sc;
    for (auto [x, s] : {std::pair{cx, "C"}, std::pair{fx, "F"}, std::pair{vx, "P"}}) {
        p.setPen(QPen(t.dim, 1));
        p.drawLine(QPointF(x, axisY - 4), QPointF(x, axisY + 4));
        text(p, QPointF(x, axisY + 20), s, t.muted, 10, Qt::AlignHCenter);
    }
    const double hp = std::min(m_in.h * 18.0, a.height() * 0.28);
    const QPointF base(vx - m_in.s * sc, axisY), tip(base.x(), axisY - hp);
    p.setPen(QPen(t.amber, 2));
    p.drawLine(base, tip);
    head(p, tip, QPointF(0, -1), t.amber, 8);
    if (m_detailed) text(p, tip + QPointF(-4, -14), "Object " + QString(m_in.h > 0 ? "+" : "") + num(m_in.h, 2) + " cm", t.text, 10);

    if (!img.atInfinity) {
        const double ix = vx - di * sc;
        const QPointF T(ix, axisY - img.magnification * hp);
        const double dy = tip.y() - axisY;
        const double hitX = cx + (concave ? 1 : -1) * std::sqrt(std::max(0.0, R * R - dy * dy));
        const QPointF hits[2] = {QPointF(hitX, tip.y()), QPointF(vx, axisY)};
        for (const QPointF& h : hits) {
            ray(p, tip, h, t.amber, 1.4);
            if (img.real) ray(p, h, extendTo(h, T, 14), t.amber, 1.4);
            else { ray(p, h, extendTo(h, h + unit(h - T) * 1000, 14), t.amber, 1.4); ray(p, h, T, t.muted, 1, -1, Qt::DashLine); }
        }
        p.setPen(QPen(img.real ? t.text : t.sage, 2, img.real ? Qt::SolidLine : Qt::SolidLine));
        p.drawLine(QPointF(T.x(), axisY), T);
        head(p, T, QPointF(0, T.y() < axisY ? -1 : 1), img.real ? t.text : t.sage, 8);
        if (m_detailed) {
            text(p, T + QPointF(-4, -26), img.real ? "Real image" : "Virtual image", t.sage, 10, Qt::AlignHCenter);
            text(p, T + QPointF(-4, -12), QString(img.magnification * m_in.h >= 0 ? "+" : "") + num(img.magnification * m_in.h, 4) + " cm", t.sage, 10, Qt::AlignHCenter);
            // dimension lines under the axis
            const double y1 = axisY + hp + 36, y2 = y1 + 36;
            auto dim = [&](double x0, double x1, double y, const QString& label, const QColor& c) {
                p.setPen(QPen(c, 1));
                p.drawLine(QPointF(x0, y), QPointF(x1, y));
                p.drawLine(QPointF(x0, y - 5), QPointF(x0, y + 5));
                p.drawLine(QPointF(x1, y - 5), QPointF(x1, y + 5));
                text(p, QPointF((x0 + x1) / 2, y + 16), label, c, 10, Qt::AlignHCenter);
            };
            dim(std::min(base.x(), vx), std::max(base.x(), vx), y1, "dₒ = " + QString(m_in.s > 0 ? "+" : "") + num(m_in.s, 2) + " cm", t.muted);
            dim(std::min(ix, vx), std::max(ix, vx), y2, "dᵢ = " + num(di, 4) + " cm", t.sage);
        } else {
            text(p, QPointF(T.x(), T.y() + (T.y() < axisY ? -12 : 16)), "Image", t.text, 10, Qt::AlignHCenter);
            text(p, QPointF(a.left() + 14, a.bottom() - 34), "dₒ = " + QString::number(m_in.s, 'g', 6) + " cm  ·  dᵢ = " +
                 QString::number(std::fabs(di), 'g', 6) + " cm  ·  m = " + QString::number(img.magnification, 'g', 5), t.text, 10);
        }
    } else {
        text(p, QPointF(a.center().x(), a.top() + 60), "Image at infinity (object at F)", t.error, 10, Qt::AlignHCenter);
    }
    if (m_detailed) {
        const double ratio = 18.0 / std::max(sc, 1e-6);
        const double ly = a.bottom() - 14;
        legendItem(p, QPointF(14, ly), "Physical rays", t.amber);
        legendItem(p, QPointF(14 + 130, ly), "Backward extensions", t.muted, true);
        if (ratio > 1.5) text(p, QPointF(14 + 300, ly), QString("/   vertical scale ×%1").arg(qRound(ratio)), t.muted, 10);
    } else {
        text(p, QPointF(a.left() + 14, a.bottom() - 14), "Reflecting surface faces the object", t.muted, 10);
    }
}

// ============================================================================ prism
PrismCanvas::PrismCanvas(QWidget* parent) : GridCanvas(parent) { setMinimumSize(300, 330); }

void PrismCanvas::setInput(const PrismInput& in, const QList<PrismPath>& extra) {
    m_in = in; m_extra = extra;
    update();
}

namespace {
std::optional<QPointF> refractVec(QPointF d, QPointF n, double eta) {   // n points against d
    const double cosi = -(n.x() * d.x() + n.y() * d.y());
    const double k = 1 - eta * eta * (1 - cosi * cosi);
    if (k < 0) return std::nullopt;
    return d * eta + n * (eta * cosi - std::sqrt(k));
}
std::optional<QPointF> lineHit(QPointF p, QPointF d, QPointF q, QPointF e) {
    const double det = d.x() * (-e.y()) - d.y() * (-e.x());
    if (std::fabs(det) < 1e-9) return std::nullopt;
    const double s = ((q.x() - p.x()) * (-e.y()) - (q.y() - p.y()) * (-e.x())) / det;
    return p + d * s;
}
}

void PrismCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double A = m_in.apex * kDeg, half = A / 2;
    const double H = std::min(a.height() * 0.60, a.width() * 0.40 / std::max(0.2, std::tan(half)));
    const QPointF apex(a.center().x(), a.top() + 40);
    const QPointF left(apex.x() - H * std::tan(half), apex.y() + H), right(apex.x() + H * std::tan(half), apex.y() + H);

    p.setPen(QPen(t.sage, 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawPolygon(QPolygonF({apex, left, right}));

    const QPointF nL(-std::cos(half), -std::sin(half)), nR(std::cos(half), -std::sin(half));   // outward normals
    const QPointF P1 = apex + (left - apex) * 0.55;
    const QPointF faceR = unit(right - apex);

    struct Trace { QPointF d0, d1, P2, d2, X; bool ok = false, tir = false, hasX = false; };
    auto trace = [&](double np) {
        Trace tr;
        const double nrel = np / m_in.nMedium;
        const double ang = half - m_in.incidence * kDeg;
        tr.d0 = QPointF(std::cos(ang), std::sin(ang));
        const auto d1 = refractVec(tr.d0, nL, 1.0 / nrel);
        if (!d1) return tr;
        tr.d1 = *d1;
        const auto P2 = lineHit(P1, tr.d1, apex, faceR);
        if (!P2) return tr;
        tr.P2 = *P2;
        const double u = QPointF::dotProduct(tr.P2 - apex, faceR) / std::hypot(right.x() - apex.x(), right.y() - apex.y());
        if (u < 0 || u > 1.0001) return tr;
        tr.ok = true;
        const auto d2 = refractVec(tr.d1, -nR, nrel);
        if (!d2) { tr.tir = true; return tr; }
        tr.d2 = *d2;
        if (const auto X = lineHit(P1, tr.d0, tr.P2, tr.d2)) { tr.X = *X; tr.hasX = true; }
        return tr;
    };

    auto drawPath = [&](const Trace& tr, const QColor& c, bool annotate) {
        const double sx = (P1.x() - 22) / std::max(0.05, tr.d0.x());
        ray(p, P1 - tr.d0 * sx, P1, c, 1.9);
        if (!tr.ok) return;
        ray(p, P1, tr.P2, c, 1.9);
        if (tr.tir) {
            if (annotate) text(p, tr.P2 + QPointF(10, -8), "TIR at the exit face: no emergent ray", t.error, 10);
            return;
        }
        const double ex = (a.right() - 22 - tr.P2.x()) / std::max(0.05, tr.d2.x());
        ray(p, tr.P2, tr.P2 + tr.d2 * std::max(30.0, ex), c, 1.9);
    };

    const Trace main = trace(m_in.nPrism);
    // normals
    p.setPen(QPen(t.dim, 1, Qt::DashLine));
    p.drawLine(P1 - nL * 40, P1 + nL * 60);
    if (main.ok) p.drawLine(main.P2 - nR * 40, main.P2 + nR * 60);

    if (m_extra.isEmpty()) drawPath(main, t.amber, true);
    else for (const PrismPath& e : m_extra) drawPath(trace(e.nPrism), e.color, false);

    const auto res = PC::prism(m_in.nPrism, m_in.nMedium, m_in.apex, m_in.incidence);
    // construction lines and angle labels (main ray)
    if (main.ok && !main.tir && main.hasX) {
        p.setPen(QPen(t.dim, 1, Qt::DashLine));
        p.drawLine(P1, main.X);
        p.drawLine(main.X, main.P2);
        arcDirs(p, main.X, main.d0, main.d2, 24, t.text);
        text(p, main.P2 + QPointF(26, 44), "δ = " + num(res.deviation) + "°", t.amber, 10);
    }
    arcDirs(p, P1, nL, -main.d0, 24, t.text);
    text(p, P1 + QPointF(-6, -22), "i₁ = " + num(m_in.incidence) + "°", t.text, 9, Qt::AlignRight);
    if (main.ok && !main.tir) {
        arcDirs(p, main.P2, nR, main.d2, 24, t.text);
        text(p, main.P2 + QPointF(6, -22), "i₂ = " + num(res.i2) + "°", t.text, 9);
        const bool sym = std::fabs(res.r1 - res.r2) < 0.005;
        text(p, (P1 + main.P2) / 2 + QPointF(0, 18), sym ? "r₁ = r₂ = " + QString::number(res.r1, 'g', 6) + "°"
                                                         : "r₁ = " + num(res.r1, 2) + "°  r₂ = " + num(res.r2, 2) + "°", t.text, 9, Qt::AlignHCenter);
    }
    arcDirs(p, apex, unit(left - apex), unit(right - apex), 26, t.text);
    text(p, apex + QPointF(0, 44), "A = " + QString::number(m_in.apex, 'g', 5) + "°", t.text, 9, Qt::AlignHCenter);
    text(p, QPointF(apex.x(), left.y() - 22), "n = " + num(m_in.nPrism / m_in.nMedium), t.text, 9, Qt::AlignHCenter);
    text(p, QPointF(16, P1.y() + 70), "Incident ray", t.amber, 10);

    text(p, QPointF(16, a.bottom() - 36), "δ = i₁ + i₂ - A", t.text, 10);
    const double ly = a.bottom() - 14;
    legendItem(p, QPointF(16, ly), "Optical path", t.amber);
    legendItem(p, QPointF(16 + 110, ly), "Glass boundary", t.sage);
    legendItem(p, QPointF(16 + 235, ly), "Normals", t.dim, true);
}

// ============================================================================ double slit
WaveCanvas::WaveCanvas(QWidget* parent) : GridCanvas(parent) { setMinimumSize(300, 330); }

void WaveCanvas::setInput(const SlitInput& in, bool wavefronts) {
    m_in = in; m_wavefronts = wavefronts;
    update();
}

void WaveCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double cy = a.top() + a.height() * 0.50, bx = a.left() + 106, sx = a.right() - 76;
    const QPointF S1(bx, cy - 22), S2(bx, cy + 22);
    QColor wave = t.lime; wave.setAlpha(m_wavefronts ? 200 : 80);

    text(p, QPointF(14, a.top() + 18), "Plane wave", t.text, 10);
    for (int k = -2; k <= 2; ++k) ray(p, QPointF(a.left() + 14, cy + k * 32), QPointF(bx - 12, cy + k * 32), t.amber, 1.8, -1);

    p.save();
    p.setClipRect(QRectF(bx, a.top(), sx - bx, a.height()));
    p.setBrush(Qt::NoBrush);
    for (const QPointF& s : {S1, S2})
        for (double r = 14; r < sx - bx + 30; r += 19) {
            p.setPen(QPen(wave, 1.3));
            p.drawEllipse(s, r, r);
        }
    p.restore();

    p.setPen(QPen(t.sage, 1.8));
    p.drawLine(QPointF(bx, a.top() + 26), QPointF(bx, S1.y() - 5));
    p.drawLine(QPointF(bx, S1.y() + 5), QPointF(bx, S2.y() - 5));
    p.drawLine(QPointF(bx, S2.y() + 5), QPointF(bx, a.bottom() - 26));
    p.drawLine(QPointF(sx, a.top() + 30), QPointF(sx, a.bottom() - 30));
    p.setPen(Qt::NoPen);
    p.setBrush(t.amber);
    p.drawEllipse(S1, 3.5, 3.5);
    p.drawEllipse(S2, 3.5, 3.5);
    text(p, S1 + QPointF(8, -4), "S₁", t.text, 9);
    text(p, S2 + QPointF(8, 12), "S₂", t.text, 9);
    text(p, QPointF(sx - 24, a.top() + 18), "Screen", t.text, 10);

    const double pitch = 20.0;
    if (!m_wavefronts) {
        p.setPen(Qt::NoPen);
        p.setBrush(t.lime);
        for (int k = -6; k <= 6; ++k) p.drawRoundedRect(QRectF(sx + 8, cy + k * pitch - 3, 24, 6), 3, 3);
    }
    const double ym = cy - m_in.order * pitch;
    for (const QPointF& s : {S1, S2}) ray(p, s, QPointF(sx, ym), t.text, 1, -1, Qt::DashLine);
    for (const QPointF& s : {S1, S2}) ray(p, s, QPointF(sx, cy), t.text, 1, -1, Qt::DashLine);
    text(p, QPointF(sx - 10, ym - 8), "m = +" + QString::number(m_in.order), t.amber, 9, Qt::AlignRight);
    text(p, QPointF(sx - 10, cy + 12), "m = 0", t.amber, 9, Qt::AlignRight);

    text(p, QPointF(a.left() + 14, cy + 105), "d = " + QString::number(m_in.separationMm, 'g', 4) + " mm", t.amber, 10);
    text(p, QPointF((bx + sx) / 2, cy + 128), "L = " + QString::number(m_in.screenM, 'f', 3) + " m", t.text, 10, Qt::AlignHCenter);
    text(p, QPointF(a.left() + 14, a.bottom() - 14), "Constructive: Δ = mλ     Destructive: Δ = (m + ½)λ", t.muted, 9);
}


// ============================================================================ the six diagrams of the second Figma batch
namespace {
QColor mix(const QColor& a, const QColor& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    // QColor's floating-point constructor takes floats; narrow explicitly after interpolation.
    const auto blend = [t](double first, double second) {
        return static_cast<float>(first + (second - first) * t);
    };
    return QColor::fromRgbF(blend(a.redF(), b.redF()), blend(a.greenF(), b.greenF()), blend(a.blueF(), b.blueF()));
}
/// A square image whose pixel value is intensity(x, y) in [0,1] blended between the canvas colour and lime.
QImage fringeImage(int size, const std::function<double(double, double)>& intensity) {
    const Theme& t = th();
    QImage img(size, size, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    const double r0 = size / 2.0;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const double dx = x - r0 + 0.5, dy = y - r0 + 0.5;
            if (dx * dx + dy * dy > r0 * r0) continue;
            img.setPixelColor(x, y, mix(t.canvas, t.lime, intensity(dx, dy)));
        }
    return img;
}
} // namespace

void NewtonCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const int m = static_cast<int>(std::lround(std::max(0.0, m_in.order)));
    const bool transmitted = m_in.transmitted;
    const double order = transmitted ? m + 0.5 : double(m);                  // transmitted dark rings sit at half orders
    const double lam = m_in.lambdaNm * 1e-9, r = std::sqrt(order * lam * m_in.radiusM / m_in.n);
    text(p, QPointF(14, a.top() + 18), "PLANO-CONVEX LENS · R = " + num(m_in.radiusM, 4) + " m", t.amber, 10);

    const double cx = a.center().x(), hw = a.width() / 2 - 34, y0 = a.top() + 52;
    const QRectF plate(a.left() + 30, a.top() + a.height() * 0.40, a.width() - 60, 14);
    const double yb = plate.top() - 22;
    QPainterPath lens;
    lens.moveTo(cx - hw, y0); lens.lineTo(cx + hw, y0); lens.lineTo(cx + hw, yb);
    lens.quadTo(cx, 2 * plate.top() - yb, cx - hw, yb);
    lens.closeSubpath();
    QColor glass = t.blue; glass.setAlpha(40);
    p.setPen(QPen(t.sage, 1.5)); p.setBrush(glass); p.drawPath(lens);
    p.setBrush(Qt::NoBrush); p.drawRect(plate);
    text(p, QPointF(cx - hw + 18, y0 + 22), "Glass lens", t.muted, 10);
    text(p, QPointF(cx - hw + 18, plate.top() - 6), "Air film · n = " + num(m_in.n, 4), t.muted, 10);
    text(p, QPointF(cx, plate.bottom() + 16), "Contact · t = 0", t.muted, 10, Qt::AlignHCenter);
    const double gx = cx + hw * 0.5, k = (plate.top() - yb) / (hw * hw);                  // gap at x: parabola of the curved face
    const double gap = k * (gx - cx) * (gx - cx);
    p.setPen(QPen(t.text, 2)); p.drawLine(QPointF(gx, plate.top() - gap), QPointF(gx, plate.top()));
    text(p, QPointF(gx + 6, plate.top() - gap / 2), "t(r)", t.muted, 10);

    // ring pattern: m(r) = (r/r₁)², I = sin²(π m) reflected (dark centre) / cos²(π m) transmitted (bright centre)
    const int size = static_cast<int>(std::min(150.0, a.height() * 0.40));
    const double rs = 0.82 * (size / 2.0) / std::sqrt(std::max(1.0, order));
    const QImage img = fringeImage(size, [rs, transmitted](double dx, double dy) {
        const double mm = (dx * dx + dy * dy) / (rs * rs);
        return transmitted ? std::pow(std::cos(std::numbers::pi * mm), 2) : std::pow(std::sin(std::numbers::pi * mm), 2);
    });
    const QPointF c(a.right() - size / 2.0 - 40, a.bottom() - size / 2.0 - 28);
    p.drawImage(QPointF(c.x() - size / 2.0, c.y() - size / 2.0), img);
    p.setPen(QPen(t.amber, 1.2)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, rs * std::sqrt(order), rs * std::sqrt(order));
    text(p, QPointF(14, a.top() + a.height() * 0.62), transmitted ? "TRANSMITTED FIELD" : "REFLECTED FIELD", t.amber, 10);
    const QString sub = m < 10 ? QString(QChar(0x2080 + m)) : QString("(m)");
    text(p, QPointF(14, a.top() + a.height() * 0.62 + 20), transmitted ? "Bright center · first dark at m = ½" : "Dark center: m = 0", t.muted, 10);
    text(p, QPointF(14, a.top() + a.height() * 0.62 + 36), "Selected ring: m = " + QString::number(m), t.muted, 10);
    text(p, QPointF(14, a.top() + a.height() * 0.62 + 52), "r" + sub + " = " + num(r * 1e3, 4) + " mm", t.muted, 10);
    text(p, QPointF(14, a.top() + a.height() * 0.62 + 68), "D" + sub + " = " + num(2 * r * 1e3, 4) + " mm", t.muted, 10);
    text(p, QPointF(14, a.bottom() - 30), "Radius ≠ diameter.", t.muted, 10);
    text(p, QPointF(14, a.bottom() - 14), transmitted ? "Dark rings: 2nt = (m + ½)λ." : "Dark rings: 2nt = mλ.", t.muted, 10);
}

void MichelsonCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const QPointF bs(a.center().x() - 20, a.top() + a.height() * 0.40);
    text(p, QPointF(14, a.top() + 18), "MICHELSON INTERFEROMETER · TOP VIEW", t.amber, 10);
    // source -> beam splitter
    ray(p, QPointF(a.left() + 24, bs.y()), bs - QPointF(14, 0), t.amber, 1.8);
    text(p, QPointF(a.left() + 24, bs.y() - 16), "Source", t.muted, 10);
    p.setPen(QPen(t.sage, 1.6)); p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(bs.x() - 14, bs.y() - 14, 28, 28));
    p.drawLine(QPointF(bs.x() - 14, bs.y() + 14), QPointF(bs.x() + 14, bs.y() - 14));
    text(p, bs + QPointF(0, 28), "BS", t.muted, 10, Qt::AlignHCenter);
    // M1 (fixed, above) and M2 (movable, right)
    const double m1y = bs.y() - 96, m2x = bs.x() + 130 + std::clamp(m_in.displacementUm, -1.0, 1.0) * 18;
    p.setPen(QPen(t.sage, 3)); p.drawLine(QPointF(bs.x() - 22, m1y), QPointF(bs.x() + 22, m1y)); p.drawLine(QPointF(m2x, bs.y() - 22), QPointF(m2x, bs.y() + 22));
    if (m_in.piezo) {                                                  // second tab: M1 rides on a piezo stack
        p.setPen(QPen(t.blue, 2));
        for (int k = 0; k < 3; ++k) p.drawLine(QPointF(bs.x() - 18, m1y - 6 - 4 * k), QPointF(bs.x() + 18, m1y - 6 - 4 * k));
        text(p, QPointF(bs.x() - 24, m1y - 8), "piezo", t.blue, 10, Qt::AlignRight);
    }
    text(p, QPointF(bs.x() + 30, m1y), m_in.piezo ? "M1 · piezo mount" : "M1 fixed", t.muted, 10);
    text(p, QPointF(m2x + 8, bs.y() - 30), "M2 · Δx = " + QString(m_in.displacementUm > 0 ? "+" : "") + num(m_in.displacementUm, 4) + " µm", t.amber, 10);
    p.setPen(QPen(t.dim, 1, Qt::DashLine)); p.drawLine(QPointF(bs.x() + 130, bs.y() + 34), QPointF(m2x, bs.y() + 34));
    ray(p, bs + QPointF(0, -14), QPointF(bs.x(), m1y + 4), t.text, 1.6);
    ray(p, bs + QPointF(14, 0), QPointF(m2x - 4, bs.y()), t.amber, 1.6);
    ray(p, QPointF(bs.x() + 3, m1y + 4), bs + QPointF(3, 14), t.text, 1.3);
    const QPointF det(bs.x(), a.bottom() - 110);
    ray(p, bs + QPointF(0, 14), det, t.amber, 1.8);
    text(p, det + QPointF(14, 0), "Detector", t.muted, 10);
    // fringe disc: I = cos²(Δφ/2 + k r²), Δφ = 4πnΔx/λ
    const double dphi = 4 * std::numbers::pi * m_in.n * m_in.displacementUm * 1e3 / m_in.lambdaNm;
    const double vis = std::clamp(m_in.visibility, 0.0, 1.0);
    const int size = static_cast<int>(std::min(118.0, a.height() * 0.30));
    const QImage img = fringeImage(size, [dphi, size, vis](double dx, double dy) {
        const double r2 = (dx * dx + dy * dy) / double((size / 2) * (size / 2));
        return (1 + vis * std::cos(dphi + 18.0 * r2)) / (1 + vis);      // I = (1 + V cos Δφ_r)/(1 + V)
    });
    const QPointF c(det.x(), a.bottom() - size / 2.0 - 16);
    p.drawImage(QPointF(c.x() - size / 2.0, c.y() - size / 2.0), img);
    text(p, QPointF(a.left() + 14, a.bottom() - 14), "ΔOPD = 2nΔx = " + num(2 * m_in.n * m_in.displacementUm, 4) + " µm", t.muted, 10);
    if (m_in.fringes) {                                                // fringe-count tab: spacing and contrast on the bench
        const double spacing = m_in.lambdaNm / (2 * m_in.n);
        text(p, QPointF(a.left() + 14, a.bottom() - 30), "Fringe spacing λ/(2n) = " + num(spacing, 4) + " nm per fringe", t.muted, 10);
        text(p, QPointF(a.left() + 14, a.bottom() - 44), "Contrast V = " + num(m_in.visibility, 2) + "  ·  sweep ±2 µm", t.muted, 10);
    }
}

void ThinFilmCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double left = a.left() + 40, right = a.right() - 40;
    const double filmTop = a.top() + a.height() * 0.38;
    const double thick = std::clamp(m_in.thicknessNm / 400.0, 0.12, 1.0) * 90;
    const double filmBot = filmTop + thick;
    const bool transmission = m_in.transmission;
    text(p, QPointF(14, a.top() + 18), transmission ? "NORMAL INCIDENCE · TRANSMITTED BEAM" : "NORMAL INCIDENCE · SINGLE FILM", t.amber, 10);
    QColor film = t.blue; film.setAlpha(60);
    QColor sub = t.sage; sub.setAlpha(40);
    p.setPen(Qt::NoPen);
    p.setBrush(film); p.drawRect(QRectF(left, filmTop, right - left, thick));
    p.setBrush(sub); p.drawRect(QRectF(left, filmBot, right - left, a.bottom() - filmBot - 70));
    p.setPen(QPen(t.sage, 1.5)); p.drawLine(QPointF(left, filmTop), QPointF(right, filmTop)); p.drawLine(QPointF(left, filmBot), QPointF(right, filmBot));
    text(p, QPointF(left + 6, filmTop - 12), "Air · n₀ = 1.0000", t.muted, 10);
    text(p, QPointF(left + 6, filmTop + thick / 2), "Film · n = " + num(m_in.n, 4), t.text, 10);
    text(p, QPointF(left + 6, filmBot + 16), "Substrate · n₂ > n", t.muted, 10);
    const double x0 = a.center().x() - 20;
    ray(p, QPointF(x0 - 60, a.top() + 54), QPointF(x0, filmTop), t.amber, 1.8);                 // incident (drawn slightly oblique)
    if (transmission) {
        ray(p, QPointF(x0, filmTop), QPointF(x0 + 20, filmBot), t.amber, 1.3);                  // T1 through the film
        ray(p, QPointF(x0 + 20, filmBot), QPointF(x0 + 34, a.bottom() - 80), t.amber, 1.8);     // T2 down through the substrate
        text(p, QPointF(x0 + 38, a.bottom() - 80), "T · no net reversal", t.amber, 10);
        ray(p, QPointF(x0 + 20, filmBot), QPointF(x0 + 6, filmTop), t.dim, 1.1, -1, Qt::DashLine);   // weak internal bounce
    } else {
        ray(p, QPointF(x0, filmTop), QPointF(x0 + 60, a.top() + 54), t.text, 1.6);              // R1: phase reversal
        text(p, QPointF(x0 + 64, a.top() + 54), "R1 · phase reversal", t.text, 10);
        ray(p, QPointF(x0, filmTop), QPointF(x0 + 14, filmBot), t.amber, 1.3);
        ray(p, QPointF(x0 + 14, filmBot), QPointF(x0 + 28, filmTop), t.amber, 1.3);
        ray(p, QPointF(x0 + 28, filmTop), QPointF(x0 + 88, a.top() + 92), t.amber, 1.6);        // R2
        text(p, QPointF(x0 + 92, a.top() + 92), "R2 · no reversal", t.amber, 10);
    }
    p.setPen(QPen(t.text, 1)); p.drawLine(QPointF(right - 24, filmTop), QPointF(right - 24, filmBot));
    text(p, QPointF(right - 30, filmTop + thick / 2), "t = " + num(m_in.thicknessNm) + " nm", t.text, 10, Qt::AlignRight);
    text(p, QPointF(14, a.bottom() - 46), "OPD = 2nt = " + num(2 * m_in.n * m_in.thicknessNm) + " nm", t.muted, 10);
    if (transmission) {
        const double trans = std::pow(std::cos(std::numbers::pi * 2 * m_in.n * m_in.thicknessNm / m_in.lambdaNm), 2);
        text(p, QPointF(14, a.bottom() - 30), "I_t/I_t,max = cos²(2πnt/λ) = " + num(trans, 4), t.muted, 10);
        text(p, QPointF(14, a.bottom() - 14), "Bright when 2nt = qλ", t.muted, 10);
    } else {
        text(p, QPointF(14, a.bottom() - 30), "Δψ = 4πnt/λ − π", t.muted, 10);
        text(p, QPointF(14, a.bottom() - 14), "Bright when 2nt = (q + ½)λ", t.muted, 10);
    }
}

void GratingCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double d = 0.01 / std::max(1e-9, m_in.linesPerCm), lam = m_in.lambdaNm * 1e-9;
    text(p, QPointF(14, a.top() + 18), "NORMAL INCIDENCE · d = " + num(d * 1e6, 4) + " µm", t.amber, 10);
    if (m_in.orderList)                                                        // second tab: every peak is the subject
        text(p, QPointF(a.right() - 14, a.top() + 18), "ORDERS VIEW · d/λ = " + num(d / lam, 4), t.amber, 10, Qt::AlignRight);
    const QPointF g(a.left() + a.width() * 0.30, a.top() + a.height() * 0.40);
    const double sx = a.right() - 60, len = sx - g.x();
    for (int k = -1; k <= 1; ++k) ray(p, QPointF(a.left() + 24, g.y() + k * 20), QPointF(g.x() - 6, g.y() + k * 20), t.sage, 1.4, -1);
    p.setPen(QPen(t.dim, 5, Qt::DotLine)); p.drawLine(QPointF(g.x(), g.y() - 70), QPointF(g.x(), g.y() + 70));
    p.setPen(QPen(t.sage, 2)); p.drawLine(QPointF(sx, g.y() - 100), QPointF(sx, g.y() + 100));
    const auto orders = WaveExperiments::allowedGratingOrders(m_in.linesPerCm, m_in.lambdaNm, m_in.incidenceDeg);
    for (int m : orders) {
        const double s = m * lam / d + std::sin(m_in.incidenceDeg * std::numbers::pi / 180);
        const double th2 = std::asin(std::clamp(s, -1.0, 1.0));
        const QPointF end(g.x() + len * std::cos(th2) * 0.92, g.y() - len * std::sin(th2) * 0.92);
        const bool selected = m == static_cast<int>(std::lround(m_in.order)) || m_in.orderList;
        p.setPen(QPen(selected ? t.amber : t.sage, selected ? 2.2 : 1.3));
        p.drawLine(g, end);
        text(p, end + QPointF(6, 0), (m > 0 ? "+" : "") + QString::number(m), selected ? t.amber : t.muted, 10);
    }
    text(p, QPointF(a.center().x(), g.y() + 108), "Ray fan schematic · not to scale", t.muted, 10, Qt::AlignHCenter);
    // angular screen: one sharp bar per allowed order
    const double by = a.bottom() - 70, bw = a.width() - 120, bx = a.left() + 60;
    text(p, QPointF(14, by - 26), "ANGULAR SCREEN · SHARP ALLOWED ORDERS", t.amber, 10);
    p.setPen(Qt::NoPen); p.setBrush(t.bg); p.drawRect(QRectF(bx, by - 14, bw, 30));
    for (int m : orders) {
        const double th2 = std::asin(std::clamp(m * lam / d + std::sin(m_in.incidenceDeg * std::numbers::pi / 180), -1.0, 1.0)) * 180 / std::numbers::pi;
        const double x = bx + bw / 2 + th2 / 90.0 * bw / 2;
        const bool selected = m == static_cast<int>(std::lround(m_in.order));
        p.setBrush(selected || m_in.orderList ? t.amber : t.sage); p.drawRect(QRectF(x - 2, by - 12, 4, 26));
        text(p, QPointF(x, by + 28), QString::number(m), t.muted, 9, Qt::AlignHCenter);
    }
    if (!orders.empty()) {
        const double lim = std::asin(std::clamp(orders.back() * lam / d, -1.0, 1.0)) * 180 / std::numbers::pi;
        text(p, QPointF(a.center().x(), a.bottom() - 10), num(-lim, 2) + "° … 0° … +" + num(lim, 2) + "°", t.muted, 10, Qt::AlignHCenter);
    }
}

void SingleSlitCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    const double aa = m_in.widthMm * 1e-3, lam = m_in.lambdaNm * 1e-9;
    const double s1 = std::min(1.0, lam / aa), theta = std::asin(s1);
    text(p, QPointF(14, a.top() + 18), "SINGLE SLIT · a = " + num(m_in.widthMm, 4) + " mm", t.amber, 10);
    const QPointF g(a.left() + a.width() * 0.26, a.top() + a.height() * 0.50);
    const double sx = a.right() - 110, gap = 18;
    for (int k = -2; k <= 2; ++k) ray(p, QPointF(a.left() + 20, g.y() + k * 14), QPointF(g.x() - 8, g.y() + k * 14), t.sage, 1.3, -1);
    p.setPen(QPen(t.text, 4)); p.drawLine(QPointF(g.x(), a.top() + 40), QPointF(g.x(), g.y() - gap / 2)); p.drawLine(QPointF(g.x(), g.y() + gap / 2), QPointF(g.x(), a.bottom() - 40));
    p.setPen(QPen(t.sage, 2)); p.drawLine(QPointF(sx, a.top() + 40), QPointF(sx, a.bottom() - 40));
    const double yEdge = std::min((a.height() - 90) / 2.0, (sx - g.x()) * std::tan(theta));
    QColor lobe = t.amber; lobe.setAlpha(35);
    p.setPen(Qt::NoPen); p.setBrush(lobe);
    p.drawPolygon(QPolygonF({g, QPointF(sx, g.y() - yEdge), QPointF(sx, g.y() + yEdge)}));
    p.setPen(QPen(t.amber, 1.4, Qt::DashLine));
    p.drawLine(g, QPointF(sx, g.y() - yEdge)); p.drawLine(g, QPointF(sx, g.y() + yEdge));
    text(p, QPointF(sx + 8, g.y() - yEdge), "−1st min", t.muted, 9);
    text(p, QPointF(sx + 8, g.y() + yEdge), "+1st min", t.muted, 9);
    // intensity profile to the right of the screen
    QPolygonF profile;
    const double span = std::max(0.2, 4.2 * theta);
    for (int k = -100; k <= 100; ++k) {
        const double ang = span * k / 100.0;
        const double beta = std::numbers::pi * aa * std::sin(ang) / lam;
        const double I = std::fabs(beta) < 1e-9 ? 1.0 : std::pow(std::sin(beta) / beta, 2);
        profile << QPointF(sx + 8 + I * 70, g.y() - (a.height() - 100) / 2 * (k / 100.0));
    }
    p.setPen(QPen(t.text, 1.4)); p.setBrush(Qt::NoBrush); p.drawPolyline(profile);
    if (m_in.geometry) {                                                     // second tab: where each minimum lands
        for (int k = 3; k >= 1; --k) {
            const double yl = std::min((sx - g.x()) * std::tan(std::asin(std::min(1.0, k * lam / aa))), (a.height() - 90) / 2.0);
            p.setPen(QPen(t.dim, 1.2, Qt::DashLine));
            p.drawLine(g, QPointF(sx, g.y() - yl));
            p.drawLine(g, QPointF(sx, g.y() + yl));
            p.drawLine(QPointF(sx - 5, g.y() - yl), QPointF(sx + 5, g.y() - yl));    // tick on the screen
            p.drawLine(QPointF(sx - 5, g.y() + yl), QPointF(sx + 5, g.y() + yl));
        }
        text(p, QPointF(14, a.bottom() - 30),
             "y₁ = ±" + num(m_in.screenM * std::tan(std::asin(std::min(1.0, lam / aa))) * 1e3, 4) + " mm · y₂ = ±"
                 + num(m_in.screenM * std::tan(std::asin(std::min(1.0, 2 * lam / aa))) * 1e3, 4) + " mm", t.muted, 10);
        text(p, QPointF(14, a.bottom() - 14),
             "y₃ = ±" + num(m_in.screenM * std::tan(std::asin(std::min(1.0, 3 * lam / aa))) * 1e3, 4)
                 + " mm · w = " + num(2 * m_in.screenM * std::tan(theta) * 1e3, 5) + " mm (central lobe)", t.muted, 10);
    } else {
        text(p, QPointF(14, a.bottom() - 30), "w = 2L tan θ₁ = " + num(2 * m_in.screenM * std::tan(theta) * 1e3, 5) + " mm", t.muted, 10);
        text(p, QPointF(14, a.bottom() - 14), "Minima: a sin θ = mλ", t.muted, 10);
    }
}

void PolarizationCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    if (m_in.brewster) {
        // Second tab: one interface at θB — incident, reflected (s only) and refracted ray.
        const double thB = std::atan(m_in.n2 / m_in.n1), deg = thB * 180 / std::numbers::pi;
        const double s2 = (m_in.n1 / m_in.n2) * std::sin(thB);
        const double ct = std::sqrt(std::max(0.0, 1 - s2 * s2)), c = std::cos(thB);
        const double as = (m_in.n1 * c - m_in.n2 * ct) / (m_in.n1 * c + m_in.n2 * ct);
        const double rsB = as * as;
        text(p, QPointF(14, a.top() + 18), "BREWSTER · REFLECTED BEAM IS s-POLARIZED", t.amber, 10);
        const double y0 = a.top() + a.height() * 0.42;
        const QPointF o(a.center().x(), y0);
        const double L = std::min(a.height() * 0.30, 110.0);
        QColor glass = t.blue; glass.setAlpha(45);
        p.setPen(Qt::NoPen); p.setBrush(glass);
        p.drawRect(QRectF(a.left() + 20, y0, a.width() - 40, a.bottom() - y0 - 76));
        p.setPen(QPen(t.sage, 1.8)); p.drawLine(QPointF(a.left() + 20, y0), QPointF(a.right() - 20, y0));
        text(p, QPointF(a.left() + 26, y0 - 8), "n₁ = " + num(m_in.n1, 4) + "  (air side)", t.muted, 10);
        text(p, QPointF(a.left() + 26, y0 + 18), "n₂ = " + num(m_in.n2, 4) + "  (glass side)", t.muted, 10);
        p.setPen(QPen(t.dim, 1, Qt::DashLine)); p.drawLine(o + QPointF(0, -L - 12), o + QPointF(0, L));
        ray(p, o + QPointF(-std::sin(thB) * L, -std::cos(thB) * L), o, t.amber, 1.8);            // incident at θB
        ray(p, o, o + QPointF(std::sin(thB) * L, -std::cos(thB) * L), t.amber, 1.6);             // reflected: s only
        ray(p, o, o + QPointF(std::cos(thB) * L, std::sin(thB) * L), t.amber, 1.4);              // refracted at 90° − θB
        text(p, o + QPointF(-L * 0.70, -L * 0.30), "θB = " + num(deg, 4) + "°", t.text, 10, Qt::AlignRight);
        text(p, o + QPointF(L * 0.26, -L * 0.62), "reflected · s only", t.amber, 9);
        text(p, o + QPointF(L * 0.26, L * 0.55), "refracted · " + num(90 - deg, 4) + "°", t.muted, 9);
        text(p, QPointF(14, a.bottom() - 58), "tan θB = n₂ / n₁ = " + num(m_in.n2 / m_in.n1, 5), t.muted, 10);
        text(p, QPointF(14, a.bottom() - 42), "R_p = 0.0000   ·   R_s = " + num(rsB, 4), t.text, 10);
        text(p, QPointF(14, a.bottom() - 26), "reflected and refracted rays are perpendicular", t.muted, 10);
        text(p, QPointF(14, a.bottom() - 10), "unpolarized beam: " + num((rsB / 2) * 100, 3) + "% reflected", t.muted, 10);
        return;
    }
    const double i0 = m_in.unpolarized ? m_in.incidentIntensity / 2 : m_in.incidentIntensity;
    const double iOut = i0 * std::pow(std::cos(m_in.angleDeg * std::numbers::pi / 180), 2);
    const double half = a.top() + a.height() * 0.50;
    text(p, QPointF(14, a.top() + 18), "A / MALUS · AXIS ROTATION", t.amber, 10);
    const double cy = a.top() + a.height() * 0.22;
    const QPointF p1(a.left() + a.width() * 0.34, cy), p2(a.left() + a.width() * 0.70, cy);
    ray(p, QPointF(a.left() + 14, cy), p1 - QPointF(28, 0), t.sage, 1.4);
    ray(p, p1 + QPointF(28, 0), p2 - QPointF(28, 0), t.sage, 1.4);
    ray(p, p2 + QPointF(28, 0), QPointF(a.right() - 10, cy), t.amber, 1.4);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(t.sage, 1.6)); p.drawEllipse(p1, 22, 38); p.drawLine(p1 + QPointF(0, -34), p1 + QPointF(0, 34));
    const double ang = m_in.angleDeg * std::numbers::pi / 180;
    p.setPen(QPen(t.amber, 1.6)); p.drawEllipse(p2, 22, 38);
    p.drawLine(p2 + QPointF(-std::sin(ang) * 34, -std::cos(ang) * 34), p2 + QPointF(std::sin(ang) * 34, std::cos(ang) * 34));
    text(p, QPointF(a.left() + 14, cy - 22), "Iᵢₙ = " + num(m_in.incidentIntensity, 4) + " W/m²", t.muted, 9);
    text(p, QPointF(p1.x() + 34, cy - 22), "I₀ = " + num(i0, 4) + " W/m²", t.muted, 9);
    text(p, QPointF(a.right() - 10, cy - 22), "I = " + num(iOut, 5) + " W/m²", t.amber, 9, Qt::AlignRight);
    text(p, p1 + QPointF(0, 54), "P₁ axis 0°", t.muted, 9, Qt::AlignHCenter);
    text(p, p2 + QPointF(0, 54), "P₂ axis " + num(m_in.angleDeg, 4) + "°", t.muted, 9, Qt::AlignHCenter);
    text(p, QPointF(a.center().x(), cy + 76), "θ = " + num(m_in.angleDeg, 4) + "° between transmission axes", t.text, 10, Qt::AlignHCenter);

    text(p, QPointF(14, half + 14), "B / BREWSTER · INTERFACE ANGLE", t.amber, 10);
    const double thB = std::atan(m_in.n2 / m_in.n1), o_y = half + (a.bottom() - half) * 0.58;
    const QPointF o(a.right() - a.width() * 0.30, o_y);
    const double L = 70;
    p.setPen(QPen(t.sage, 1.6)); p.drawLine(QPointF(a.left() + a.width() * 0.45, o.y()), QPointF(a.right() - 14, o.y()));
    p.setPen(QPen(t.dim, 1, Qt::DashLine)); p.drawLine(o + QPointF(0, -L), o + QPointF(0, L));
    ray(p, o + QPointF(-std::sin(thB) * L, -std::cos(thB) * L), o, t.amber, 1.6);
    ray(p, o, o + QPointF(std::sin(thB) * L, -std::cos(thB) * L), t.amber, 1.6);
    ray(p, o, o + QPointF(std::cos(thB) * L, std::sin(thB) * L), t.amber, 1.3);                // refracted: 90° − θB from normal
    text(p, o + QPointF(18, -L * 0.62), num(thB * 180 / std::numbers::pi, 4) + "°", t.amber, 10);
    text(p, o + QPointF(24, L * 0.62), num(90 - thB * 180 / std::numbers::pi, 4) + "°", t.muted, 10);
    text(p, QPointF(a.left() + a.width() * 0.46, o.y() + 22), "glass n₂ = " + num(m_in.n2, 4), t.muted, 10);
    text(p, QPointF(14, half + 40), "tan θB = n₂/n₁ = " + num(m_in.n2 / m_in.n1, 5), t.muted, 10);
    text(p, QPointF(14, half + 56), "θB = " + num(thB * 180 / std::numbers::pi, 6) + "°", t.muted, 10);
    text(p, QPointF(14, half + 76), "Measured from normal,", t.muted, 10);
    text(p, QPointF(14, half + 92), "not between polarizer axes.", t.muted, 10);
}

// ============================================================================ dashboard previews
PreviewIcon::PreviewIcon(Kind kind, QWidget* parent) : QWidget(parent), m_kind(kind) { setFixedSize(130, 132); }

void PreviewIcon::paintEvent(QPaintEvent*) {
    const Theme& t = th();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), t.canvas);
    p.setPen(QPen(t.amber, 1.4));
    p.setBrush(Qt::NoBrush);
    const QPointF c(width() / 2.0, height() / 2.0);
    switch (m_kind) {
    case Refraction:
        p.drawLine(QPointF(8, c.y()), QPointF(width() - 8, c.y()));
        p.drawLine(QPointF(c.x() - 40, c.y() - 55), QPointF(c.x() + 40, c.y() + 55));
        p.drawLine(QPointF(c.x() + 40, c.y() - 55), QPointF(c.x() - 10, c.y()));
        p.drawLine(QPointF(c.x(), 14), QPointF(c.x(), height() - 14));
        p.drawLine(QPointF(c.x() - 25, c.y() - 55), QPointF(c.x() + 38, c.y() + 25));
        break;
    case Lens: {
        QPainterPath lens;
        lens.moveTo(c.x() + 18, 12); lens.quadTo(c.x() + 36, c.y(), c.x() + 18, height() - 12);
        lens.quadTo(c.x(), c.y(), c.x() + 18, 12);
        p.drawPath(lens);
        p.drawRect(QRectF(12, c.y() - 28, 80, 26));
        p.drawLine(QPointF(12, c.y() - 28), QPointF(width() - 12, c.y() + 40));
        p.drawLine(QPointF(12, c.y() - 2), QPointF(width() - 12, c.y() + 40));
        break;
    }
    case Prism:
        p.drawPolygon(QPolygonF({QPointF(c.x(), 22), QPointF(c.x() - 40, height() - 22), QPointF(c.x() + 40, height() - 22)}));
        p.drawLine(QPointF(c.x() - 22, c.y() + 8), QPointF(c.x() + 22, c.y() + 8));
        break;
    case Waves:
        for (int k = 0; k < 2; ++k) {
            QPainterPath w;
            const double y = c.y() + (k ? 14 : -14);
            w.moveTo(10, y); w.quadTo(c.x(), y + (k ? 42 : -42), width() - 10, y);
            w.quadTo(c.x(), y + (k ? -42 : 42), 10, y);
            p.drawPath(w);
        }
        break;
    }
}

// ============================================================================ Thick lens
void ThickLensCanvas::paintDiagram(QPainter& p, const QRectF& a) {
    const Theme& t = th();
    ThickLensInput in = m_in;
    const TL::Result r = TL::analyse(in);
    const double axisY = a.top() + a.height() * 0.52;
    p.setPen(QPen(t.dim, 1));
    p.drawLine(QPointF(20, axisY), QPointF(a.right() - 20, axisY));
    if (!r.valid) {
        text(p, QPointF(a.center().x(), a.center().y()), "Element not usable", t.error, 11, Qt::AlignHCenter);
        if (!r.error.empty()) text(p, QPointF(a.center().x(), a.center().y() + 18), QString::fromStdString(r.error), t.muted, 9, Qt::AlignHCenter);
        return;
    }

    const double f = std::fabs(r.focalLength);
    const double extent = std::max({f * 2.4, in.thickness * 4.0, in.diameter * 2.4});
    const double sc = (a.width() / 2 - 40) / std::max(extent, 1e-6);
    // the lens body is drawn at least 26 px thick so a thin element is still visible
    const double tPx = std::max(26.0, in.thickness * sc);
    const double v1x = a.center().x() - tPx / 2;                        // front vertex
    const double v2x = v1x + tPx;
    const double sag = std::min(a.height() * 0.30, 26.0);               // how strongly the surfaces curve on screen

    // ---- glass body: two arcs (a biconvex lens bulges outwards, a biconcave one pinches in)
    const double halfD = std::min(a.height() * 0.34, 96.0);
    QPainterPath body;
    const auto bulge = [&](double x, double sign, double radius) {       // sign: +1 right surface, -1 left surface
        const double k = std::clamp(std::fabs(radius) / 10.0, 0.35, 1.6);
        const double amount = (radius > 0 ? 1.0 : -1.0) * sign * sag * k;  // convex -> away from the centre
        return QPointF(x + amount, 0);
    };
    const QPointF topL = bulge(v1x, -1, in.r1), topR = bulge(v2x, 1, in.r2);
    body.moveTo(topL.x(), axisY - halfD);
    body.cubicTo(topL.x() + (topR.x() - topL.x()) * 0.35, axisY - halfD - sag * 0.15,
                 topL.x() + (topR.x() - topL.x()) * 0.65, axisY - halfD - sag * 0.15, topR.x(), axisY - halfD);
    body.lineTo(topR.x(), axisY + halfD);
    body.cubicTo(topL.x() + (topR.x() - topL.x()) * 0.65, axisY + halfD + sag * 0.15,
                 topL.x() + (topR.x() - topL.x()) * 0.35, axisY + halfD + sag * 0.15, topL.x(), axisY + halfD);
    body.closeSubpath();
    QColor glass = t.blue; glass.setAlpha(70);
    p.setPen(QPen(t.sage, 1.6));
    p.setBrush(glass);
    p.drawPath(body);

    // ---- axis marks: vertices, principal planes, focal points
    const auto tick = [&](double x, const QString& label, const QColor& c, bool dashed) {
        p.setPen(QPen(c, dashed ? 1 : 1.4, dashed ? Qt::DashLine : Qt::DotLine));
        p.drawLine(QPointF(x, axisY - halfD - 12), QPointF(x, axisY + halfD + 12));
        text(p, QPointF(x, axisY + halfD + 24), label, c, 9, Qt::AlignHCenter);
    };
    tick(v1x, "V₁", t.muted, false);
    tick(v2x, "V₂", t.muted, false);
    tick(v1x + r.h1 * sc, "H₁", t.amber, true);
    tick(v2x + r.h2 * sc, "H₂", t.amber, true);
    tick(v2x + r.backFocal * sc, "F′", t.sage, false);

    // ---- marginal rays from infinity: they cross the axis in front of the paraxial focus
    const double h = std::min(in.diameter / 2.0, halfD / sc);
    const QPointF entryL(v1x - 30, axisY - h * sc), entryR(v1x - 30, axisY + h * sc);
    const double paraxialX = v2x + r.backFocal * sc;
    const double marginalX = v2x + r.marginalFocus * sc;
    for (const QPointF& e : {entryL, entryR}) {
        ray(p, QPointF(24, e.y()), e, t.amber, 1.5);
        ray(p, e, QPointF(marginalX, axisY), t.amber, 1.3);
    }
    p.setPen(QPen(t.sage, 1.2, Qt::DashLine));
    p.drawLine(QPointF(paraxialX, axisY - 26), QPointF(paraxialX, axisY + 26));
    if (std::fabs(marginalX - paraxialX) > 1.5) {                       // the aberration is visible
        p.setPen(QPen(t.error, 1.2, Qt::DashLine));
        p.drawLine(QPointF(marginalX, axisY - 16), QPointF(marginalX, axisY + 16));
        text(p, QPointF((paraxialX + marginalX) / 2, axisY - 34), "ΔSA", t.error, 9, Qt::AlignHCenter);
    }

    // ---- the labels of the design
    text(p, QPointF(20, a.top() + 18), "THICK LENS · PRINCIPAL PLANES", t.amber, 10);
    text(p, QPointF(a.right() - 20, a.top() + 18), QString::fromStdString(r.element).toUpper(), t.amber, 10, Qt::AlignRight);
    text(p, QPointF(20, a.bottom() - 34), "f = " + num(r.focalLength) + " cm   BFL = " + num(r.backFocal) + " cm", t.muted, 10);
    text(p, QPointF(20, a.bottom() - 18), "t = " + num(in.thickness) + " cm   D = " + num(in.diameter) + " cm   " + QString::fromStdString(r.saClass), t.muted, 10);
    text(p, QPointF(a.right() - 20, a.bottom() - 18), "marginal (h = " + num(h, 2) + " cm) vs paraxial focus", t.dim, 9, Qt::AlignRight);
}

} // namespace phy104
