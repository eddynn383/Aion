#pragma once

#include <QQuickImageProvider>

// Iconițele SVG (Lucide) recolorate și randate la mărimea cerută.
// Adresa: image://icon/<culoare RRGGBB sau AARRGGBB>/<nume>, de exemplu image://icon/EDEDED/move.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
