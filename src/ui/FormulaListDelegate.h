#pragma once
#include <QHash>
#include <QPixmap>
#include <QStyledItemDelegate>

namespace phy104 {

/// Draws each formula as a card: (optional chapter heading), bold name, rendered LaTeX.
/// Item data: DisplayRole = name, IdRole = formula id, ChapterRole = heading to show above
/// the card (empty = none), LatexRole = LaTeX source.
class FormulaListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole, ChapterRole = Qt::UserRole + 1, LatexRole = Qt::UserRole + 2 };

    explicit FormulaListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    const QPixmap& formulaPixmap(const QString& latex) const;

    mutable QHash<QString, QPixmap> m_cache;     // LaTeX -> pixmap (cleared when the theme changes)
};

} // namespace phy104
