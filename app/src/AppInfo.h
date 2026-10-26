#pragma once

#include <QObject>
#include <QPointF>
#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// Informații despre aplicație expuse către QML (versiune, backend grafic, setări de sistem).
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString graphicsApi READ graphicsApi CONSTANT)
    Q_PROPERTY(int themeOverride READ themeOverride CONSTANT)
    Q_PROPERTY(bool reduceMotion READ reduceMotion CONSTANT)
    Q_PROPERTY(bool debugBuild READ debugBuild CONSTANT)
    Q_PROPERTY(bool storybookOnStart READ storybookOnStart CONSTANT)
    Q_PROPERTY(QStringList startupActions READ startupActions CONSTANT)

public:
    explicit AppInfo(QObject* parent = nullptr);

    QString version() const;
    QString graphicsApi() const;

    // Tema cerută prin AION_THEME (0 = sistem, 1 = light, 2 = dark).
    int themeOverride() const;
    bool reduceMotion() const;
    bool debugBuild() const;
    // AION_STORYBOOK=1 deschide pagina de componente la pornire (pentru capturi automate).
    bool storybookOnStart() const;
    // AION_STARTUP="view.top;palette:zoom" rulează comenzi la pornire (pentru verificări automate).
    QStringList startupActions() const;

    // Textul unei scurtături în forma platformei („Ctrl+K” devine „⌘K” pe macOS).
    Q_INVOKABLE QString shortcutText(const QString& sequence) const;

    // Trimite un click real (mișcare, apăsare, eliberare) la poziția dată din item.
    // Folosit de scripturile AION_STARTUP pentru verificarea uneltelor prin calea completă de input.
    Q_INVOKABLE void simulateClick(QQuickItem* item, const QPointF& position) const;
};
