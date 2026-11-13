#include "Icons.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace phy104 {

QPixmap Icons::pixmap(const QString& name, const QColor& color, int size, qreal dpr) {
    QFile file(":/icons/" + name + ".svg");
    if (!file.open(QIODevice::ReadOnly)) return QPixmap();
    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name().toUtf8());
    QSvgRenderer renderer(svg);
    QPixmap pix(qRound(size * dpr), qRound(size * dpr));
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pix;
}

QIcon Icons::get(const QString& name, const QColor& color, const QColor& colorOn, int size) {
    QIcon icon;
    icon.addPixmap(pixmap(name, color, size), QIcon::Normal, QIcon::Off);
    icon.addPixmap(pixmap(name, colorOn.isValid() ? colorOn : color, size), QIcon::Normal, QIcon::On);
    return icon;
}

} // namespace phy104
