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
