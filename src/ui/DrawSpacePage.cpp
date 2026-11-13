#include "DrawSpacePage.h"
#include "AppContext.h"
#include "Components.h"
#include "LatexRenderer.h"
#include "ThemeManager.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QStandardPaths>
#include <QSvgGenerator>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <algorithm>
#include <cmath>

namespace phy104 {
namespace {

QColor colorOf(const Shape& s) {
    int r = 215, g = 153, b = 33;
    DrawingGeometry::parseColor(s.color, r, g, b);
    return QColor(r, g, b);
}

QPointF at(const Point& p) { return QPointF(p.x, p.y); }

} // namespace

// ============================================================================ DrawingCanvas
DrawingCanvas::DrawingCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("drawCanvas");
    setMinimumSize(360, 300);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    m_history.reset(m_drawing);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
}

void DrawingCanvas::setDrawing(const Drawing& drawing) {
    m_drawing = drawing;
    m_history.reset(m_drawing);
    m_selected = -1;
    m_active = -1;
    m_dragging = false;
    emit selectionChanged(-1);
    emit historyChanged();
    update();
}

void DrawingCanvas::setTool(Tool tool) {
    m_tool = tool;
    setSelected(-1);
    setCursor(tool == Select ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

ShapeType DrawingCanvas::shapeTypeFor(Tool tool) const {
    switch (tool) {
    case Pen: return ShapeType::Pen;
    case Line: return ShapeType::Line;
    case Ray: return ShapeType::Ray;
    case Rectangle: return ShapeType::Rect;
    case Ellipse: return ShapeType::Ellipse;
    case Text: return ShapeType::Text;
    case Equation: return ShapeType::Equation;
    case Lens: return ShapeType::Lens;
    case Mirror: return ShapeType::Mirror;
    case Prism: return ShapeType::Prism;
    case Interface: return ShapeType::Interface;
    case Axis: return ShapeType::Axis;
    case Normal: return ShapeType::Normal;
    case Select: break;
    }
    return ShapeType::Line;
}

QPointF DrawingCanvas::snapped(const QPointF& p) const {
    if (!m_snap) return p;
    return QPointF(DrawingGeometry::snap(p.x(), 8.0), DrawingGeometry::snap(p.y(), 8.0));
}

void DrawingCanvas::setSelected(int index) {
    if (index >= static_cast<int>(m_drawing.shapes.size())) index = -1;
    if (m_selected == index) return;
    m_selected = index;
    emit selectionChanged(m_selected);
    update();
}

void DrawingCanvas::commit() {
    m_history.commit(m_drawing);
    emit historyChanged();
    emit drawingChanged();
    update();
}

void DrawingCanvas::undo() {
    m_drawing = m_history.undo();
    m_active = -1;
    setSelected(-1);
    emit historyChanged();
    emit drawingChanged();
    update();
}

void DrawingCanvas::redo() {
    m_drawing = m_history.redo();
    m_active = -1;
    setSelected(-1);
    emit historyChanged();
    emit drawingChanged();
    update();
}

void DrawingCanvas::clear() {
    if (m_drawing.shapes.empty()) return;
    m_drawing.shapes.clear();
    m_active = -1;
    setSelected(-1);
    commit();
}

void DrawingCanvas::removeSelected() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_drawing.shapes.size())) return;
    m_drawing.shapes.erase(m_drawing.shapes.begin() + m_selected);
    m_active = -1;
    setSelected(-1);
    commit();
}

void DrawingCanvas::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    const QPointF pos = snapped(event->position());
    m_last = pos;
    m_moved = false;

    if (m_tool == Select) {
        const int hit = DrawingGeometry::hitTest(m_drawing, {pos.x(), pos.y()}, 8.0);
        setSelected(hit);
        m_dragging = hit >= 0;
        return;
    }
    if (m_tool == Text || m_tool == Equation) {
        Shape shape;
        shape.type = shapeTypeFor(m_tool);
        shape.points = {{pos.x(), pos.y()}};
        shape.color = m_color.name().toStdString();
        shape.width = m_width;
        shape.dashed = m_dashed;
        QString text = m_label.trimmed();
        if (text.isEmpty()) text = m_tool == Text ? QStringLiteral("label") : QStringLiteral("\\theta");
        shape.text = text.toStdString();
        m_drawing.shapes.push_back(shape);
        m_active = -1;
        setSelected(static_cast<int>(m_drawing.shapes.size()) - 1);
        commit();
        return;
    }

    Shape shape;
    shape.type = shapeTypeFor(m_tool);
    shape.color = m_color.name().toStdString();
    shape.width = m_width;
    shape.dashed = m_dashed;
    if (m_tool == Pen) shape.points = {{pos.x(), pos.y()}};
    else shape.points = {{pos.x(), pos.y()}, {pos.x(), pos.y()}};
    m_drawing.shapes.push_back(shape);
    m_active = static_cast<int>(m_drawing.shapes.size()) - 1;
    update();
}

