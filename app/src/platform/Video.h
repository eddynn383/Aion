#pragma once

#include <QString>
#include <QStringList>

namespace platform {

// Codează cadrele (PNG-uri de aceeași mărime) într-un video MP4 (H.264) cu encoder-ul sistemului:
// AVFoundation pe macOS. Pe celelalte platforme întoarce false (apelantul încearcă ffmpeg).
bool encodeVideoNative(const QStringList& frames, int fps, const QString& output, QString* error);

} // namespace platform
