#include "AppInfo.h"

#include "forma/core/Version.h"
#include "forma/render/GraphicsApi.h"

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
