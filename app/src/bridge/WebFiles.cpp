#include "WebFiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WASM
#include <QFileDialog>
#endif

WebFiles::WebFiles(QObject* parent)
    : QObject(parent)
{
}

bool WebFiles::available() const
{
#ifdef Q_OS_WASM
    return true;
#else
    return false;
#endif
}

QString WebFiles::temporaryPath(const QString& fileName) const
{
    const QString dir = QDir::tempPath() + QStringLiteral("/aion");
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + QFileInfo(fileName).fileName();
}

void WebFiles::pick(const QString& purpose, const QString& nameFilter)
{
#ifdef Q_OS_WASM
    QFileDialog::getOpenFileContent(nameFilter, [this, purpose](const QString& name, const QByteArray& content) {
        if (name.isEmpty()) {
            return; // anulat
        }
        const QString path = temporaryPath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly) && file.write(content) == content.size()) {
            file.close();
            emit filePicked(purpose, path);
        }
    });
#else
    Q_UNUSED(purpose)
    Q_UNUSED(nameFilter)
#endif
}

bool WebFiles::download(const QString& path, const QString& fileName)
{
#ifdef Q_OS_WASM
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QFileDialog::saveFileContent(file.readAll(), fileName);
    return true;
#else
    Q_UNUSED(path)
    Q_UNUSED(fileName)
    return false;
#endif
}
