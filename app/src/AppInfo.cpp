#include "AppInfo.h"

#include "aion/core/Version.h"
#include "aion/render/GraphicsApi.h"
#include "platform/Platform.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QMouseEvent>
#include <QQuickWindow>

AppInfo::AppInfo(QObject* parent)
    : QObject(parent)
{
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
