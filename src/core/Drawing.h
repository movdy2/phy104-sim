#pragma once
// Vector drawing model for the Draw Space (optics sketches). No Qt: shapes are plain data, so
// hit-testing, moving and undo/redo are unit tested and the drawing can be saved in the session file.
#include <string>
#include <vector>

namespace phy104 {

struct Point { double x = 0, y = 0; };

enum class ShapeType {
    Pen,          // freehand polyline
    Line,         // 2 points
    Ray,          // 2 points, arrow head at the end
    Rect,         // 2 points (corners)
    Ellipse,      // 2 points (corners of the bounding box)
    Text,         // 1 point, plain text
    Equation,     // 1 point, LaTeX text
    Lens,         // 2 points (top, bottom of the lens); flag = converging
    Mirror,       // 2 points (top, bottom); flag = concave
    Prism,        // 2 points (corners of the bounding box)
    Interface,    // 2 points: boundary between two media (thick line)
    Axis,         // 2 points: optical axis
    Normal        // 2 points: dashed normal
};

struct Shape {
    ShapeType type = ShapeType::Line;
    std::vector<Point> points;
    std::string color = "#d79921";     // CSS hex colour
    double width = 2.0;
    bool dashed = false;
    bool flag = true;                  // Lens: converging, Mirror: concave
    std::string text;                  // Text / Equation content
};

struct Rect { double left = 0, top = 0, right = 0, bottom = 0; };

struct Drawing {
    std::vector<Shape> shapes;
    bool operator==(const Drawing& o) const;
};

namespace DrawingGeometry {

Rect bounds(const Shape& s);
/// Smallest distance from p to the visible part of the shape (0 inside text boxes). Used for selecting / erasing.
double distance(const Shape& s, Point p);
void translate(Shape& s, double dx, double dy);
/// Index of the topmost shape within `tolerance` of p, or -1.
int hitTest(const Drawing& d, Point p, double tolerance);
/// Snaps v to the nearest multiple of `step` (step <= 0: unchanged).
double snap(double v, double step);
/// Angle in degrees of the segment a->b (0° = right, counter-clockwise positive, screen y downwards).
double angleDegrees(Point a, Point b);
/// Parses "#rrggbb"; falls back to amber.
void parseColor(const std::string& hex, int& r, int& g, int& b);

} // namespace DrawingGeometry

/// Undo / redo by snapshots (drawings are small). `limit` snapshots are kept.
class DrawingHistory {
public:
    explicit DrawingHistory(std::size_t limit = 100) : m_limit(limit) {}

    void reset(const Drawing& initial);
    /// Call AFTER a change: records the new state (clears the redo stack).
    void commit(const Drawing& current);
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index + 1 < m_states.size(); }
    Drawing undo();
    Drawing redo();
    const Drawing& current() const { return m_states[m_index]; }

private:
    std::vector<Drawing> m_states{Drawing{}};
    std::size_t m_index = 0;
    std::size_t m_limit;
};

} // namespace phy104
