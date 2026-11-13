#pragma once
#include "BasePage.h"
#include "Drawing.h"

#include <QColor>
#include <QList>
#include <QPointF>
#include <QWidget>

class QLabel;
class QPainter;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace phy104 {

class Panel;

/// Whiteboard for optics sketches: paints a `Drawing` and turns mouse input into shapes.
///  * tools: select/move, pen, line, ray, rectangle, ellipse, text, LaTeX, lens, mirror, prism, interface, axis, normal
///  * snap to an 8 px grid, dashed strokes, six theme colours, three widths
///  * undo / redo via DrawingHistory, Delete removes the selection
///  * PNG (2x) and SVG export of the whole board
class DrawingCanvas : public QWidget {
    Q_OBJECT
public:
    enum Tool { Select, Pen, Line, Ray, Rectangle, Ellipse, Text, Equation, Lens, Mirror, Prism, Interface, Axis, Normal };

    explicit DrawingCanvas(QWidget* parent = nullptr);

    const Drawing& drawing() const { return m_drawing; }
    void setDrawing(const Drawing& drawing);           // also resets the undo history
    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    QColor color() const { return m_color; }
    void setColor(const QColor& color) { m_color = color; update(); }
    double strokeWidth() const { return m_width; }
    void setStrokeWidth(double width) { m_width = width; }
    bool snapEnabled() const { return m_snap; }
    void setSnapEnabled(bool on) { m_snap = on; }
    bool dashed() const { return m_dashed; }
    void setDashed(bool on) { m_dashed = on; }
    QString labelText() const { return m_label; }
    void setLabelText(const QString& text) { m_label = text; }
    int selected() const { return m_selected; }
    bool canUndo() const { return m_history.canUndo(); }
    bool canRedo() const { return m_history.canRedo(); }

    bool exportPng(const QString& path, int scale = 2) const;
    bool exportSvg(const QString& path) const;

public slots:
    void undo();
    void redo();
    void clear();
    void removeSelected();

signals:
    void drawingChanged();                        // a shape was added, moved, removed or undone/redone
    void selectionChanged(int index);
    void historyChanged();
    void cursorMoved(double x, double y);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    ShapeType shapeTypeFor(Tool tool) const;
    QPointF snapped(const QPointF& p) const;
    void paintShape(QPainter& p, const Shape& shape, bool selected) const;
    void renderScene(QPainter& p, const QSize& size) const;
    void commit();
    void setSelected(int index);

    Drawing m_drawing;
    DrawingHistory m_history;
    Tool m_tool = Pen;
    QColor m_color = QColor("#d79921");
    QString m_label;
    double m_width = 2.0;
    bool m_snap = true;
    bool m_dashed = false;
    int m_selected = -1;
    int m_active = -1;                            // shape being drawn (index into m_drawing.shapes)
    bool m_dragging = false;
    bool m_moved = false;
    QPointF m_last;
};

/// Draw Space page: tool palette | canvas | style, label, history and export.
/// The sketch is stored in the session (`SessionLog::setDrawing`) and written to the .phy file.
class DrawSpacePage : public BasePage {
    Q_OBJECT
public:
    explicit DrawSpacePage(AppContext* context, QWidget* parent = nullptr);

    DrawingCanvas* canvas() const { return m_canvas; }
    void setTool(DrawingCanvas::Tool tool);            // also checks the matching palette button

    /// The file dialogs of the Export buttons, without the dialog (used by the tests).
    bool exportPngTo(const QString& path) const { return m_canvas->exportPng(path, 2); }
    bool exportSvgTo(const QString& path) const { return m_canvas->exportSvg(path); }

private slots:
    void refreshMeta();
    void refreshHistory();
    void saveDrawing();
    void exportPng();
    void exportSvg();

private:
    DrawingCanvas* m_canvas;
    Panel* m_sketchPanel;
    QLabel* m_foot;
    QLineEdit* m_labelEdit;
    QList<QToolButton*> m_toolButtons;
    QPushButton *m_undo, *m_redo, *m_delete, *m_clear;
};

} // namespace phy104
