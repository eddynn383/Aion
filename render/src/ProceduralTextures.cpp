#include "ProceduralTextures.h"

#include <QBuffer>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>

#include <cmath>

namespace aion::render {

namespace {

// Zgomot fin (granulație), deterministic, peste toată imaginea.
void addGrain(QImage& image, quint32 seed, int strength)
{
    QRandomGenerator random(seed);
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int d = static_cast<int>(random.bounded(2 * strength + 1)) - strength;
            const QRgb c = line[x];
            line[x] = qRgb(std::clamp(qRed(c) + d, 0, 255), std::clamp(qGreen(c) + d, 0, 255), std::clamp(qBlue(c) + d, 0, 255));
        }
    }
}

QColor jitter(QRandomGenerator& random, const QColor& base, int amount)
{
    const auto d = [&] { return static_cast<int>(random.bounded(2 * amount + 1)) - amount; };
    return QColor(std::clamp(base.red() + d(), 0, 255), std::clamp(base.green() + d(), 0, 255),
        std::clamp(base.blue() + d(), 0, 255));
}

// Cărămizi 250 × 65 mm cu rost de 10 mm, în legătură (rândurile decalate cu o jumătate): 1 m × 1 m.
QImage brick(int size)
{
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(QColor(196, 188, 172)); // mortarul
    QPainter p(&image);
    p.setPen(Qt::NoPen);
    QRandomGenerator random(7);
    const double k = size / 1000.0;
    const double rowHeight = 75.0;
    const int rows = static_cast<int>(std::round(1000.0 / rowHeight));
    const double h = 1000.0 / rows;
    for (int r = 0; r < rows; ++r) {
        const double offset = (r % 2) * 130.0;
        for (double x = -offset; x < 1000.0; x += 260.0) {
            p.setBrush(jitter(random, QColor(160, 74, 52), 18));
            const QRectF rect((x + 5) * k, (r * h + 5) * k, 250 * k, (h - 10) * k);
            p.drawRect(rect);
            if (rect.right() > size) { // repetabil: partea care iese în dreapta apare în stânga
                p.drawRect(rect.translated(-size, 0));
            }
        }
    }
    p.end();
    addGrain(image, 11, 10);
    return image;
}

// Parchet: lamele de 1200 × 100 mm, cu capetele decalate și nuanțe diferite (1,2 m × 1,2 m).
QImage parquet(int size)
{
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(QColor(120, 84, 50));
    QPainter p(&image);
    QRandomGenerator random(3);
    const double k = size / 1200.0;
    for (int r = 0; r < 12; ++r) {
        const double shift = random.bounded(1200.0);
        for (int part = -1; part < 1; ++part) {
            const QRectF rect((shift + part * 1200.0) * k, r * 100.0 * k, 1200.0 * k - 1, 100.0 * k - 1);
            const QColor base = jitter(random, QColor(178, 132, 84), 16);
            QLinearGradient g(rect.topLeft(), rect.bottomLeft());
            g.setColorAt(0, base.lighter(106));
            g.setColorAt(1, base.darker(106));
            p.fillRect(rect, g);
            // Fibra: linii fine de-a lungul lamelei.
            p.setPen(QPen(base.darker(118), 1.0));
            for (int f = 0; f < 6; ++f) {
                const double y = rect.top() + rect.height() * (0.15 + 0.14 * f) + random.bounded(3.0);
                QPainterPath path(QPointF(rect.left(), y));
                for (double x = rect.left(); x <= rect.right(); x += 40.0) {
                    path.lineTo(x, y + std::sin(x * 0.01 + f) * 2.0);
                }
                p.drawPath(path);
            }
        }
    }
    p.end();
    addGrain(image, 5, 6);
    return image;
}

// Lemn masiv: fibră continuă pe verticală, cu noduri rare (0,8 m × 0,8 m).
QImage wood(int size)
{
    QImage image(size, size, QImage::Format_RGB32);
    QRandomGenerator random(9);
    const double phase = random.bounded(6.28);
    for (int y = 0; y < size; ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < size; ++x) {
            const double u = static_cast<double>(x) / size;
            const double v = static_cast<double>(y) / size;
            // Inele: un sinus al lui u deformat lin de v (perioade întregi, ca imaginea să se repete).
            const double w = std::sin((u * 18.0 + 0.6 * std::sin(v * 6.2832 * 2 + phase)) * 6.2832);
            const double t = 0.5 + 0.5 * w;
            const int r = static_cast<int>(150 + 40 * t);
            const int g = static_cast<int>(106 + 30 * t);
            const int b = static_cast<int>(66 + 18 * t);
            line[x] = qRgb(r, g, b);
        }
    }
    addGrain(image, 13, 5);
    return image;
}

// Beton: gri cu pete moi și pori mici (2 m × 2 m).
QImage concrete(int size)
{
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(QColor(168, 166, 160));
    QPainter p(&image);
    p.setPen(Qt::NoPen);
    QRandomGenerator random(21);
    for (int i = 0; i < 260; ++i) {
        const QPointF c(random.bounded(size * 1.0), random.bounded(size * 1.0));
        const double radius = 20 + random.bounded(110.0);
        QRadialGradient g(c, radius);
        const QColor tone = random.bounded(2) ? QColor(150, 148, 142, 40) : QColor(186, 184, 178, 40);
        g.setColorAt(0, tone);
        g.setColorAt(1, QColor(tone.red(), tone.green(), tone.blue(), 0));
        p.setBrush(g);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                p.drawEllipse(c + QPointF(dx * size, dy * size), radius, radius);
            }
        }
    }
    p.setBrush(QColor(110, 108, 104));
    for (int i = 0; i < 900; ++i) {
        const double r = 0.6 + random.bounded(1.6);
        p.drawEllipse(QPointF(random.bounded(size * 1.0), random.bounded(size * 1.0)), r, r);
    }
    p.end();
    addGrain(image, 17, 8);
    return image;
}

// Gresie 300 × 300 mm cu rost de 3 mm (4 × 4 plăci = 1,2 m).
QImage tiles(int size)
{
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(QColor(150, 146, 140)); // rostul
    QPainter p(&image);
    p.setPen(Qt::NoPen);
    QRandomGenerator random(4);
    const double k = size / 1200.0;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            const QRectF rect((c * 300 + 1.5) * k, (r * 300 + 1.5) * k, 297 * k, 297 * k);
            const QColor base = jitter(random, QColor(222, 218, 210), 6);
            QLinearGradient g(rect.topLeft(), rect.bottomRight());
            g.setColorAt(0, base.lighter(103));
            g.setColorAt(1, base.darker(103));
            p.setBrush(g);
            p.drawRect(rect);
        }
    }
    p.end();
    addGrain(image, 23, 4);
    return image;
}

} // namespace

QImage proceduralTexture(const QString& key, int size)
{
    if (key == QLatin1String("brick")) return brick(size);
    if (key == QLatin1String("parquet")) return parquet(size);
    if (key == QLatin1String("wood")) return wood(size);
    if (key == QLatin1String("concrete")) return concrete(size);
    if (key == QLatin1String("tiles")) return tiles(size);
    return {};
}

QByteArray proceduralTexturePng(const QString& key)
{
    const QImage image = proceduralTexture(key);
    if (image.isNull()) {
        return {};
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

} // namespace aion::render
