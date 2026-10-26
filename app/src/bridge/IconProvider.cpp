#include "IconProvider.h"

#include <QColor>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace {

constexpr int kDefaultSize = 24;

} // namespace

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const qsizetype slash = id.indexOf(QLatin1Char('/'));
    const QString colorHex = slash > 0 ? id.left(slash) : QStringLiteral("000000");
    const QString name = slash > 0 ? id.mid(slash + 1) : id;

    const QSize outSize = requestedSize.isValid() && !requestedSize.isEmpty()
        ? requestedSize
        : QSize(kDefaultSize, kDefaultSize);
    if (size != nullptr) {
        *size = outSize;
    }

    // Opacitatea se aplică separat, ca SVG-ul să primească o culoare simplă #RRGGBB.
    return renderLucideIcon(name, QColor::fromString(QLatin1Char('#') + colorHex), outSize);
}

QImage renderLucideIcon(const QString& name, const QColor& color, QSize size, double strokeWidth)
{
    QFile file(QStringLiteral(":/forma/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Iconiță lipsă: %s", qPrintable(name));
        return {};
    }

    // Lucide desenează cu „currentColor” și linii de 2 unități; le înlocuim pe amândouă.
    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
    svg.replace("stroke-width=\"2\"", QByteArray("stroke-width=\"") + QByteArray::number(strokeWidth) + '"');

    QSvgRenderer renderer(svg);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(color.alphaF());
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    return image;
}
