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
    // Un text de ajutor scris cu tastele de pe Mac (⌘, ⌥, ⇧, „Option”, „Cmd”), adaptat platformei curente.
    Q_INVOKABLE QString platformText(const QString& text) const;

    // Trimite un click real (mișcare, apăsare, eliberare) la poziția dată din item.
    // Folosit de scripturile AION_STARTUP pentru verificarea uneltelor prin calea completă de input.
    Q_INVOKABLE void simulateClick(QQuickItem* item, const QPointF& position) const;
    // Doar mișcarea cursorului (hover), fără click.
    Q_INVOKABLE void simulateMove(QQuickItem* item, const QPointF& position) const;
    // Tragere cu butonul stâng apăsat, în pași mici (pentru verificarea drag & drop-ului).
    Q_INVOKABLE void simulateDrag(QQuickItem* item, const QPointF& from, const QPointF& to, bool release = true) const;
    // Click cu butonul drept (meniul contextual).
    Q_INVOKABLE void simulateRightClick(QQuickItem* item, const QPointF& position) const;
    // Mouse-ul în timp real (verificare de performanță): mișcări la ~120 Hz pe un cerc de rază `radius` px în jurul
    // lui `center`, timp de `seconds`; `mode` = "orbit" (butonul din mijloc), "pan" (Shift + mijloc) sau "hover".
    Q_INVOKABLE void simulateNavigation(QQuickItem* item, const QPointF& center, double radius, double seconds,
        const QString& mode) const;
    // Animația scenelor: cadrele PNG din `directory` → video MP4 la `output` (AVFoundation pe macOS, altfel ffmpeg).
    // Întoarce un text gol la succes (și șterge cadrele), altfel motivul. Fără encoder, cadrele rămân pe disc.
    Q_INVOKABLE QString encodeVideo(const QString& directory, int fps, const QString& output) const;
    // Rotița mouse-ului (pași de 120 = o treaptă; pozitiv = apropiere).
    // Tastele ca de la tastatură (prin harta de scurtături a aplicației, ca în realitate): text + o tastă la final
    // („enter”, „escape”, „up”, „down” sau gol).
    Q_INVOKABLE void simulateKeys(const QString& text, const QString& finalKey) const;
    Q_INVOKABLE void simulateWheel(QQuickItem* item, const QPointF& position, int angleDelta) const;

    // Fișierele cerute din afară (Finder, Dock, linia de comandă) până să le preia interfața.
    static void queueFileOpen(const QString& path);
    Q_INVOKABLE QStringList takePendingFiles();

signals:
    // A sosit un fișier de deschis; interfața îl preia cu takePendingFiles().
    void fileOpenRequested();
};
