#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

namespace phy104 {

/// Lucide SVG icons from the resource file, recoloured at run time ("currentColor" is replaced).
class Icons {
public:
    /// `colorOn` is used for the checked/On state (e.g. an active navigation item).
    static QIcon get(const QString& name, const QColor& color, const QColor& colorOn = QColor(), int size = 18);
    static QPixmap pixmap(const QString& name, const QColor& color, int size, qreal dpr = 2.0);
};

} // namespace phy104
