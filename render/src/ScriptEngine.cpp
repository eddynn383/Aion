#include "aion/render/ScriptEngine.h"

#include "aion/core/model/Operations.h"
#include "aion/render/Document.h"

#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <glm/trigonometric.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace aion::render {

namespace {

QVector3D vec(const QVariantList& xyz)
{
    return xyz.size() >= 3 ? QVector3D(xyz[0].toFloat(), xyz[1].toFloat(), xyz[2].toFloat()) : QVector3D();
}

// Exemplele din consolă (și documentația API-ului, prin folosire).
const char* const kSpiral = R"JS(// Scară în spirală: trepte rotite în jurul unei coloane.
const steps = 18, rise = 170, radius = 1000, angle = 20;
aion.cylinder(-120, -120, 0, 240, steps * rise + 200, "Coloană");
for (let i = 0; i < steps; i++) {
    const id = aion.box(0, -150, i * rise, radius, 300, 40, "Treaptă " + (i + 1));
    aion.rotate(id, i * angle, "z", [0, 0, 0]); // în jurul coloanei
    aion.material(id, "Lemn de stejar");
}
aion.log("Gata: " + steps + " trepte.");
)JS";

const char* const kFence = R"JS(// Gard: stâlpi și scânduri pe o lungime dată.
const length = 6000, height = 1200, posts = 7, gap = 30, plank = 120;
const span = length / (posts - 1);
for (let i = 0; i < posts; i++) {
    const id = aion.box(i * span - 50, -50, 0, 100, 100, height + 200, "Stâlp " + (i + 1));
    aion.material(id, "Lemn de nuc");
}
for (let x = 0; x + plank <= length; x += plank + gap) {
    const id = aion.box(x, 60, 150, plank, 20, height, "Scândură");
    aion.material(id, "Lemn de pin");
}
)JS";

const char* const kShelves = R"JS(// Un perete de biblioteci (obiecte dinamice), cu înălțimi diferite.
for (let i = 0; i < 4; i++) {
    aion.object("shelf", { width: 900, height: 1600 + i * 200, shelves: 3 + i }, i * 920, 0, 0);
}
aion.log(aion.objects().length + " obiecte în model.");
)JS";

const char* const kGrid = R"JS(// O grilă de cuburi cu culori diferite.
const n = 5, size = 300, step = 450;
for (let i = 0; i < n; i++)
    for (let j = 0; j < n; j++) {
        const id = aion.box(i * step, j * step, 0, size, size, size * (1 + (i + j) % 3));
        aion.material(id, Qt.hsla((i * n + j) / (n * n), 0.6, 0.55, 1).toString());
    }
)JS";

} // namespace

// --- ScriptApi ---

ScriptApi::ScriptApi(Document* document, QObject* parent)
    : QObject(parent)
    , m_document(document)
{
}

void ScriptApi::log(const QJSValue& value)
{
    emit output(value.isObject() && !value.isCallable()
            ? QString::fromUtf8(QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact))
            : value.toString());
}

void ScriptApi::message(const QString& text)
{
    emit messageRequested(text);
}

double ScriptApi::box(
    double x, double y, double z, double width, double depth, double height, const QString& name)
{
    return m_document
        ? m_document->createPrimitive(QStringLiteral("box"), QVector3D(float(x), float(y), float(z)),
              QVector3D(float(width), float(depth), float(height)), name)
        : 0.0;
}

double ScriptApi::cylinder(double x, double y, double z, double diameter, double height, const QString& name)
{
    return m_document
        ? m_document->createPrimitive(QStringLiteral("cylinder"), QVector3D(float(x), float(y), float(z)),
              QVector3D(float(diameter), float(diameter), float(height)), name)
        : 0.0;
}

double ScriptApi::cone(double x, double y, double z, double diameter, double height, const QString& name)
{
    return m_document
        ? m_document->createPrimitive(QStringLiteral("cone"), QVector3D(float(x), float(y), float(z)),
              QVector3D(float(diameter), float(diameter), float(height)), name)
        : 0.0;
}

double ScriptApi::sphere(double x, double y, double z, double diameter, const QString& name)
{
    return m_document
        ? m_document->createPrimitive(QStringLiteral("sphere"), QVector3D(float(x), float(y), float(z)),
              QVector3D(float(diameter), float(diameter), float(diameter)), name)
        : 0.0;
}

double ScriptApi::object(
    const QString& generator, const QVariantMap& parameters, double x, double y, double z)
{
    return m_document
        ? m_document->createParametricAtCorner(generator, QVector3D(float(x), float(y), float(z)), parameters)
        : 0.0;
}

bool ScriptApi::place(double id, const QVariantMap& placement)
{
    return m_document && m_document->setInstancePlacement(id, placement);
}

