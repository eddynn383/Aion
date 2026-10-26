// Randează iconița aplicației din SVG în PNG-urile unui .iconset (macOS) sau într-un singur PNG.
//   aion_iconrender <mare.svg> <mic.svg> <folder.iconset>
//   aion_iconrender <fișier.svg> <dimensiune> <ieșire.png>
// Dimensiunile de 16 și 32 px folosesc varianta „mică” (linii mai groase).
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QDir>
#include <cstdio>

namespace {

bool render(const QString& svg, int size, const QString& out)
{
    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        std::fprintf(stderr, "SVG invalid: %s\n", qPrintable(svg));
        return false;
    }
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    painter.end();
    if (!image.save(out)) {
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
