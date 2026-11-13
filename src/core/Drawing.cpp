#include "Drawing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace phy104 {

bool Drawing::operator==(const Drawing& o) const {
    if (shapes.size() != o.shapes.size()) return false;
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        const Shape& a = shapes[i];
        const Shape& b = o.shapes[i];
        if (a.type != b.type || a.color != b.color || a.width != b.width || a.dashed != b.dashed || a.flag != b.flag ||
            a.text != b.text || a.points.size() != b.points.size()) return false;
        for (std::size_t k = 0; k < a.points.size(); ++k)
            if (a.points[k].x != b.points[k].x || a.points[k].y != b.points[k].y) return false;
    }
    return true;
}

namespace DrawingGeometry {
namespace {

double segmentDistance(Point p, Point a, Point b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double t = len2 < 1e-12 ? 0.0 : ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

double polylineDistance(const std::vector<Point>& pts, Point p, bool closed) {
    if (pts.empty()) return 1e18;
    if (pts.size() == 1) return std::hypot(p.x - pts[0].x, p.y - pts[0].y);
    double best = 1e18;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) best = std::min(best, segmentDistance(p, pts[i], pts[i + 1]));
    if (closed) best = std::min(best, segmentDistance(p, pts.back(), pts.front()));
    return best;
}

} // namespace

Rect bounds(const Shape& s) {
    Rect r{1e18, 1e18, -1e18, -1e18};
    for (const Point& p : s.points) {
        r.left = std::min(r.left, p.x); r.right = std::max(r.right, p.x);
        r.top = std::min(r.top, p.y);   r.bottom = std::max(r.bottom, p.y);
    }
    if (s.points.empty()) return {0, 0, 0, 0};
    if (s.type == ShapeType::Text || s.type == ShapeType::Equation) {          // approximate text box
        const double w = std::max<double>(24.0, static_cast<double>(s.text.size()) * (s.type == ShapeType::Text ? 8.0 : 9.0));
        r.right = r.left + w;
        r.top -= 18;
    }
    if (s.type == ShapeType::Lens) { r.left -= 14; r.right += 14; }
    if (s.type == ShapeType::Mirror) r.right += 18;
    return r;
}

double distance(const Shape& s, Point p) {
    const auto& pts = s.points;
    switch (s.type) {
    case ShapeType::Text:
    case ShapeType::Equation: {
        const Rect r = bounds(s);
        const double dx = std::max({r.left - p.x, 0.0, p.x - r.right});
        const double dy = std::max({r.top - p.y, 0.0, p.y - r.bottom});
        return std::hypot(dx, dy);
    }
    case ShapeType::Rect: {
        if (pts.size() < 2) return 1e18;
        const Point a = pts[0], c = pts[1], b{c.x, a.y}, d{a.x, c.y};
        return polylineDistance({a, b, c, d}, p, true);
    }
    case ShapeType::Ellipse: {
        if (pts.size() < 2) return 1e18;
        const double cx = (pts[0].x + pts[1].x) / 2, cy = (pts[0].y + pts[1].y) / 2;
        const double rx = std::fabs(pts[1].x - pts[0].x) / 2, ry = std::fabs(pts[1].y - pts[0].y) / 2;
        if (rx < 1e-9 || ry < 1e-9) return std::hypot(p.x - cx, p.y - cy);
        const double k = std::hypot((p.x - cx) / rx, (p.y - cy) / ry);       // 1.0 on the outline
        return std::fabs(k - 1.0) * std::min(rx, ry);
    }
    case ShapeType::Prism: {
        if (pts.size() < 2) return 1e18;
        const Rect r = bounds(s);
        return polylineDistance({{(r.left + r.right) / 2, r.top}, {r.left, r.bottom}, {r.right, r.bottom}}, p, true);
    }
    case ShapeType::Pen:
        return polylineDistance(pts, p, false);
    default:                                       // Line, Ray, Lens, Mirror, Interface, Axis, Normal: the segment / chord
        return pts.size() >= 2 ? segmentDistance(p, pts[0], pts[1]) : 1e18;
    }
}

void translate(Shape& s, double dx, double dy) {
    for (Point& p : s.points) { p.x += dx; p.y += dy; }
}

int hitTest(const Drawing& d, Point p, double tolerance) {
    for (int i = static_cast<int>(d.shapes.size()) - 1; i >= 0; --i) {           // topmost first
        const Shape& s = d.shapes[static_cast<std::size_t>(i)];
        if (distance(s, p) <= tolerance + s.width / 2) return i;
    }
    return -1;
}

double snap(double v, double step) {
    return step > 0 ? std::round(v / step) * step : v;
}

double angleDegrees(Point a, Point b) {
    double deg = std::atan2(-(b.y - a.y) + 0.0, b.x - a.x) * 180.0 / std::numbers::pi;   // "+ 0.0" turns -0.0 into 0.0
    if (deg <= -180.0) deg += 360.0;                                                      // range (-180, 180]
    return deg;
}

void parseColor(const std::string& hex, int& r, int& g, int& b) {
    r = 215; g = 153; b = 33;
    if (hex.size() == 7 && hex[0] == '#') {
        r = static_cast<int>(std::strtol(hex.substr(1, 2).c_str(), nullptr, 16));
        g = static_cast<int>(std::strtol(hex.substr(3, 2).c_str(), nullptr, 16));
        b = static_cast<int>(std::strtol(hex.substr(5, 2).c_str(), nullptr, 16));
    }
}

} // namespace DrawingGeometry

void DrawingHistory::reset(const Drawing& initial) {
    m_states = {initial};
    m_index = 0;
}

void DrawingHistory::commit(const Drawing& current) {
    if (current == m_states[m_index]) return;
    m_states.resize(m_index + 1);                  // drop the redo branch
    m_states.push_back(current);
    if (m_states.size() > m_limit) m_states.erase(m_states.begin());
    m_index = m_states.size() - 1;
}

Drawing DrawingHistory::undo() {
    if (m_index > 0) --m_index;
    return m_states[m_index];
}

Drawing DrawingHistory::redo() {
    if (m_index + 1 < m_states.size()) ++m_index;
    return m_states[m_index];
}

} // namespace phy104