bool ScriptApi::move(double id, double dx, double dy, double dz)
{
    return m_document && m_document->transformInstance(id, math::Transform::translation({ dx, dy, dz }));
}

bool ScriptApi::rotate(double id, double degrees, const QString& axis, const QVariantList& pivot)
{
    if (!m_document) {
        return false;
    }
    const model::InstanceId instance { static_cast<std::uint64_t>(id) };
    if (!m_document->model().instances().contains(instance)) {
        return false;
    }
    // În jurul pivotului obiectului (implicit centrul lui).
    math::Vec3d center = model::pivotPoint(m_document->model(), instance);
    if (pivot.size() >= 3) {
        center = { pivot[0].toDouble(), pivot[1].toDouble(), pivot[2].toDouble() };
    }
    const math::Vec3d a = axis == QLatin1String("x") ? math::Vec3d { 1, 0, 0 }
        : axis == QLatin1String("y")                 ? math::Vec3d { 0, 1, 0 }
                                                     : math::Vec3d { 0, 0, 1 };
    return m_document->transformInstance(id, math::Transform::rotation(center, a, glm::radians(degrees)));
}

bool ScriptApi::remove(double id)
{
    return m_document && m_document->removeInstanceById(id);
}

bool ScriptApi::material(double id, const QString& nameOrColor)
{
    return m_document && m_document->setInstanceMaterialByName(id, nameOrColor);
}

bool ScriptApi::line(const QVariantList& from, const QVariantList& to)
{
    return m_document && m_document->drawLineSegment(vec(from), vec(to));
}

bool ScriptApi::face(const QVariantList& points)
{
    return m_document && m_document->drawFace(points);
}

double ScriptApi::board(
    const QString& type, double thickness, double length, double width, double x, double y, double z)
{
    return m_document
        ? m_document->createBoardAt(type, thickness, length, width, QVector3D(float(x), float(y), float(z)))
        : 0.0;
}

bool ScriptApi::setBoardValue(double id, const QString& key, const QVariant& value)
{
    if (!m_document) {
        return false;
    }
    m_document->selectInstances({ id });
    m_document->setBoardValue(key, value);
    return true;
}

bool ScriptApi::setBand(double id, int side, const QString& bandKey, const QString& decor, bool rounded)
{
    if (!m_document) {
        return false;
    }
    m_document->selectInstances({ id });
    m_document->setBand(side, bandKey, decor, rounded);
    return true;
}

QVariantMap ScriptApi::cutList(const QVariantMap& options) const
{
    return m_document ? m_document->cutList(options) : QVariantMap {};
}

QVariantList ScriptApi::objects() const
{
    return m_document ? m_document->objectList() : QVariantList {};
}

QVariantList ScriptApi::selection() const
{
    QVariantList out;
    if (m_document) {
        for (model::InstanceId id : m_document->selection().instances) {
            out.append(static_cast<double>(id.value));
        }
    }
    return out;
}

void ScriptApi::select(const QVariantList& ids)
{
    if (m_document) {
        m_document->selectInstances(ids);
    }
}

void ScriptApi::command(const QString& id)
{
    emit commandRequested(id);
}

// --- ScriptEngine ---

ScriptEngine::ScriptEngine(QObject* parent)
    : QObject(parent)
{
    reloadPlugins();
}

void ScriptEngine::setDocument(Document* document)
{
    if (m_document == document) {
        return;
    }
    m_document = document;
    emit documentChanged();
}

QVariantList ScriptEngine::examples() const
{
    return { QVariantMap { { QStringLiteral("name"), QStringLiteral("Scară în spirală") },
                 { QStringLiteral("code"), QString::fromUtf8(kSpiral) } },
        QVariantMap { { QStringLiteral("name"), QStringLiteral("Gard") },
            { QStringLiteral("code"), QString::fromUtf8(kFence) } },
        QVariantMap { { QStringLiteral("name"), QStringLiteral("Perete de biblioteci") },
            { QStringLiteral("code"), QString::fromUtf8(kShelves) } },
        QVariantMap { { QStringLiteral("name"), QStringLiteral("Grilă de cuburi colorate") },
            { QStringLiteral("code"), QString::fromUtf8(kGrid) } } };
}