void DrawingCanvas::mouseMoveEvent(QMouseEvent* event) {
    const QPointF pos = snapped(event->position());
    emit cursorMoved(pos.x(), pos.y());

    if (m_tool == Select) {
        if (m_dragging && m_selected >= 0) {
            Shape& shape = m_drawing.shapes[static_cast<std::size_t>(m_selected)];
            DrawingGeometry::translate(shape, pos.x() - m_last.x(), pos.y() - m_last.y());
            m_last = pos;
            m_moved = true;
            update();
        }
        return;
    }
    if (m_active < 0 || m_active >= static_cast<int>(m_drawing.shapes.size())) return;
    Shape& shape = m_drawing.shapes[static_cast<std::size_t>(m_active)];
    if (shape.type == ShapeType::Pen) {
        if (shape.points.empty()) return;
        const Point& last = shape.points.back();
        if (std::hypot(pos.x() - last.x, pos.y() - last.y) >= 2.0) shape.points.push_back({pos.x(), pos.y()});
    } else if (shape.points.size() >= 2) {
        shape.points[1] = {pos.x(), pos.y()};
    }
    update();
}

void DrawingCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) { QWidget::mouseReleaseEvent(event); return; }
    if (m_tool == Select) {
        if (m_dragging && m_moved) commit();
        m_dragging = false;
        return;
    }
    if (m_active < 0 || m_active >= static_cast<int>(m_drawing.shapes.size())) return;
    const Shape& shape = m_drawing.shapes[static_cast<std::size_t>(m_active)];
    bool keep = shape.points.size() >= 2;
    if (keep && shape.type != ShapeType::Pen) {
        const Point& a = shape.points[0];
        const Point& b = shape.points[1];
        keep = std::hypot(b.x - a.x, b.y - a.y) >= 4.0;
    }
    if (!keep) {
        m_drawing.shapes.erase(m_drawing.shapes.begin() + m_active);
        m_active = -1;
        update();
        return;
    }
    const int finished = m_active;
    m_active = -1;
    setSelected(finished);
    commit();
}

void DrawingCanvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) { removeSelected(); return; }
    if (event->key() == Qt::Key_Escape) { setSelected(-1); return; }
    if (event->matches(QKeySequence::Undo)) { undo(); return; }
    if (event->matches(QKeySequence::Redo)) { redo(); return; }
    QWidget::keyPressEvent(event);
}

void DrawingCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    renderScene(p, size());
}

void DrawingCanvas::renderScene(QPainter& p, const QSize& area) const {
    const Theme& th = ThemeManager::instance().theme();
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.fillRect(QRect(QPoint(0, 0), area), th.canvas);
    p.setPen(QPen(th.grid, 1));
    for (int x = 0; x < area.width(); x += 24) p.drawLine(x, 0, x, area.height());
    for (int y = 0; y < area.height(); y += 24) p.drawLine(0, y, area.width(), y);
    for (std::size_t i = 0; i < m_drawing.shapes.size(); ++i)
        paintShape(p, m_drawing.shapes[i], static_cast<int>(i) == m_selected);
}

