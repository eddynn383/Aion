#include "forma/render/GraphicsApi.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>

int main(int argc, char* argv[])
{
    forma::render::selectPreferredGraphicsApi();

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Forma"));
    QGuiApplication::setOrganizationName(QStringLiteral("Forma"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("forma.dev"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("Forma", "Main");

    return QGuiApplication::exec();
}
