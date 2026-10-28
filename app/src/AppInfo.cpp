#include "AppInfo.h"

#include "aion/core/Version.h"
#include "aion/render/GraphicsApi.h"
#include "platform/Platform.h"
#include "platform/Video.h"

#include <QCoreApplication>
#include <QDir>
#if QT_CONFIG(process)
#include <QProcess>
#endif
#include <QStandardPaths>
#include <QKeySequence>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPointer>
#include <QGuiApplication>
#include <QHash>
#include <qpa/qwindowsysteminterface.h>
#include <QQuickWindow>

#include <utility>

namespace {

QStringList& pendingFiles()
{
    static QStringList files;
    return files;
}

QPointer<AppInfo>& instance()
{
    static QPointer<AppInfo> appInfo;
    return appInfo;
}

} // namespace

AppInfo::AppInfo(QObject* parent)
    : QObject(parent)
{
    instance() = this;
}

void AppInfo::queueFileOpen(const QString& path)
{
    pendingFiles().append(path);
    if (instance()) {
        emit instance()->fileOpenRequested();
    }
}

QStringList AppInfo::takePendingFiles()
{
    return std::exchange(pendingFiles(), {});
}

QString AppInfo::version() const
{
    const auto v = aion::versionString();
    return QString::fromUtf8(v.data(), static_cast<qsizetype>(v.size()));
}

QString AppInfo::graphicsApi() const
{
    return aion::render::graphicsApiName(QQuickWindow::graphicsApi());
}

int AppInfo::themeOverride() const
{
    const QString theme = qEnvironmentVariable("AION_THEME");
    if (theme == QLatin1String("light")) {
        return 1;
    }
    if (theme == QLatin1String("dark")) {
        return 2;
    }
    return 0;
}

bool AppInfo::reduceMotion() const
{
    return platform::prefersReducedMotion();
}

bool AppInfo::debugBuild() const
{
#ifdef QT_DEBUG
    return true;
#else
    return false;
#endif
}

bool AppInfo::storybookOnStart() const
{
    return debugBuild() && qEnvironmentVariableIntValue("AION_STORYBOOK") == 1;
}

QStringList AppInfo::startupActions() const
{
    return qEnvironmentVariable("AION_STARTUP").split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

QString AppInfo::shortcutText(const QString& sequence) const
{
    if (sequence.isEmpty()) {
        return {};
    }
    return QKeySequence(sequence).toString(QKeySequence::NativeText);
}

QString AppInfo::platformText(const QString& text) const
{
#ifdef Q_OS_MACOS
    return text;
#else
    QString result = text;
    result.replace(QStringLiteral("Option (⌥)"), QStringLiteral("Alt"))
        .replace(QStringLiteral("⇧⌘"), QStringLiteral("Ctrl+Shift+"))
        .replace(QStringLiteral("⌘"), QStringLiteral("Ctrl+"))
        .replace(QStringLiteral("⌥"), QStringLiteral("Alt"))
        .replace(QStringLiteral("⇧"), QStringLiteral("Shift"))
        .replace(QStringLiteral("Option"), QStringLiteral("Alt"))
        .replace(QStringLiteral("Cmd"), QStringLiteral("Ctrl"));
    return result;
#endif
}

QString AppInfo::encodeVideo(const QString& directory, int fps, const QString& output) const
{
    QDir dir(directory);
    QStringList frames = dir.entryList({ QStringLiteral("frame_*.png") }, QDir::Files, QDir::Name);
    if (frames.isEmpty()) {
        return QStringLiteral("Nu există cadre de codat.");
    }
    for (QString& f : frames) {
        f = dir.filePath(f);
    }
    QString error;
    bool ok = platform::encodeVideoNative(frames, fps, output, &error);
    if (!ok) {
        // ffmpeg, dacă e instalat (în PATH sau în locurile obișnuite pentru Homebrew).
        QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (ffmpeg.isEmpty()) {
            ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"),
                { QDir::homePath() + QStringLiteral("/homebrew/bin"), QStringLiteral("/opt/homebrew/bin"),
                    QStringLiteral("/usr/local/bin") });
        }
#if !QT_CONFIG(process)
        ffmpeg.clear(); // în browser nu se pot porni procese
#endif
        if (ffmpeg.isEmpty()) {
            return error.isEmpty() ? QStringLiteral("Niciun encoder video (instalează ffmpeg); cadrele au rămas în ")
                    + directory
                                   : error;
        }
#if QT_CONFIG(process)
        const int code = QProcess::execute(ffmpeg,
            { QStringLiteral("-y"), QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-framerate"),
                QString::number(fps), QStringLiteral("-i"), dir.filePath(QStringLiteral("frame_%05d.png")),
                QStringLiteral("-vf"), QStringLiteral("scale=trunc(iw/2)*2:trunc(ih/2)*2"), QStringLiteral("-c:v"),
                QStringLiteral("libx264"), QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), output });
        ok = code == 0;
        if (!ok) {
            return QStringLiteral("ffmpeg a eșuat (cod %1); cadrele au rămas în %2").arg(code).arg(directory);
        }
#endif
    }
    dir.removeRecursively();
    return {};
}

