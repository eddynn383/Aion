#include "NavigationCursors.h"

#include "IconProvider.h"
#include "aion/render/Viewport.h"

#include <QCursor>
#include <QPointF>
#include <QPainter>
#include <QPixmap>
#include <QQuickWindow>

namespace {

constexpr int kCursorSize = 24; // pixeli logici
constexpr qreal kCursorScale = 2.0; // desenat pentru Retina; Qt îl micșorează pe ecranele obișnuite

// Glif negru cu contur alb, ca la cursoarele de sistem: se vede pe orice fundal.
QCursor makeCursor(const QString& iconName, QPointF hotSpot = QPointF(-1, -1))
{
    const QSize px(qRound(kCursorSize * kCursorScale), qRound(kCursorSize * kCursorScale));
    const QImage halo = renderLucideIcon(iconName, Qt::white, px, 5.0);
    const QImage glyph = renderLucideIcon(iconName, Qt::black, px, 2.0);

    QImage combined(px, QImage::Format_ARGB32_Premultiplied);
    combined.fill(Qt::transparent);
    {
        QPainter p(&combined);
        p.drawImage(0, 0, halo);
        p.drawImage(0, 0, glyph);
    }
    QPixmap pixmap = QPixmap::fromImage(combined);
    pixmap.setDevicePixelRatio(kCursorScale);
    if (hotSpot.x() < 0) {
        return QCursor(pixmap); // punctul activ: centrul
    }
    return QCursor(pixmap, qRound(hotSpot.x() * kCursorSize), qRound(hotSpot.y() * kCursorSize));
}

} // namespace

void installNavigationCursors(QQuickWindow* window)
{
    if (window == nullptr) {
        return;
    }
    const QCursor orbit = makeCursor(QStringLiteral("rotate-3d"));
    const QCursor zoom = makeCursor(QStringLiteral("zoom-in"));
    // Găleata (Paint Bucket): punctul activ e vârful din care „curge” vopseaua (stânga-jos).
    const QCursor paint = makeCursor(QStringLiteral("paint-bucket"), QPointF(0.12, 0.88));
    // Guma: punctul activ e colțul care șterge (stânga-jos).
    const QCursor eraser = makeCursor(QStringLiteral("eraser"), QPointF(0.2, 0.8));
    const auto viewports = window->contentItem()->findChildren<aion::render::Viewport*>();
    for (auto* viewport : viewports) {
        viewport->setNavigationCursors(orbit, zoom);
        viewport->setPaintCursor(paint);
        viewport->setEraserCursor(eraser);
    }
}
