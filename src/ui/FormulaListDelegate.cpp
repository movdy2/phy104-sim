#include "FormulaListDelegate.h"
#include "LatexRenderer.h"
#include "ThemeManager.h"

#include <QPainter>

namespace phy104 {
namespace {
constexpr int kHeaderHeight = 24;
constexpr int kNameHeight = 22;
constexpr int kPad = 10;
}

FormulaListDelegate::FormulaListDelegate(QObject* parent) : QStyledItemDelegate(parent) {
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { m_cache.clear(); });
}

const QPixmap& FormulaListDelegate::formulaPixmap(const QString& latex) const {
    auto it = m_cache.find(latex);
    if (it == m_cache.end()) {
        QFont font; font.setPointSizeF(13);
        it = m_cache.insert(latex, LatexRenderer::render(latex, font, ThemeManager::instance().theme().text, 2.0));
    }
    return it.value();
}

QSize FormulaListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const bool header = !index.data(ChapterRole).toString().isEmpty();
    const QPixmap& pix = formulaPixmap(index.data(LatexRole).toString());
    const int h = (header ? kHeaderHeight : 0) + 6 + kPad + kNameHeight + int(pix.deviceIndependentSize().height()) + kPad;
    return QSize(option.rect.width(), h);
}

void FormulaListDelegate::paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const Theme& t = ThemeManager::instance().theme();
    const QString chapter = index.data(ChapterRole).toString();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;

    p->save();
    p->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    QRect r = option.rect.adjusted(4, 3, -4, -3);

    if (!chapter.isEmpty()) {                                   // chapter heading above the card
        QFont f = option.font; f.setBold(true); f.setPixelSize(11);
        p->setFont(f);
        p->setPen(t.aqua);
        p->drawText(QRect(r.left() + 2, r.top(), r.width(), kHeaderHeight - 2), Qt::AlignVCenter | Qt::AlignLeft,
                    chapter.toUpper());
        r.setTop(r.top() + kHeaderHeight);
    }

    p->setPen(QPen(selected ? t.yellow : t.border, selected ? 2 : 1));
    p->setBrush(selected || hover ? t.cardHover : t.bg);
    p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    QFont nameFont = option.font; nameFont.setBold(true); nameFont.setPixelSize(13);
    p->setFont(nameFont);
    p->setPen(t.yellow);
    p->drawText(QRect(r.left() + kPad, r.top() + kPad - 2, r.width() - 2 * kPad, kNameHeight),
                Qt::AlignVCenter | Qt::AlignLeft, index.data(Qt::DisplayRole).toString());

    const QPixmap& pix = formulaPixmap(index.data(LatexRole).toString());
    p->drawPixmap(QPointF(r.left() + kPad, r.top() + kPad + kNameHeight), pix);
    p->restore();
}

} // namespace phy104
