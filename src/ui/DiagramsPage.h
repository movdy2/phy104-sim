#pragma once
#include <QWidget>

class QLabel;
class QListWidget;

namespace phy104 {

class LatexWidget;
class SvgDiagramView;

/// Gallery of high-resolution optical-setup diagrams (SVG, theme aware, smooth scaling).
class DiagramsPage : public QWidget {
    Q_OBJECT
public:
    explicit DiagramsPage(QWidget* parent = nullptr);

private slots:
    void showDiagram(int row);
    void exportPng();

private:
    SvgDiagramView* m_view;
    QListWidget* m_list;
    QLabel* m_title;
    QLabel* m_caption;
    QWidget* m_equations;
};

} // namespace phy104
