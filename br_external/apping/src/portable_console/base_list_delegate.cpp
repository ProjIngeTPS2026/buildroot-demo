#include "portable_console/base_list_delegate.h"

#include "portable_console/base_directory_model.h"

#include <algorithm>

#include <QApplication>
#include <QPainter>

namespace apping {

namespace {

struct BadgeVisual {
    QString text;
    QColor accent;
    QColor background;
};

BadgeVisual stateVisual(const QModelIndex& index) {
    const QString status = index.data(BaseDirectoryModel::StatusRole).toString();
    const QString audioState = index.data(BaseDirectoryModel::AudioStateRole).toString();

    if (status == QStringLiteral("offline")) {
        return {QStringLiteral("Hors ligne"), QColor("#64748b"), QColor("#eef2f7")};
    }
    if (status == QStringLiteral("stale")) {
        return {QStringLiteral("Signal faible"), QColor("#b7791f"), QColor("#fff4d6")};
    }
    if (audioState == QStringLiteral("playing_prerecorded")) {
        return {QStringLiteral("Diffusion"), QColor("#155eef"), QColor("#e6efff")};
    }
    if (audioState == QStringLiteral("live_megaphone")) {
        return {QStringLiteral("Direct"), QColor("#c2410c"), QColor("#ffedd5")};
    }
    if (audioState == QStringLiteral("megaphone_ready")) {
        return {QStringLiteral("Message pret"), QColor("#8b5e00"), QColor("#fff3c4")};
    }
    return {QStringLiteral("Disponible"), QColor("#0f766e"), QColor("#ddfbf6")};
}

QRect takeBadgeRect(const QRect& source, int width) {
    return QRect(source.left(), source.top(), width, source.height());
}

void drawBadge(QPainter* painter,
               const QRect& rect,
               const QString& text,
               const QColor& fg,
               const QColor& bg) {
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
    painter->setPen(fg);
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
}

} // namespace

BaseListDelegate::BaseListDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {
}

QSize BaseListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    const int height = std::max(86, option.fontMetrics.height() * 5 + 24);
    return QSize(option.rect.width(), height);
}

void BaseListDelegate::paint(QPainter* painter,
                             const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect cardRect = option.rect.adjusted(3, 3, -3, -3);
    const bool targeted = index.data(BaseDirectoryModel::SelectedRole).toBool();
    const QString name = index.data(BaseDirectoryModel::NameRole).toString();
    const QString description = index.data(BaseDirectoryModel::DescriptionRole).toString();
    const QString lastSeen = index.data(BaseDirectoryModel::LastSeenTextRole).toString();
    const QString host = index.data(BaseDirectoryModel::HostRole).toString();
    const int controlPort = index.data(BaseDirectoryModel::ControlPortRole).toInt();
    const BadgeVisual state = stateVisual(index);

    const QColor borderColor = targeted ? QColor("#67a4f5") : QColor("#d7dde3");
    const QColor background = targeted ? QColor("#f4f9ff") : QColor("#ffffff");
    const QColor titleColor = QColor("#0f172a");
    const QColor detailColor = QColor("#52606d");

    painter->setPen(QPen(borderColor, targeted ? 2.0 : 1.0));
    painter->setBrush(background);
    painter->drawRoundedRect(cardRect, 10, 10);

    const QRect accentRect(cardRect.left(), cardRect.top(), 8, cardRect.height());
    painter->setPen(Qt::NoPen);
    painter->setBrush(state.accent);
    painter->drawRoundedRect(accentRect, 10, 10);

    const QRect contentRect = cardRect.adjusted(18, 9, -12, -9);

    QFont titleFont = option.font;
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0.0) {
        titleFont.setPointSizeF(titleFont.pointSizeF() + 0.5);
    } else {
        titleFont.setPixelSize(option.fontMetrics.height() + 2);
    }
    painter->setFont(titleFont);
    painter->setPen(titleColor);
    const QRect titleRect(contentRect.left(), contentRect.top(), contentRect.width() - 98, option.fontMetrics.height() + 4);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, name);

    painter->setFont(option.font);
    painter->setPen(QColor("#718096"));
    const QString seenText = lastSeen.isEmpty()
        ? QStringLiteral("vu --:--:--")
        : QStringLiteral("vu %1").arg(lastSeen);
    painter->drawText(QRect(contentRect.left(),
                            contentRect.top(),
                            contentRect.width() - 6,
                            option.fontMetrics.height() + 4),
                      Qt::AlignRight | Qt::AlignVCenter,
                      seenText);

    painter->setPen(detailColor);
    const QRect descriptionRect(contentRect.left(),
                                titleRect.bottom() + 2,
                                contentRect.width() - 42,
                                option.fontMetrics.lineSpacing() + 2);
    painter->drawText(descriptionRect, Qt::AlignLeft | Qt::AlignVCenter, description);

    const int badgeHeight = option.fontMetrics.height() + 8;
    const int badgeY = cardRect.bottom() - badgeHeight - 9;
    const QRect stateBadge(contentRect.left(),
                           badgeY,
                           std::max(98, option.fontMetrics.horizontalAdvance(state.text) + 20),
                           badgeHeight);
    drawBadge(painter, stateBadge, state.text, state.accent, state.background);

    const QString signalText = index.data(BaseDirectoryModel::StatusRole).toString() == QStringLiteral("offline")
        ? QStringLiteral("Signal --")
        : QStringLiteral("Signal bon");
    const int batteryPercent = 82 - (index.row() % 4) * 7;
    const QString detailText = QStringLiteral("%1  •  Batt. %2%  •  %3:%4")
                                   .arg(signalText,
                                        QString::number(batteryPercent),
                                        host,
                                        QString::number(controlPort));
    const QRect selectedBadge = targeted
        ? QRect(cardRect.right() - 102,
                cardRect.bottom() - badgeHeight - 9,
                std::max(82, option.fontMetrics.horizontalAdvance(QStringLiteral("Cible")) + 18),
                badgeHeight)
        : QRect();
    const QRect quickInfoRect(stateBadge.right() + 8,
                              badgeY,
                              (targeted ? selectedBadge.left() : contentRect.right()) - stateBadge.right() - 10,
                              badgeHeight);
    painter->setPen(QColor("#475569"));
    painter->drawText(quickInfoRect, Qt::AlignLeft | Qt::AlignVCenter, detailText);

    if (targeted) {
        drawBadge(painter,
                  selectedBadge,
                  QStringLiteral("Cible"),
                  QColor("#12407b"),
                  QColor("#e8f1ff"));
    }

    painter->restore();
}

} // namespace apping