void AppInfo::simulateKeys(const QString& text, const QString& finalKey) const
{
    QWindow* window = QGuiApplication::focusWindow();
    if (window == nullptr) {
        const auto windows = QGuiApplication::topLevelWindows();
        window = windows.isEmpty() ? nullptr : windows.front();
    }
    if (window == nullptr) {
        return;
    }
    // Scurtăturile (QShortcut) lucrează doar în fereastra activă; pornită din terminal, aplicația poate fi în spate.
    if (QGuiApplication::focusWindow() != window) {
        QWindowSystemInterface::handleFocusWindowChanged<QWindowSystemInterface::SynchronousDelivery>(window);
    }
    const auto press = [&](int key, const QString& t, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        // Pe macOS plugin-ul Cocoa întreabă singur harta scurtăturilor; un eveniment injectat trebuie s-o întrebe.
        if (QWindowSystemInterface::handleShortcutEvent(window, 0, key, mods, 0, 0, 0, t)) {
            return;
        }
        QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(window, QEvent::KeyPress, key, mods, t);
        QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(window, QEvent::KeyRelease, key, mods, t);
    };
    for (const QChar c : text) {
        press(c.isDigit() ? Qt::Key_0 + c.digitValue() : c == QLatin1Char('.') ? Qt::Key_Period : c.toUpper().unicode(), QString(c));
    }
    // Tasta finală, cu modificatori opționali: „enter”, „alt+right”, „shift+left”, „space”, „home”.
    QStringList parts = finalKey.toLower().split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return;
    }
    const QString name = parts.takeLast();
    Qt::KeyboardModifiers mods;
    for (const QString& m : parts) {
        mods |= m == QLatin1String("shift") ? Qt::ShiftModifier
            : m == QLatin1String("alt")     ? Qt::AltModifier
            : m == QLatin1String("ctrl")    ? Qt::ControlModifier
                                            : Qt::NoModifier;
    }
    static const QHash<QString, std::pair<int, QString>> kKeys {
        { QStringLiteral("enter"), { Qt::Key_Return, QStringLiteral("\r") } },
        { QStringLiteral("escape"), { Qt::Key_Escape, {} } },
        { QStringLiteral("up"), { Qt::Key_Up, {} } },
        { QStringLiteral("down"), { Qt::Key_Down, {} } },
        { QStringLiteral("left"), { Qt::Key_Left, {} } },
        { QStringLiteral("right"), { Qt::Key_Right, {} } },
        { QStringLiteral("home"), { Qt::Key_Home, {} } },
        { QStringLiteral("end"), { Qt::Key_End, {} } },
        { QStringLiteral("space"), { Qt::Key_Space, QStringLiteral(" ") } },
        { QStringLiteral("delete"), { Qt::Key_Delete, {} } },
        { QStringLiteral("backspace"), { Qt::Key_Backspace, {} } },
    };
    if (const auto it = kKeys.find(name); it != kKeys.end()) {
        press(it->first, it->second, mods);
    } else if (name.size() == 1) {
        press(name.at(0).toUpper().unicode(), mods & ~Qt::ShiftModifier ? QString() : name, mods);
    }
}

void AppInfo::simulateWheel(QQuickItem* item, const QPointF& position, int angleDelta) const
{
    if (item == nullptr || item->window() == nullptr) {
        return;
    }
    QQuickWindow* window = item->window();
    const QPointF scenePos = item->mapToScene(position);
    QWheelEvent e(scenePos, window->mapToGlobal(scenePos), QPoint(), QPoint(0, angleDelta), Qt::NoButton, Qt::NoModifier,
        Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(window, &e);
}

void AppInfo::simulateClick(QQuickItem* item, const QPointF& position) const
{
    if (item == nullptr || item->window() == nullptr) {
        return;
    }
    QQuickWindow* window = item->window();
    const QPointF scenePos = item->mapToScene(position);
    const QPointF globalPos = window->mapToGlobal(scenePos);
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent e(type, scenePos, scenePos, globalPos, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &e);
    };
    send(QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
    send(QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, Qt::LeftButton, Qt::NoButton);
}

void AppInfo::simulateMove(QQuickItem* item, const QPointF& position) const
{
    if (item == nullptr || item->window() == nullptr) {
        return;
    }
    QQuickWindow* window = item->window();
    const QPointF scenePos = item->mapToScene(position);
    QMouseEvent e(QEvent::MouseMove, scenePos, scenePos, window->mapToGlobal(scenePos), Qt::NoButton, Qt::NoButton,
        Qt::NoModifier);
    QCoreApplication::sendEvent(window, &e);
}

void AppInfo::simulateDrag(QQuickItem* item, const QPointF& from, const QPointF& to, bool release) const
{
    if (item == nullptr || item->window() == nullptr) {
        return;
    }
    QQuickWindow* window = item->window();
    const auto send = [&](QEvent::Type type, const QPointF& position, Qt::MouseButton button, Qt::MouseButtons buttons) {
        const QPointF scenePos = item->mapToScene(position);
        QMouseEvent e(type, scenePos, scenePos, window->mapToGlobal(scenePos), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &e);
        QCoreApplication::processEvents();
    };
    send(QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
    send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    constexpr int steps = 20;
    for (int i = 1; i <= steps; ++i) {
        send(QEvent::MouseMove, from + (to - from) * (static_cast<double>(i) / steps), Qt::NoButton, Qt::LeftButton);
    }
    if (release) {
        send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    }
}

void AppInfo::simulateRightClick(QQuickItem* item, const QPointF& position) const
{
    if (item == nullptr || item->window() == nullptr) {
        return;
    }
    QQuickWindow* window = item->window();
    const QPointF scenePos = item->mapToScene(position);
    const QPointF globalPos = window->mapToGlobal(scenePos);
    const auto send = [&](QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent e(type, scenePos, scenePos, globalPos, button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &e);
    };
    send(QEvent::MouseMove, Qt::NoButton, Qt::NoButton);
    send(QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton);
    send(QEvent::MouseButtonRelease, Qt::RightButton, Qt::NoButton);
}
