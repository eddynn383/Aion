// Randează iconița aplicației din SVG în PNG-urile unui .iconset (macOS), într-un .ico (Windows) sau într-un PNG.
//   aion_iconrender <mare.svg> <mic.svg> <folder.iconset>
//   aion_iconrender <mare.svg> <mic.svg> <Aion.ico>
//   aion_iconrender <fișier.svg> <dimensiune> <ieșire.png>
// Dimensiunile de 16 și 32 px folosesc varianta „mică” (linii mai groase).
#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QDir>
#include <cstdio>

namespace {

QImage rasterize(const QString& svg, int size)
{
    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        std::fprintf(stderr, "SVG invalid: %s\n", qPrintable(svg));
        return {};
    }
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();
    return image;
}

bool render(const QString& svg, int size, const QString& out)
{
    const QImage image = rasterize(svg, size);
    if (image.isNull() || !image.save(out)) {
        std::fprintf(stderr, "Nu pot scrie %s\n", qPrintable(out));
        return false;
    }
    return true;
}

void putLe16(QByteArray& bytes, int value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
}

void putLe32(QByteArray& bytes, qint64 value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.append(static_cast<char>((value >> shift) & 0xff));
    }
}

// Un .ico cu imagini PNG (acceptat de Windows Vista și mai noi): antet, câte o intrare de director, apoi datele.
bool writeIco(const QString& big, const QString& small, const QString& out)
{
    const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
    QList<QByteArray> images;
    for (int px : sizes) {
        const QImage image = rasterize(px <= 32 ? small : big, px);
        if (image.isNull()) {
            return false;
        }
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        images.append(png);
    }
    QByteArray ico;
    putLe16(ico, 0);
    putLe16(ico, 1); // tip: iconiță
    putLe16(ico, static_cast<int>(images.size()));
    qint64 offset = 6 + 16 * images.size();
    for (qsizetype i = 0; i < images.size(); ++i) {
        const int px = sizes[i];
        ico.append(static_cast<char>(px >= 256 ? 0 : px)); // 0 înseamnă 256
        ico.append(static_cast<char>(px >= 256 ? 0 : px));
        ico.append('\0'); // fără paletă
        ico.append('\0');
        putLe16(ico, 1); // planuri
        putLe16(ico, 32); // biți per pixel
        putLe32(ico, images[i].size());
        putLe32(ico, offset);
        offset += images[i].size();
    }
    for (const QByteArray& png : images) {
        ico.append(png);
    }
    QFile file(out);
    if (!file.open(QIODevice::WriteOnly) || file.write(ico) != ico.size()) {
        std::fprintf(stderr, "Nu pot scrie %s\n", qPrintable(out));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 4) {
        std::fprintf(stderr, "Folosire: %s <mare.svg> <mic.svg> <folder.iconset> | <fișier.svg> <px> <ieșire.png>\n", argv[0]);
        return 2;
    }
    const QString a = QString::fromLocal8Bit(argv[1]);
    const QString b = QString::fromLocal8Bit(argv[2]);
    const QString c = QString::fromLocal8Bit(argv[3]);
    bool ok = false;
    const int single = b.toInt(&ok);
    if (ok) {
        return render(a, single, c) ? 0 : 1;
    }

    if (c.endsWith(QLatin1String(".ico"), Qt::CaseInsensitive)) {
        return writeIco(a, b, c) ? 0 : 1;
    }

    QDir().mkpath(c);
    struct Entry {
        const char* name;
        int px;
    };
    const Entry entries[] = {
        { "icon_16x16.png", 16 }, { "icon_16x16@2x.png", 32 }, { "icon_32x32.png", 32 },
        { "icon_32x32@2x.png", 64 }, { "icon_128x128.png", 128 }, { "icon_128x128@2x.png", 256 },
        { "icon_256x256.png", 256 }, { "icon_256x256@2x.png", 512 }, { "icon_512x512.png", 512 },
        { "icon_512x512@2x.png", 1024 },
    };
    for (const Entry& e : entries) {
        const QString& svg = e.px <= 32 ? b : a;
        if (!render(svg, e.px, c + QLatin1Char('/') + QLatin1String(e.name))) {
            return 1;
        }
    }
    return 0;
}
