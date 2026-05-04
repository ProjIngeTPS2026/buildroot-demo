#pragma once

#include <QStyledItemDelegate>

namespace apping {

class BaseListDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit BaseListDelegate(QObject* parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

} // namespace apping