QVariantMap ScriptEngine::run(const QString& code, const QString& name)
{
    QVariantMap result { { QStringLiteral("ok"), false }, { QStringLiteral("output"), QString() },
        { QStringLiteral("error"), QString() }, { QStringLiteral("line"), 0 } };
    if (!m_document) {
        result[QStringLiteral("error")] = QStringLiteral("Niciun document.");
        return result;
    }
    QJSEngine engine;
    engine.installExtensions(QJSEngine::ConsoleExtension);
    ScriptApi api(m_document);
    QStringList output;
    connect(&api, &ScriptApi::output, this, [&output](const QString& text) { output.append(text); });
    connect(&api, &ScriptApi::messageRequested, this, &ScriptEngine::messageRequested);
    connect(&api, &ScriptApi::commandRequested, this, &ScriptEngine::commandRequested);
    engine.globalObject().setProperty(QStringLiteral("aion"), engine.newQObject(&api));
    // print(...) = aion.log(...); Qt.hsla și altele din obiectul Qt nu există în QJSEngine: le dăm pe cele
    // utile.
    engine.evaluate(
        QStringLiteral("var print = function() { aion.log(Array.prototype.join.call(arguments, ' ')); };"
                       "var Qt = { hsla: function(h, s, l, a) { "
                       "  const f = function(n) { const k = (n + h * 12) % 12; const v = l - s * Math.min(l, "
                       "1 - l) * Math.max(-1, Math.min(k - 3, 9 - k, 1));"
                       "    return ('0' + Math.round(v * 255).toString(16)).slice(-2); };"
                       "  return { toString: function() { return '#' + f(0) + f(8) + f(4); } }; } };"));

    // Limita de timp: un script blocat (buclă infinită) se oprește după 15 s.
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    std::thread watchdog([&] {
        std::unique_lock lock(mutex);
        if (!finished.wait_for(lock, std::chrono::seconds(15), [&] { return done; })) {
            engine.setInterrupted(true);
        }
    });

    m_document->beginMacro(name.toStdString());
    const QJSValue value = engine.evaluate(code, name);
    {
        std::lock_guard lock(mutex);
        done = true;
    }
    finished.notify_all();
    watchdog.join();

    const bool ok = !value.isError() && !engine.hasError() && !engine.isInterrupted();
    m_document->endMacro(ok);
    if (!ok) {
        const QString message = engine.isInterrupted()
            ? QStringLiteral("Scriptul a fost oprit (a durat peste 15 s).")
            : value.property(QStringLiteral("message")).toString();
        result[QStringLiteral("error")] = message.isEmpty() ? value.toString() : message;
        result[QStringLiteral("line")] = value.property(QStringLiteral("lineNumber")).toInt();
    } else if (!value.isUndefined()) {
        output.append(value.toString());
    }
    result[QStringLiteral("ok")] = ok;
    result[QStringLiteral("output")] = output.join(QLatin1Char('\n'));
    return result;
}

QString ScriptEngine::pluginFolder() const
{
    const QString dir
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Plugins");
    QDir().mkpath(dir);
    return dir;
}

void ScriptEngine::reloadPlugins()
{
    m_plugins.clear();
    const QDir root(pluginFolder());
    for (const QString& entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QFile manifest(root.filePath(entry + QStringLiteral("/manifest.json")));
        if (!manifest.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject m = QJsonDocument::fromJson(manifest.readAll()).object();
        QVariantList commands;
        for (const QJsonValue& c : m.value(QStringLiteral("commands")).toArray()) {
            const QJsonObject command = c.toObject();
            commands.append(QVariantMap {
                { QStringLiteral("id"),
                    entry + QLatin1Char('/') + command.value(QStringLiteral("function")).toString() },
                { QStringLiteral("title"), command.value(QStringLiteral("title")).toString() } });
        }
        m_plugins.append(QVariantMap { { QStringLiteral("id"), entry },
            { QStringLiteral("name"), m.value(QStringLiteral("name")).toString(entry) },
            { QStringLiteral("version"), m.value(QStringLiteral("version")).toString() },
            { QStringLiteral("description"), m.value(QStringLiteral("description")).toString() },
            { QStringLiteral("commands"), commands } });
    }
    emit pluginsChanged();
}

QVariantMap ScriptEngine::runPluginCommand(const QString& commandId)
{
    const QString plugin = commandId.section(QLatin1Char('/'), 0, 0);
    const QString function = commandId.section(QLatin1Char('/'), 1);
    QFile file(QDir(pluginFolder()).filePath(plugin + QStringLiteral("/main.js")));
    if (!file.open(QIODevice::ReadOnly)) {
        return { { QStringLiteral("ok"), false },
            { QStringLiteral("error"), QStringLiteral("Plugin-ul nu are main.js.") } };
    }
    QString title = function;
    for (const QVariant& p : m_plugins) {
        for (const QVariant& c : p.toMap().value(QStringLiteral("commands")).toList()) {
            if (c.toMap().value(QStringLiteral("id")).toString() == commandId) {
                title = c.toMap().value(QStringLiteral("title")).toString();
            }
        }
    }
    return run(
        QString::fromUtf8(file.readAll()) + QStringLiteral("\n;") + function + QStringLiteral("();"), title);
}

} // namespace aion::render
