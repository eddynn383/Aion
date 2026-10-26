#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Fișierele în browser (Qt for WebAssembly): sistemul de fișiere al aplicației e doar în memorie, deci
// deschiderea trece prin selectorul de fișiere al browserului, iar salvarea printr-un download.
// Documentul lucrează în continuare cu căi: fișierul ales se copiază în memorie (/tmp/...), iar ce se salvează
// acolo se descarcă. Pe desktop, `available` e false și se folosesc dialogurile obișnuite.
class WebFiles : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit WebFiles(QObject* parent = nullptr);

    bool available() const;

    // Deschide selectorul browserului; la alegere emite filePicked(purpose, calea din memorie).
    Q_INVOKABLE void pick(const QString& purpose, const QString& nameFilter);
    // O cale temporară pentru un fișier care urmează să fie descărcat.
    Q_INVOKABLE QString temporaryPath(const QString& fileName) const;
    // Descarcă fișierul de la `path` (din memorie) cu numele dat.
    Q_INVOKABLE bool download(const QString& path, const QString& fileName);

signals:
    void filePicked(const QString& purpose, const QString& path);
};
