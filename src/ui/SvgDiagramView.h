#pragma once
#include <QSvgRenderer>
#include <QWidget>

namespace phy104 {

/// Shows an SVG diagram scaled to the widget (aspect ratio kept, smooth at any size).
/// The SVG files contain colour tokens (@red@, @fg@ ...). They are replaced with the active
/// Gruvbox theme colours, so one file serves both dark and light mode.
class SvgDiagramView : public QWidget {
    Q_OBJECT
public:
    explicit SvgDiagramView(QWidget* parent = nullptr);

    /// Loads a diagram from a Qt resource path or a file. Returns false if it cannot be read.
    bool load(const QString& path);

    /// Renders the diagram to a PNG file with the given width in pixels (height follows the aspect ratio).
    bool exportPng(const QString& fileName, int widthPixels = 2400) const;

    /// Replaces colour tokens with the theme colours. (public for testing)
    static QByteArray applyTheme(QByteArray svg);

    QSize sizeHint() const override { return QSize(720, 480); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void reload();

    QString m_path;
    QByteArray m_template;
    QSvgRenderer m_renderer;
};

} // namespace phy104
