#pragma once

#include <QQuickImageProvider>

// Randează o iconiță Lucide din resurse (:/forma/icons/<nume>.svg) în culoarea și grosimea de linie date.
// Întoarce o imagine goală dacă iconița lipsește.
QImage renderLucideIcon(const QString& name, const QColor& color, QSize size, double strokeWidth = 1.75);

// Iconițele SVG (Lucide) recolorate și randate la mărimea cerută.
// Adresa: image://icon/<culoare RRGGBB sau AARRGGBB>/<nume>, de exemplu image://icon/EDEDED/move.
class IconProvider : public QQuickImageProvider {
public:
    IconProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
