#include "AppInfo.h"

#include "forma/core/Version.h"
#include "forma/render/GraphicsApi.h"
#include "platform/Platform.h"

#include <QKeySequence>
#include <QQuickWindow>

AppInfo::AppInfo(QObject* parent)
    : QObject(parent)
{
}

QString AppInfo::version() const
{
    const auto v = forma::versionString();
    return QString::fromUtf8(v.data(), static_cast<qsizetype>(v.size()));
}

QString AppInfo::graphicsApi() const
{
    return forma::render::graphicsApiName(QQuickWindow::graphicsApi());
}

int AppInfo::themeOverride() const
{
    const QString theme = qEnvironmentVariable("FORMA_THEME");
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
    return debugBuild() && qEnvironmentVariableIntValue("FORMA_STORYBOOK") == 1;
}

QStringList AppInfo::startupActions() const
{
    return qEnvironmentVariable("FORMA_STARTUP").split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

QString AppInfo::shortcutText(const QString& sequence) const
{
    if (sequence.isEmpty()) {
        return {};
    }
    return QKeySequence(sequence).toString(QKeySequence::NativeText);
}
