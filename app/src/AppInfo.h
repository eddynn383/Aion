#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Informații despre aplicație expuse către QML (versiune, backend grafic).
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString graphicsApi READ graphicsApi CONSTANT)

public:
    explicit AppInfo(QObject* parent = nullptr);

    QString version() const;
    QString graphicsApi() const;
};
