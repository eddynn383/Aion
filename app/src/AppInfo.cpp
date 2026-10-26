#include "AppInfo.h"

#include "aion/core/Version.h"
#include "aion/render/GraphicsApi.h"
#include "platform/Platform.h"
#include "platform/Video.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QKeySequence>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPointer>
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
        if (ffmpeg.isEmpty()) {
            return error.isEmpty() ? QStringLiteral("Niciun encoder video (instalează ffmpeg); cadrele au rămas în ")
                    + directory
                                   : error;
        }
        const int code = QProcess::execute(ffmpeg,
            { QStringLiteral("-y"), QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-framerate"),
                QString::number(fps), QStringLiteral("-i"), dir.filePath(QStringLiteral("frame_%05d.png")),
                QStringLiteral("-vf"), QStringLiteral("scale=trunc(iw/2)*2:trunc(ih/2)*2"), QStringLiteral("-c:v"),
                QStringLiteral("libx264"), QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), output });
        ok = code == 0;
        if (!ok) {
            return QStringLiteral("ffmpeg a eșuat (cod %1); cadrele au rămas în %2").arg(code).arg(directory);
        }
    }
    dir.removeRecursively();
    return {};
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