void DrawingCanvas::paintShape(QPainter& p, const Shape& s, bool selected) const {
    const QColor color = colorOf(s);
    const auto& pts = s.points;
    QPen pen(color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (s.dashed || s.type == ShapeType::Normal) pen.setStyle(Qt::DashLine);
    if (s.type == ShapeType::Axis) pen.setStyle(Qt::DashDotLine);
    if (s.type == ShapeType::Interface) pen.setWidthF(std::max(3.0, s.width * 2.5));
    p.save();
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto segment = [&] {
        if (pts.size() >= 2) p.drawLine(at(pts[0]), at(pts[1]));
    };

    switch (s.type) {
    case ShapeType::Pen:
        if (pts.size() == 1) { p.setBrush(color); p.drawEllipse(at(pts[0]), s.width, s.width); }
        else if (pts.size() > 1) {
            QPolygonF poly;
            for (const Point& q : pts) poly << at(q);
            p.drawPolyline(poly);
        }
        break;
    case ShapeType::Line: segment(); break;
    case ShapeType::Ray: {
        segment();
        if (pts.size() >= 2) {
            const QPointF tip = at(pts[1]);
            const double angle = std::atan2(pts[1].y - pts[0].y, pts[1].x - pts[0].x);
            const double len = 10.0 + s.width * 2.0;
            for (const double d : {2.6, -2.6})
                p.drawLine(tip, tip + QPointF(len * std::cos(angle + d), len * std::sin(angle + d)));
        }
        break;
    }
    case ShapeType::Rect:
        if (pts.size() >= 2) p.drawRect(QRectF(at(pts[0]), at(pts[1])).normalized());
        break;
    case ShapeType::Ellipse:
        if (pts.size() >= 2) p.drawEllipse(QRectF(at(pts[0]), at(pts[1])).normalized());
        break;
    case ShapeType::Prism: {
        const Rect bb = DrawingGeometry::bounds(s);
        QPolygonF tri;
        tri << QPointF((bb.left + bb.right) / 2, bb.top) << QPointF(bb.left, bb.bottom) << QPointF(bb.right, bb.bottom);
        QColor fill = color;
        fill.setAlpha(30);
        p.setBrush(fill);
        p.drawPolygon(tri);
        break;
    }
    case ShapeType::Lens: {
        if (pts.size() < 2) break;
        const double cx = (pts[0].x + pts[1].x) / 2;
        const double cy = (pts[0].y + pts[1].y) / 2;
        const double h = std::hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
        const double bulge = std::max(10.0, h * 0.18) * (s.flag ? 1.0 : -1.0);
        QPainterPath path;
        path.moveTo(at(pts[0]));
        path.quadTo(QPointF(cx + bulge, cy), at(pts[1]));
        path.quadTo(QPointF(cx - bulge, cy), at(pts[0]));
        QColor fill = color;
        fill.setAlpha(30);
        p.setBrush(fill);
        p.drawPath(path);
        break;
    }
    case ShapeType::Mirror: {
        if (pts.size() < 2) break;
        const double cx = (pts[0].x + pts[1].x) / 2;
        const double cy = (pts[0].y + pts[1].y) / 2;
        const double h = std::hypot(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
        const double bulge = std::max(12.0, h * 0.16) * (s.flag ? 1.0 : -1.0);
        QPainterPath path;
        path.moveTo(at(pts[0]));
        path.quadTo(QPointF(cx - bulge, cy), at(pts[1]));
        p.drawPath(path);
        for (int i = 1; i < 7; ++i) {                       // hatching on the back of the mirror
            const double t = i / 7.0;
            const QPointF on(QPointF(pts[0].x, pts[0].y) + t * (QPointF(pts[1].x, pts[1].y) - QPointF(pts[0].x, pts[0].y)));
            const QPointF back = on + QPointF(-bulge * 0.9, 0);
            p.drawLine(back, back + QPointF(-7.0, 0));
        }
        break;
    }
    case ShapeType::Interface:
    case ShapeType::Axis:
    case ShapeType::Normal:
        segment();
        break;
    case ShapeType::Text: {
        if (pts.empty()) break;
        QFont f(ThemeManager::uiFont());
        f.setPixelSize(std::max(11, static_cast<int>(11 + s.width)));
        p.setFont(f);
        p.setPen(color);
        p.drawText(QPointF(pts[0].x, pts[0].y - 6), QString::fromStdString(s.text));
        break;
    }
    case ShapeType::Equation: {
        if (pts.empty()) break;
        QFont f(ThemeManager::uiFont());
        f.setPixelSize(15);
        const QPixmap pixmap = LatexRenderer::render(QString::fromStdString(s.text), f, color, 2.0);
        if (!pixmap.isNull()) p.drawPixmap(QPointF(pts[0].x, pts[0].y - pixmap.height() / (2.0 * pixmap.devicePixelRatio())), pixmap);
        break;
    }
    }
    if (selected) {
        const Rect bb = DrawingGeometry::bounds(s);
        p.setPen(QPen(ThemeManager::instance().theme().amber, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(bb.left - 4, bb.top - 4, bb.right - bb.left + 8, bb.bottom - bb.top + 8));
    }
    p.restore();
}

bool DrawingCanvas::exportPng(const QString& path, int scale) const {
    if (path.isEmpty() || width() <= 0 || height() <= 0) return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QPixmap pixmap(size() * scale);
    pixmap.fill(ThemeManager::instance().theme().canvas);
    QPainter p(&pixmap);
    p.scale(scale, scale);
    renderScene(p, size());
    p.end();
    return pixmap.save(path);
}

bool DrawingCanvas::exportSvg(const QString& path) const {
    if (path.isEmpty() || width() <= 0 || height() <= 0) return false;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setSize(size());
    generator.setViewBox(QRect(QPoint(0, 0), size()));
    generator.setTitle("PHY104 Draw Space");
    generator.setDescription("Optics sketch");
    QPainter p;
    if (!p.begin(&generator)) return false;
    renderScene(p, size());
    p.end();
    return QFileInfo(path).size() > 0;
}

// ============================================================================ DrawSpacePage
DrawSpacePage::DrawSpacePage(AppContext* ctx, QWidget* parent)
    : BasePage("Draw Space", "SKETCHPAD / Optics diagrams: pen, rays, lenses, mirrors, prisms and LaTeX labels", ctx, parent) {
    useSaveExportHeader();
    connect(this, &BasePage::exportRequested, this, &DrawSpacePage::exportPng);
    auto cols = addThreeColumns(224, 258);

    // ---------------------------------------------------------------- left: tool palette
    auto* tools = new Panel("Tools", "SKETCH");
    auto* grid = new QGridLayout;
    grid->setSpacing(4);
    struct Entry { DrawingCanvas::Tool tool; const char* label; const char* tip; };
    const Entry entries[] = {
        {DrawingCanvas::Select, "Select", "Pick a shape and drag it"},
        {DrawingCanvas::Pen, "Pen", "Freehand stroke"},
        {DrawingCanvas::Line, "Line", "Straight line"},
        {DrawingCanvas::Ray, "Ray", "Ray with an arrow head"},
        {DrawingCanvas::Lens, "Lens", "Thin lens: click top, then bottom"},
        {DrawingCanvas::Mirror, "Mirror", "Spherical mirror: click top, then bottom"},
        {DrawingCanvas::Prism, "Prism", "Glass prism (bounding box)"},
        {DrawingCanvas::Interface, "Interface", "Boundary between two media"},
        {DrawingCanvas::Axis, "Axis", "Optical axis (dash-dot)"},
        {DrawingCanvas::Normal, "Normal", "Normal to the surface (dashed)"},
        {DrawingCanvas::Rectangle, "Rect", "Rectangle"},
        {DrawingCanvas::Ellipse, "Ellipse", "Ellipse / circular surface"},
        {DrawingCanvas::Text, "Text", "Text label (field on the right)"},
        {DrawingCanvas::Equation, "LaTeX", "Typeset a LaTeX label with LatexRenderer"}};
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    int row = 0, column = 0;
    for (const Entry& e : entries) {
        auto* b = new QToolButton;
        b->setObjectName("fmt");
        b->setText(e.label);
        b->setToolTip(e.tip);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setMinimumWidth(96);
        b->setProperty("tool", static_cast<int>(e.tool));
        connect(b, &QToolButton::clicked, this, [this, e] { setTool(e.tool); });
        group->addButton(b);
        grid->addWidget(b, row, column);
        m_toolButtons.append(b);
        if (++column == 2) { column = 0; ++row; }
    }
    tools->body()->addLayout(grid);
    auto* hint = new QLabel("Two-point shapes: click once, drag, release. Select then drag to move; Delete removes.");
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    tools->body()->addWidget(hint);
    tools->body()->addStretch();
    cols.left->addWidget(tools, 1);

    // ---------------------------------------------------------------- centre: the board
    m_sketchPanel = new Panel("Sketch", "00 SHAPES");
    m_canvas = new DrawingCanvas;
    m_canvas->setDrawing(ctx->log.drawing());           // restored from the .phy session
    m_sketchPanel->body()->setContentsMargins(8, 8, 8, 8);
    m_sketchPanel->body()->addWidget(m_canvas, 1);
    m_foot = new QLabel("Grid 24 px · snap 8 px · the sketch is autosaved in the .phy session");
    m_foot->setObjectName("caption");
    m_sketchPanel->body()->addWidget(m_foot);
    cols.center->addWidget(m_sketchPanel, 1);

    // ---------------------------------------------------------------- right: style
    auto* style = new Panel("Style", "PEN");
    auto* swatches = new QHBoxLayout;
    swatches->setSpacing(4);
    auto* colorGroup = new QButtonGroup(this);
    colorGroup->setExclusive(true);
    struct Swatch { const char* hex; const char* name; bool on; };
    const Swatch palette[] = {{"#d79921", "amber", true}, {"#a9b665", "green", false}, {"#fb4934", "red", false},
                              {"#83a598", "blue", false}, {"#d3869b", "purple", false}, {"#ebdbb2", "paper", false}};
    for (const Swatch& s : palette) {
        auto* b = new QToolButton;
        b->setObjectName("swatch");
        b->setCheckable(true);
        b->setChecked(s.on);
        b->setFixedSize(26, 26);
        b->setToolTip(s.name);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("hex", QString(s.hex));
        b->setStyleSheet(QString("QToolButton { background: %1; border: 2px solid transparent; border-radius: 5px; }"
                                 "QToolButton:checked { border-color: %2; }").arg(QString::fromLatin1(s.hex), ThemeManager::instance().theme().text.name()));
        connect(b, &QToolButton::clicked, this, [this, hex = QString(s.hex)] { m_canvas->setColor(QColor(hex)); });
        colorGroup->addButton(b);
        swatches->addWidget(b);
    }
    swatches->addStretch();
    style->body()->addLayout(swatches);

    auto* widths = new ModeTabs({"1 px", "2 px", "4 px"});
    widths->setCurrent(1);
    connect(widths, &ModeTabs::currentChanged, this, [this](int index) { m_canvas->setStrokeWidth(index == 0 ? 1.0 : index == 1 ? 2.0 : 4.0); });
    style->body()->addWidget(labeled("Stroke width", widths));
    auto* dashed = new QCheckBox("Dashed");
    connect(dashed, &QCheckBox::toggled, this, [this](bool on) { m_canvas->setDashed(on); });
    auto* snap = new QCheckBox("Snap to grid");
    snap->setChecked(true);
    connect(snap, &QCheckBox::toggled, this, [this](bool on) { m_canvas->setSnapEnabled(on); });
    auto* checks = new QHBoxLayout;
    checks->addWidget(dashed);
    checks->addStretch();
    checks->addWidget(snap);
    style->body()->addLayout(checks);
    cols.right->addWidget(style, 0);

    // ---------------------------------------------------------------- right: label
    auto* labelPanel = new Panel("Label", "TEXT / LATEX");
    m_labelEdit = new QLineEdit;
    m_labelEdit->setPlaceholderText("e.g. f = 10 cm   or   \\frac{1}{f} = ...");
    connect(m_labelEdit, &QLineEdit::textChanged, this, [this](const QString& text) { m_canvas->setLabelText(text); });
    labelPanel->body()->addWidget(m_labelEdit);
    auto* labelHint = new QLabel("Text writes this as-is; LaTeX typesets it with the LaTeX engine.");
    labelHint->setObjectName("hint");
    labelHint->setWordWrap(true);
    labelPanel->body()->addWidget(labelHint);
    cols.right->addWidget(labelPanel, 0);

    // ---------------------------------------------------------------- right: history
    auto* historyPanel = new Panel("History", "UNDO");
    m_undo = new QPushButton("Undo");
    m_undo->setObjectName("secondary");
    m_redo = new QPushButton("Redo");
    m_redo->setObjectName("secondary");
    m_delete = new QPushButton("Delete selected");
    m_delete->setObjectName("secondary");
    m_clear = new QPushButton("Clear sketch");
    m_clear->setObjectName("secondary");
    historyPanel->body()->addWidget(m_undo);
    historyPanel->body()->addWidget(m_redo);
    historyPanel->body()->addWidget(m_delete);
    historyPanel->body()->addWidget(m_clear);
    cols.right->addWidget(historyPanel, 0);

    // ---------------------------------------------------------------- right: export
    auto* exportPanel = new Panel("Export", "IMAGE");
    auto* png = new QPushButton("Export PNG");
    png->setObjectName("secondary");
    auto* svg = new QPushButton("Export SVG");
    svg->setObjectName("secondary");
    auto* exportHint = new QLabel("PNG is rendered at 2x for the report; SVG stays vector.");
    exportHint->setObjectName("hint");
    exportHint->setWordWrap(true);
    exportPanel->body()->addWidget(png);
    exportPanel->body()->addWidget(svg);
    exportPanel->body()->addWidget(exportHint);
    cols.right->addWidget(exportPanel, 0);
    cols.right->addStretch();

    connect(m_undo, &QPushButton::clicked, m_canvas, &DrawingCanvas::undo);
    connect(m_redo, &QPushButton::clicked, m_canvas, &DrawingCanvas::redo);
    connect(m_delete, &QPushButton::clicked, m_canvas, &DrawingCanvas::removeSelected);
    connect(m_clear, &QPushButton::clicked, m_canvas, &DrawingCanvas::clear);
    connect(png, &QPushButton::clicked, this, &DrawSpacePage::exportPng);
    connect(svg, &QPushButton::clicked, this, &DrawSpacePage::exportSvg);
    connect(m_canvas, &DrawingCanvas::drawingChanged, this, &DrawSpacePage::saveDrawing);
    connect(m_canvas, &DrawingCanvas::drawingChanged, this, &DrawSpacePage::refreshHistory);
    connect(m_canvas, &DrawingCanvas::historyChanged, this, &DrawSpacePage::refreshHistory);
    connect(m_canvas, &DrawingCanvas::selectionChanged, this, &DrawSpacePage::refreshHistory);
    connect(m_canvas, &DrawingCanvas::cursorMoved, this, [this](double x, double y) {
        m_foot->setText(QString("Grid 24 px · snap 8 px · cursor %1, %2 · the sketch is autosaved in the .phy session")
                            .arg(x, 0, 'f', 0).arg(y, 0, 'f', 0));
    });

    StatusInfo st;
    st.left = "Draw Space";
    st.mid = "Vector sketch";
    st.right = "Autosaved";
    setStatusInfo(st, false);

    setTool(DrawingCanvas::Pen);
    refreshMeta();
    refreshHistory();
}

void DrawSpacePage::setTool(DrawingCanvas::Tool tool) {
    m_canvas->setTool(tool);
    for (QToolButton* b : m_toolButtons)
        if (b->property("tool").toInt() == static_cast<int>(tool)) b->setChecked(true);
}

void DrawSpacePage::refreshMeta() {
    const int count = static_cast<int>(m_canvas->drawing().shapes.size());
    m_sketchPanel->setMeta(QString("%1 SHAPES").arg(count, 2, 10, QChar('0')));
}

void DrawSpacePage::refreshHistory() {
    m_undo->setEnabled(m_canvas->canUndo());
    m_redo->setEnabled(m_canvas->canRedo());
    m_delete->setEnabled(m_canvas->selected() >= 0);
    m_clear->setEnabled(!m_canvas->drawing().shapes.empty());
    refreshMeta();
}

void DrawSpacePage::saveDrawing() {
    m_ctx->log.setDrawing(m_canvas->drawing());
    m_ctx->saveSession();                                  // the sketch lives in the .phy session
    refreshMeta();
}

void DrawSpacePage::exportPng() {
    const QString suggested = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PHY104/PHY104_sketch.png";
    const QString path = QFileDialog::getSaveFileName(this, "Export sketch", suggested, "PNG image (*.png)");
    if (path.isEmpty()) return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!exportPngTo(path)) QMessageBox::warning(this, "Export", "Could not write " + path);
    else emit m_ctx->message("Exported " + path);
}

void DrawSpacePage::exportSvg() {
    const QString suggested = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PHY104/PHY104_sketch.svg";
    const QString path = QFileDialog::getSaveFileName(this, "Export sketch", suggested, "SVG image (*.svg)");
    if (path.isEmpty()) return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!exportSvgTo(path)) QMessageBox::warning(this, "Export", "Could not write " + path);
    else emit m_ctx->message("Exported " + path);
}

} // namespace phy104
