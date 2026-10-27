#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

// Texturile procedurale ale bibliotecii de materiale, desenate la cerere (fără imagini binare în repo):
// „brick”, „parquet”, „wood”, „concrete”, „tiles”. Imaginile sunt repetabile (marginile se potrivesc).
namespace aion::render {

[[nodiscard]] QImage proceduralTexture(const QString& key, int size = 1024);
[[nodiscard]] QByteArray proceduralTexturePng(const QString& key);

} // namespace aion::render
