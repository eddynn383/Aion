#include "bridge/IconProvider.h"
#include "bridge/NavigationCursors.h"
#include "forma/render/GraphicsApi.h"
#include "platform/Platform.h"

#include <QDirIterator>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QTimer>

#include <cstdlib>

Q_IMPORT_QML_PLUGIN(Forma_RenderPlugin)

namespace {

// Inter (UI) și Geist Mono (valori numerice) sunt incluse în aplicație.
void loadFonts()
{
    QDirIterator it(QStringLiteral(":/forma/fonts"), { QStringLiteral("*.ttf") });
    while (it.hasNext()) {
        if (QFontDatabase::addApplicationFont(it.next()) < 0) {
            qWarning("Fontul %s nu s-a putut încărca", qPrintable(it.filePath()));
        }
    }
    QFont ui(QStringLiteral("Inter"));
    ui.setPixelSize(13);
    ui.setHintingPreference(QFont::PreferNoHinting);
    QGuiApplication::setFont(ui);
}

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

    loadFonts();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider); // engine-ul preia obiectul
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("Forma", "Main");

    if (!engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        // Trebuie să corespundă cu Theme.titleBarHeight.
        constexpr int kTitleBarHeight = 44;
        platform::integrateTitleBar(window, kTitleBarHeight);
        installNavigationCursors(window);
    }

    scheduleScreenshotIfRequested(engine);

    return QGuiApplication::exec();
}
