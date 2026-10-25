#include "forma/render/GraphicsApi.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

#include <cstdlib>

namespace {

// Pentru verificări vizuale automate: FORMA_SCREENSHOT=cale.png salvează fereastra
// după ce s-a randat și închide aplicația. FORMA_THEME=light|dark forțează tema.
void scheduleScreenshotIfRequested(QQmlApplicationEngine& engine)
{
    const QString path = qEnvironmentVariable("FORMA_SCREENSHOT");
    if (path.isEmpty() || engine.rootObjects().isEmpty()) {
        return;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }
    const int delayMs = qEnvironmentVariableIntValue("FORMA_SCREENSHOT_DELAY_MS");
    QTimer::singleShot(delayMs > 0 ? delayMs : 1500, window, [window, path] {
        const bool saved = window->grabWindow().save(path);
        qInfo("Captură %s: %s", saved ? "salvată" : "EȘUATĂ", qPrintable(path));
        QCoreApplication::exit(saved ? EXIT_SUCCESS : EXIT_FAILURE);
    });
}

} // namespace

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

    scheduleScreenshotIfRequested(engine);

    return QGuiApplication::exec();
}
