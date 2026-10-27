#include "aion/render/Document.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/inference/Inference.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/io/Exchange.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/MaterialLibrary.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Interaction.h"
#include "aion/core/model/Modeling.h"
#include "aion/core/model/Parametric.h"
#include "aion/core/model/Transfer.h"
#include "ProceduralTextures.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Quantities.h"
#include "aion/core/units/Length.h"
#include "aion/core/view/ModelPicking.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/Picking.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <span>
#include <utility>
#include <limits>
#include <set>

namespace aion::render {

namespace {

// Căile vin din Qt ca UTF-16; pe Windows, std::string ar fi interpretat în codepage-ul local (diacriticele s-ar pierde).
std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

// Copia de recuperare, pe web (unde fișierele nu persistă).
const QString kRecoveryDataKey = QStringLiteral("recovery/data");


QString plural(std::size_t n, const char* one, const char* many)
{
    return n == 1 ? QStringLiteral("1 %1").arg(QString::fromUtf8(one))
                  : QStringLiteral("%1 %2").arg(n).arg(QString::fromUtf8(many));
}

model::InstanceId toInstanceId(double id)
{
    return model::InstanceId { static_cast<std::uint64_t>(id) };
}

model::TagId toTagId(double id)
{
    return model::TagId { static_cast<std::uint64_t>(id) };
}

// Culorile tag-urilor noi, pe rând.
const model::Color kTagPalette[] = {
    { 0x3B, 0x82, 0xF6, 255 }, { 0xF5, 0x9E, 0x0B, 255 }, { 0x10, 0xB9, 0x81, 255 }, { 0xEC, 0x48, 0x99, 255 },
    { 0x8B, 0x5C, 0xF6, 255 }, { 0x14, 0xB8, 0xA6, 255 }, { 0xEF, 0x44, 0x44, 255 }, { 0x84, 0xCC, 0x16, 255 },
};

QString formatArea(double mm2)
{
    if (mm2 >= 1e6) {
        return QString::number(mm2 / 1e6, 'f', 2) + QStringLiteral(" m²");
    }
    return QString::number(mm2, 'f', 0) + QStringLiteral(" mm²");
}

} // namespace

Document::Document(QObject* parent)
    : QObject(parent)
{
    m_model.addObserver(this);
    {
        QSettings settings;
        units::DisplaySettings display;
        display.unit = static_cast<units::Unit>(std::clamp(settings.value(QStringLiteral("units/unit"), 0).toInt(), 0, 5));
        display.decimals = std::clamp(settings.value(QStringLiteral("units/decimals"), -1).toInt(), -1, 6);
        units::setDisplaySettings(display);
    }
    m_stack.setOnChanged([this] { emit historyChanged(); });
    // În debug, un model invalid oprește aplicația (CommandStack); în release nu se verifică.
    retriangulate(m_model.root());
    setActiveTool(QStringLiteral("select"));

    // Autosalvare: la fiecare minut, dacă documentul are modificări nesalvate.
#ifdef Q_OS_WASM
    // În browser, fișierele sunt doar în memorie: copia de recuperare stă în QSettings (IndexedDB / localStorage).
    {
        const QByteArray data = QSettings().value(kRecoveryDataKey).toByteArray();
        QFile file(autosavePath());
        if (!data.isEmpty() && !file.exists() && file.open(QIODevice::WriteOnly)) {
            file.write(data);
        }
    }
#endif
    m_hasRecovery = QFile::exists(autosavePath());
    {
        QSettings settings;
        m_autosaveTimer.setInterval(std::clamp(settings.value(QStringLiteral("files/autosaveMinutes"), 1).toInt(), 1, 60) * 60'000);
        connect(&m_autosaveTimer, &QTimer::timeout, this, &Document::autosave);
        if (settings.value(QStringLiteral("files/autosave"), true).toBool()) {
            m_autosaveTimer.start();
        }
    }
}

Document::~Document()
{
    // O închidere normală fără modificări nesalvate nu lasă nimic de recuperat.
    if (!modified() && !m_hasRecovery) {
        removeAutosave();
    }
    m_model.removeObserver(this);
}

// --- Autosalvare (Preferințe) ---

void Document::setAutosaveEnabled(bool enabled)
{
    if (enabled == m_autosaveTimer.isActive()) {
        return;
    }
    if (enabled) {
        m_autosaveTimer.start();
    } else {
        m_autosaveTimer.stop();
    }
    QSettings().setValue(QStringLiteral("files/autosave"), enabled);
    emit autosaveSettingsChanged();
}

void Document::setAutosaveMinutes(int minutes)
{
    minutes = std::clamp(minutes, 1, 60);
    if (minutes == autosaveMinutes()) {
        return;
    }
    const bool active = m_autosaveTimer.isActive();
    m_autosaveTimer.setInterval(minutes * 60'000);
    if (active) {
        m_autosaveTimer.start();
    }
    QSettings().setValue(QStringLiteral("files/autosaveMinutes"), minutes);
    emit autosaveSettingsChanged();
}

// --- Preferințe de afișare ---

int Document::displayUnit() const
{
    return static_cast<int>(units::displaySettings().unit);
}

void Document::setDisplayUnit(int unit)
{
    units::DisplaySettings display = units::displaySettings();
    const auto value = static_cast<units::Unit>(std::clamp(unit, 0, 5));
    if (display.unit == value) {
        return;
    }
    display.unit = value;
    units::setDisplaySettings(display);
    QSettings().setValue(QStringLiteral("units/unit"), unit);
    emit unitsChanged();
    emit selectionChanged(); // Entity Info
    emit toolStatusChanged(); // Measurements box
    emit changed(); // cotele din viewport
}

int Document::displayDecimals() const
{
    return units::displaySettings().decimals;
}

void Document::setDisplayDecimals(int decimals)
{
    units::DisplaySettings display = units::displaySettings();
    const int value = std::clamp(decimals, -1, 6);
    if (display.decimals == value) {
        return;
    }
    display.decimals = value;
    units::setDisplaySettings(display);
    QSettings().setValue(QStringLiteral("units/decimals"), value);
    emit unitsChanged();
    emit selectionChanged();
    emit toolStatusChanged();
    emit changed();
}

namespace {

model::CutListOptions cutListOptions(const QVariantMap& options)
{
    model::CutListOptions o;
    o.sheetLength = options.value(QStringLiteral("sheetLength"), o.sheetLength).toDouble();
    o.sheetWidth = options.value(QStringLiteral("sheetWidth"), o.sheetWidth).toDouble();
    o.waste = options.value(QStringLiteral("waste"), o.waste).toDouble();
    o.maxThickness = options.value(QStringLiteral("maxThickness"), o.maxThickness).toDouble();
    return o;
}

} // namespace

QVariantMap Document::quantityReport() const
{
    const model::QuantityReport r = model::quantityReport(m_model);
    QVariantList objects;
    for (const model::ObjectQuantity& o : r.objects) {
        objects.append(QVariantMap {
            { QStringLiteral("name"), QString::fromStdString(o.name) },
            { QStringLiteral("component"), o.component },
            { QStringLiteral("count"), o.count },
            { QStringLiteral("area"), o.area / 1e6 },
            { QStringLiteral("volume"), o.volume / 1e9 },
            { QStringLiteral("solid"), o.solid },
            { QStringLiteral("sizeX"), o.size.x },
            { QStringLiteral("sizeY"), o.size.y },
            { QStringLiteral("sizeZ"), o.size.z },
            { QStringLiteral("material"), QString::fromStdString(o.material) },
            { QStringLiteral("tag"), QString::fromStdString(o.tag) },
        });
    }
    const auto named = [](const std::vector<model::NamedArea>& list) {
        QVariantList out;
        for (const model::NamedArea& n : list) {
            out.append(QVariantMap { { QStringLiteral("name"), QString::fromStdString(n.name) },
                { QStringLiteral("area"), n.area / 1e6 } });
        }
        return out;
    };
    return {
        { QStringLiteral("objects"), objects },
        { QStringLiteral("materials"), named(r.materials) },
        { QStringLiteral("tags"), named(r.tags) },
        { QStringLiteral("totalArea"), r.totalArea / 1e6 },
        { QStringLiteral("totalVolume"), r.totalVolume / 1e9 },
        { QStringLiteral("horizontalArea"), r.horizontalArea / 1e6 },
        { QStringLiteral("faceCount"), static_cast<int>(r.faceCount) },
        { QStringLiteral("edgeLength"), r.edgeLength / 1000.0 },
    };
}

QVariantMap Document::cutList(const QVariantMap& options) const
{
    const model::CutList list = model::cutList(m_model, cutListOptions(options));
    QVariantList parts;
    for (const model::CutPart& p : list.parts) {
        parts.append(QVariantMap {
            { QStringLiteral("name"), QString::fromStdString(p.name) },
            { QStringLiteral("material"), QString::fromStdString(p.material) },
            { QStringLiteral("length"), p.length },
            { QStringLiteral("width"), p.width },
            { QStringLiteral("thickness"), p.thickness },
            { QStringLiteral("count"), p.count },
            { QStringLiteral("panel"), p.panel },
            { QStringLiteral("flat"), p.flat },
        });
    }
    QVariantList sheets;
    for (const model::SheetNeed& s : list.sheets) {
        sheets.append(QVariantMap {
            { QStringLiteral("material"), QString::fromStdString(s.material) },
            { QStringLiteral("thickness"), s.thickness },
            { QStringLiteral("area"), s.partsArea / 1e6 },
            { QStringLiteral("sheets"), s.sheets },
        });
    }
    return { { QStringLiteral("parts"), parts }, { QStringLiteral("sheets"), sheets },
        { QStringLiteral("edgeBanding"), list.edgeBanding / 1000.0 } };
}

bool Document::exportReport(const QString& path, const QString& kind, const QVariantMap& options)
{
    const std::string csv = kind == QLatin1String("cutlist")
        ? model::toCsv(model::cutList(m_model, cutListOptions(options)), cutListOptions(options))
        : model::toCsv(model::quantityReport(m_model));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(csv.data(), static_cast<qint64>(csv.size())) != static_cast<qint64>(csv.size())) {
        setError(QStringLiteral("Raportul nu s-a putut salva în „%1”.").arg(QFileInfo(path).fileName()));
        return false;
    }
    return true;
}

QString Document::formatLength(double mm) const
{
    return QString::fromStdString(units::formatLength(mm));
}

// --- Fișiere ---

namespace {

constexpr int kMaxRecentFiles = 8;
const QString kRecentKey = QStringLiteral("files/recent");
const QString kRecoverySourceKey = QStringLiteral("recovery/source");
const QString kRecoveryTimeKey = QStringLiteral("recovery/time");

} // namespace

QString Document::autosavePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/autosave.aion");
}

QString Document::fileName() const
{
    return m_filePath.isEmpty() ? QStringLiteral("Fără titlu") : QFileInfo(m_filePath).completeBaseName();
}

QStringList Document::recentFiles() const
{
    QStringList files = QSettings().value(kRecentKey).toStringList();
    files.erase(std::remove_if(files.begin(), files.end(), [](const QString& f) { return !QFile::exists(f); }), files.end());
    return files;
}

QString Document::recoveryInfo() const
{
    QSettings settings;
    const QString source = settings.value(kRecoverySourceKey).toString();
    const QDateTime time = settings.value(kRecoveryTimeKey).toDateTime();
    const QString when = time.isValid() ? time.toString(QStringLiteral("d MMM, HH:mm")) : QString();
    const QString name = source.isEmpty() ? QStringLiteral("un document nesalvat") : QFileInfo(source).fileName();
    return when.isEmpty() ? name : name + QStringLiteral(" (") + when + QStringLiteral(")");
}

void Document::setFilePath(const QString& path)
{
    m_filePath = path;
    emit fileChanged();
}

void Document::addRecentFile(const QString& path)
{
    QSettings settings;
    QStringList files = settings.value(kRecentKey).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    settings.setValue(kRecentKey, files);
    emit recentFilesChanged();
}

void Document::clearRecentFiles()
{
    QSettings().remove(kRecentKey);
    emit recentFilesChanged();
}

void Document::setError(const QString& message)
{
    m_lastError = message;
    emit errorChanged();
}

QString Document::localPath(const QUrl& url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

void Document::replaceModel(const model::Model& model)
{
    if (m_tool) {
        const view::Camera camera;
        tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, camera };
        m_tool->cancel(ctx);
    }
    m_path.clear();
    m_selection.clear();
    m_model = model;
    m_stack.setActiveDefinition(m_model.root());
    m_stack.clear();
    m_stack.markClean();
    m_triangles.clear();
    for (model::DefinitionId d : m_model.definitions().keys()) {
        retriangulate(d);
    }
    updateHiddenTags();
    emit contextChanged();
    emit selectionChanged();
    emit outlineChanged();
    emit modelChanged();
    emit historyChanged();
    emit documentReplaced();
    touch();
}

void Document::newDocument()
{
    replaceModel(model::Model {});
    setFilePath({});
    removeAutosave();
}

bool Document::openFile(const QString& path)
{
    const io::LoadResult result = io::loadFile(toPath(path));
    if (!result.model) {
        setError(QStringLiteral("„%1” nu a putut fi deschis. %2")
                     .arg(QFileInfo(path).fileName(), QString::fromStdString(result.error)));
        return false;
    }
    replaceModel(*result.model);
    setFilePath(path);
    addRecentFile(path);
    removeAutosave();
    return true;
}

bool Document::saveFile(const QString& path, const QByteArray& thumbnailPng)
{
    const QString target = path.isEmpty() ? m_filePath : path;
    if (target.isEmpty()) {
        setError(QStringLiteral("Alege unde salvezi documentul."));
        return false;
    }
    std::string error;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(thumbnailPng.constData());
    if (!io::saveFile(toPath(target), m_model,
            std::span(bytes, static_cast<std::size_t>(thumbnailPng.size())), &error)) {
        setError(QStringLiteral("„%1” nu a putut fi salvat. %2")
                     .arg(QFileInfo(target).fileName(), QString::fromStdString(error)));
        return false;
    }
    m_stack.markClean();
    setFilePath(target);
    addRecentFile(target);
    removeAutosave();
    return true;
}

bool Document::importFile(const QString& path)
{
    const QFileInfo info(path);
    const QString ext = info.suffix().toLower();
    io::ImportResult result;
    const auto run = [&](const std::function<io::ImportResult(model::Model&)>& import) {
        const geom::Selection before = m_selection;
        m_stack.run("Importă " + info.fileName().toStdString(), [&](model::Model& model) { result = import(model); });
        m_selection.clear();
        if (result.instance && m_model.instances().contains(result.instance)) {
            m_selection.instances.insert(result.instance);
        }
        afterInteraction(before);
        emit outlineChanged();
    };
    if (ext == QLatin1String("obj")) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(QStringLiteral("„%1” nu poate fi citit.").arg(info.fileName()));
            return false;
        }
        const QByteArray text = file.readAll();
        run([&](model::Model& model) {
            return io::importObj(model, activeDefinition(), std::string_view(text.constData(), std::size_t(text.size())),
                info.completeBaseName().toStdString());
        });
    } else if (ext == QLatin1String("gltf") || ext == QLatin1String("glb")) {
        run([&](model::Model& model) {
            return io::importGltf(model, activeDefinition(), toPath(path));
        });
    } else {
        setError(QStringLiteral("Formatul „.%1” nu se poate importa (OBJ, glTF, GLB).").arg(ext));
        return false;
    }
    if (!result.instance) {
        setError(QStringLiteral("„%1” nu a putut fi importat. %2")
                     .arg(info.fileName(), QString::fromStdString(result.error)));
        return false;
    }
    return true;
}

bool Document::exportFile(const QString& path)
{
    const QFileInfo info(path);
    const QString ext = info.suffix().toLower();
    std::string error;
    bool ok = false;
    if (ext == QLatin1String("obj")) {
        const QString mtlName = info.completeBaseName() + QStringLiteral(".mtl");
        QFile obj(path);
        QFile mtl(info.dir().filePath(mtlName));
        ok = obj.open(QIODevice::WriteOnly | QIODevice::Truncate) && mtl.open(QIODevice::WriteOnly | QIODevice::Truncate);
        if (ok) {
            const std::string objText = io::exportObj(m_model, {}, mtlName.toStdString());
            const std::string mtlText = io::exportMtl(m_model);
            ok = obj.write(objText.data(), qint64(objText.size())) == qint64(objText.size())
                && mtl.write(mtlText.data(), qint64(mtlText.size())) == qint64(mtlText.size());
            // Imaginile texturilor, lângă .mtl (map_Kd le caută acolo).
            for (const auto& [name, bytes] : io::exportTextureFiles(m_model)) {
                QFile image(info.dir().filePath(QString::fromStdString(name)));
                ok = ok && image.open(QIODevice::WriteOnly | QIODevice::Truncate)
                    && image.write(reinterpret_cast<const char*>(bytes.data()), qint64(bytes.size())) == qint64(bytes.size());
            }
        }
    } else if (ext == QLatin1String("gltf") || ext == QLatin1String("glb")) {
        ok = io::exportGltf(m_model, toPath(path), ext == QLatin1String("glb"), {}, &error);
    } else {
        setError(QStringLiteral("Formatul „.%1” nu se poate exporta (OBJ, glTF, GLB).").arg(ext));
        return false;
    }
    if (!ok) {
        setError(QStringLiteral("„%1” nu a putut fi exportat. %2").arg(info.fileName(), QString::fromStdString(error)));
    }
    return ok;
}

void Document::autosave()
{
    if (!modified()) {
        return;
    }
    if (io::saveFile(toPath(autosavePath()), m_model)) {
        QSettings settings;
        settings.setValue(kRecoverySourceKey, m_filePath);
        settings.setValue(kRecoveryTimeKey, QDateTime::currentDateTime());
#ifdef Q_OS_WASM
        QFile file(autosavePath());
        if (file.open(QIODevice::ReadOnly)) {
            settings.setValue(kRecoveryDataKey, file.readAll());
        }
#endif
    }
}

void Document::removeAutosave()
{
    QFile::remove(autosavePath());
    QSettings settings;
    settings.remove(kRecoverySourceKey);
    settings.remove(kRecoveryTimeKey);
    settings.remove(kRecoveryDataKey);
    if (m_hasRecovery) {
        m_hasRecovery = false;
        emit recoveryChanged();
    }
}

void Document::recoverAutosave()
{
    const QString source = QSettings().value(kRecoverySourceKey).toString();
    const io::LoadResult result = io::loadFile(toPath(autosavePath()));
    if (!result.model) {
        setError(QStringLiteral("Sesiunea nu a putut fi recuperată. %1").arg(QString::fromStdString(result.error)));
        discardAutosave();
        return;
    }
    replaceModel(*result.model);
    setFilePath(source);
    // Recuperat, dar nesalvat: documentul rămâne marcat ca modificat.
    m_stack.markDirty();
    m_hasRecovery = false;
    emit recoveryChanged();
    emit historyChanged();
}

void Document::discardAutosave()
{
    removeAutosave();
}

void Document::touch()
{
    ++m_revision;
    emit changed();
}

std::size_t Document::validPathLength() const
{
    // În mijlocul unui undo, calea poate indica pentru scurt timp instanțe dispărute (până la repairContext()).
    std::size_t valid = 0;
    model::DefinitionId parent = m_model.root();
    for (; valid < m_path.size(); ++valid) {
        const model::Instance* inst = m_model.instances().find(m_path[valid]);
        if (inst == nullptr || inst->parent != parent) {
            break;
        }
        parent = inst->definition;
    }
    return valid;
}

model::DefinitionId Document::activeDefinition() const
{
    const std::size_t valid = validPathLength();
    return valid == 0 ? m_model.root() : m_model.instance(m_path[valid - 1]).definition;
}

math::Transform Document::contextTransform() const
{
    const std::size_t valid = validPathLength();
    if (valid == m_path.size()) {
        return model::worldTransform(m_model, m_path);
    }
    return model::worldTransform(m_model, { m_path.begin(), m_path.begin() + static_cast<std::ptrdiff_t>(valid) });
}

const std::unordered_map<geom::FaceId, geom::FaceTriangles>& Document::triangles(model::DefinitionId definition) const
{
    static const std::unordered_map<geom::FaceId, geom::FaceTriangles> kEmpty;
    const auto it = m_triangles.find(definition);
    return it != m_triangles.end() ? it->second : kEmpty;
}

void Document::setActiveTool(const QString& id)
{
    if (id == m_toolId) {
        return;
    }
    if (m_tool) {
        // Unealta veche nu are nevoie de cameră ca să renunțe; îi dăm una oarecare.
        const view::Camera camera;
        tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, camera };
        m_tool->cancel(ctx);
    }
    m_toolId = id;
    m_tool = tools::makeTool(id.toStdString());
    emit activeToolChanged();
    emit toolStatusChanged();
    touch();
}

QString Document::toolStatus() const
{
    if (m_runtime) {
        return QStringLiteral("Modul interacțiune · click, hover, trage sau apasă taste pe obiecte · Esc: ieșire");
    }
    if (!m_tool) {
        return {};
    }
    QString status = QString::fromStdString(m_tool->status());
#ifndef Q_OS_MACOS
    // Textele din core folosesc numele tastelor de pe Mac; pe Windows și Linux: Alt și Ctrl.
    status.replace(QStringLiteral("Option"), QStringLiteral("Alt")).replace(QStringLiteral("Cmd"), QStringLiteral("Ctrl"));
#endif
    return status;
}

QString Document::undoName() const
{
    return QString::fromStdString(m_stack.undoName());
}

QString Document::redoName() const
{
    return QString::fromStdString(m_stack.redoName());
}

QString Document::selectionSummary() const
{
    if (m_selection.empty()) {
        return {};
    }
    QStringList parts;
    std::size_t groups = 0;
    for (model::InstanceId id : m_selection.instances) {
        const model::Instance* inst = m_model.instances().find(id);
        groups += inst != nullptr && m_model.definition(inst->definition).group ? 1 : 0;
    }
    if (groups > 0) {
        parts << plural(groups, "grup", "grupuri");
    }
    if (m_selection.instances.size() > groups) {
        parts << plural(m_selection.instances.size() - groups, "obiect", "obiecte");
    }
    if (!m_selection.faces.empty()) {
        parts << plural(m_selection.faces.size(), "față", "fețe");
    }
    if (!m_selection.edges.empty()) {
        parts << plural(m_selection.edges.size(), "muchie", "muchii");
    }
    const std::size_t total = m_selection.faces.size() + m_selection.edges.size() + m_selection.instances.size();
    const bool onlyObjects = m_selection.faces.empty() && m_selection.edges.empty();
    const QString adjective = total == 1 ? (onlyObjects ? QStringLiteral(" selectat") : QStringLiteral(" selectată"))
                                         : QStringLiteral(" selectate");
    return parts.join(QStringLiteral(", ")) + adjective;
}

bool Document::hasMeasurement() const
{
    return m_tool && m_tool->measurement().has_value();
}

QString Document::measurementLabel() const
{
    const auto m = m_tool ? m_tool->measurement() : std::nullopt;
    return m ? QString::fromStdString(m->label) : QString();
}

QString Document::measurementValue() const
{
    const auto m = m_tool ? m_tool->measurement() : std::nullopt;
    return m ? QString::fromStdString(m->value) : QString();
}

int Document::faceCount() const
{
    std::size_t n = 0;
    for (model::DefinitionId d : m_model.definitions().keys()) {
        n += m_model.mesh(d).faces().size();
    }
    return static_cast<int>(n);
}

int Document::edgeCount() const
{
    std::size_t n = 0;
    for (model::DefinitionId d : m_model.definitions().keys()) {
        n += m_model.mesh(d).edges().size();
    }
    return static_cast<int>(n);
}

QStringList Document::contextPath() const
{
    QStringList out;
    for (model::InstanceId id : m_path) {
        const model::Instance& inst = m_model.instance(id);
        out << QString::fromStdString(inst.name.empty() ? m_model.definition(inst.definition).name : inst.name);
    }
    return out;
}

QVariantList Document::outline() const
{
    QVariantList out;
    const std::set<model::InstanceId> current(m_path.begin(), m_path.end());
    const auto visit = [&](auto&& self, model::DefinitionId parent, int depth) -> void {
        for (model::InstanceId id : m_model.childrenOf(parent)) {
            const model::Instance& inst = m_model.instance(id);
            const model::Definition& def = m_model.definition(inst.definition);
            out.append(QVariantMap {
                { QStringLiteral("id"), static_cast<double>(id.value) },
                { QStringLiteral("name"), QString::fromStdString(inst.name.empty() ? def.name : inst.name) },
                { QStringLiteral("definition"), QString::fromStdString(def.name) },
                { QStringLiteral("kind"), def.collection ? QStringLiteral("group") : QStringLiteral("object") },
                { QStringLiteral("shape"), QString::fromStdString(def.shape) },
                { QStringLiteral("copies"), static_cast<int>(m_model.instancesOf(inst.definition).size()) },
                { QStringLiteral("parentDefinition"), static_cast<double>(inst.parent.value) },
                { QStringLiteral("hasChildren"), !m_model.childrenOf(inst.definition).empty() },
                { QStringLiteral("tagColor"), inst.tag && m_model.tags().contains(inst.tag)
                        ? QVariant(QColor(m_model.tag(inst.tag).color.r, m_model.tag(inst.tag).color.g,
                              m_model.tag(inst.tag).color.b))
                        : QVariant() },
                { QStringLiteral("depth"), depth },
                { QStringLiteral("hidden"), inst.hidden },
                { QStringLiteral("locked"), inst.locked },
                { QStringLiteral("selected"), m_selection.instances.contains(id) },
                { QStringLiteral("current"), current.contains(id) },
            });
            self(self, inst.definition, depth + 1);
        }
    };
    visit(visit, m_model.root(), 0);
    return out;
}

QVariantList Document::tags() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.tags().size(); ++i) {
        const model::TagId id = m_model.tags().keys()[i];
        const model::Tag& tag = m_model.tags().values()[i];
        int count = 0;
        for (const model::Instance& inst : m_model.instances().values()) {
            count += inst.tag == id ? 1 : 0;
        }
        for (model::DefinitionId d : m_model.definitions().keys()) {
            const geom::Mesh& mesh = m_model.mesh(d);
            for (const geom::Face& f : mesh.faces().values()) {
                count += f.tag == id ? 1 : 0;
            }
            for (const geom::Edge& e : mesh.edges().values()) {
                count += e.tag == id ? 1 : 0;
            }
        }
        out.append(QVariantMap {
            { QStringLiteral("id"), static_cast<double>(id.value) },
            { QStringLiteral("name"), QString::fromStdString(tag.name) },
            { QStringLiteral("color"), QColor(tag.color.r, tag.color.g, tag.color.b, tag.color.a) },
            { QStringLiteral("visible"), tag.visible },
            { QStringLiteral("count"), count },
        });
    }
    return out;
}

namespace {
QVariantMap materialEntry(const model::Material& m); // mai jos, la materiale
} // namespace

QVariantMap Document::materialInfo(model::MaterialId id) const
{
    if (!id || !m_model.materials().contains(id)) {
        return { { QStringLiteral("id"), 0.0 }, { QStringLiteral("name"), QStringLiteral("Implicit") } };
    }
    QVariantMap entry = materialEntry(m_model.material(id));
    entry[QStringLiteral("id")] = static_cast<double>(id.value);
    return entry;
}

QVariantMap Document::entityInfo() const
{
    QVariantMap info;
    const auto detail = [](const QString& label, const QString& value) {
        return QVariantMap { { QStringLiteral("label"), label }, { QStringLiteral("value"), value } };
    };
    if (m_selection.empty()) {
        info[QStringLiteral("kind")] = QStringLiteral("none");
        return info;
    }
    const geom::Mesh& mesh = m_model.mesh(activeDefinition());
    QVariantList details;
    if (m_selection.instances.size() == 1 && m_selection.faces.empty() && m_selection.edges.empty()
        && m_model.instances().contains(*m_selection.instances.begin())) {
        const model::InstanceId id = *m_selection.instances.begin();
        const model::Instance& inst = m_model.instance(id);
        const model::Definition& def = m_model.definition(inst.definition);
        info[QStringLiteral("kind")] = QStringLiteral("instance");
        info[QStringLiteral("title")] = def.collection ? QStringLiteral("Grup") : QStringLiteral("Obiect");
        info[QStringLiteral("instanceId")] = static_cast<double>(id.value);
        info[QStringLiteral("name")] = QString::fromStdString(def.group ? inst.name : def.name);
        info[QStringLiteral("tagId")] = static_cast<double>(inst.tag.value);
        info[QStringLiteral("locked")] = inst.locked;
        info[QStringLiteral("copies")] = static_cast<int>(m_model.instancesOf(inst.definition).size());
        if (!def.group) {
            details << detail(QStringLiteral("Copii"), QString::number(m_model.instancesOf(inst.definition).size()));
        }
        // Dimensiunile și materialul au controale proprii în Inspector (editabile).
        const model::Placement placement = model::placementOf(m_model, id);
        info[QStringLiteral("placement")] = QVariantMap {
            { QStringLiteral("px"), placement.position.x },
            { QStringLiteral("py"), placement.position.y },
            { QStringLiteral("pz"), placement.position.z },
            { QStringLiteral("rx"), placement.rotation.x },
            { QStringLiteral("ry"), placement.rotation.y },
            { QStringLiteral("rz"), placement.rotation.z },
            { QStringLiteral("sx"), placement.scale.x },
            { QStringLiteral("sy"), placement.scale.y },
            { QStringLiteral("sz"), placement.scale.z },
            { QStringLiteral("dx"), placement.size.x },
            { QStringLiteral("dy"), placement.size.y },
            { QStringLiteral("dz"), placement.size.z },
        };
        info[QStringLiteral("material")] = materialInfo(inst.material);
        const model::Behavior& b = inst.behavior;
        info[QStringLiteral("behavior")] = QVariantMap {
            { QStringLiteral("kind"),
                b.kind == model::Behavior::Kind::Rotate ? QStringLiteral("rotate")
                    : b.kind == model::Behavior::Kind::Slide ? QStringLiteral("slide")
                                                             : QStringLiteral("none") },
            { QStringLiteral("anchor"), static_cast<int>(b.anchor) },
            { QStringLiteral("amount"), b.amount },
            { QStringLiteral("duration"), b.duration },
            { QStringLiteral("open"), b.open },
        };
        info[QStringLiteral("ruleCount")] = static_cast<int>(inst.rules.size());
        info[QStringLiteral("locks")] = static_cast<int>(inst.placementLocks);
        // Obiect dinamic: parametrii lui, cu limitele generatorului.
        const model::Definition& definition = m_model.definition(inst.definition);
        if (const model::GeneratorInfo* g = model::findGenerator(definition.generator)) {
            QVariantList parameters;
            for (const model::ParameterSpec& spec : g->parameters) {
                const auto it = definition.parameters.find(spec.key);
                parameters.append(QVariantMap { { QStringLiteral("key"), QString::fromStdString(spec.key) },
                    { QStringLiteral("label"), QString::fromStdString(spec.label) },
                    { QStringLiteral("unit"), QString::fromStdString(spec.unit) },
                    { QStringLiteral("value"), it != definition.parameters.end() ? it->second : spec.value },
                    { QStringLiteral("min"), spec.min }, { QStringLiteral("max"), spec.max },
                    { QStringLiteral("step"), spec.step } });
            }
            info[QStringLiteral("parametric")] = QVariantMap { { QStringLiteral("generator"), QString::fromStdString(g->id) },
                { QStringLiteral("name"), QString::fromStdString(g->name) },
                { QStringLiteral("copies"), static_cast<int>(m_model.instancesOf(inst.definition).size()) },
                { QStringLiteral("parameters"), parameters } };
        }
    } else if (m_selection.instances.empty() && m_selection.edges.empty()) {
        double area = 0.0;
        const auto& tris = triangles(activeDefinition());
        for (geom::FaceId f : m_selection.faces) {
            const auto it = tris.find(f);
            if (it == tris.end()) {
                continue;
            }
            const geom::FaceTriangles& t = it->second;
            for (std::size_t i = 0; i + 2 < t.indices.size(); i += 3) {
                const auto& a = t.points[t.indices[i]];
                area += 0.5 * glm::length(glm::cross(t.points[t.indices[i + 1]] - a, t.points[t.indices[i + 2]] - a));
            }
        }
        // Materialele de pe față și verso: comune tuturor fețelor selectate sau „mixt”.
        std::set<model::MaterialId> fronts;
        std::set<model::MaterialId> backs;
        for (geom::FaceId f : m_selection.faces) {
            if (const geom::Face* face = mesh.faces().find(f)) {
                fronts.insert(face->material);
                backs.insert(face->backMaterial);
            }
        }
        const auto sideInfo = [&](const std::set<model::MaterialId>& ids) {
            if (ids.size() == 1) {
                return materialInfo(*ids.begin());
            }
            return QVariantMap { { QStringLiteral("id"), -1.0 }, { QStringLiteral("name"), QStringLiteral("Mixt") } };
        };
        std::set<model::TagId> faceTags;
        for (geom::FaceId f : m_selection.faces) {
            if (const geom::Face* face = mesh.faces().find(f)) {
                faceTags.insert(face->tag);
            }
        }
        info[QStringLiteral("tagId")] = faceTags.size() == 1 ? static_cast<double>(faceTags.begin()->value) : -1.0;
        info[QStringLiteral("frontMaterial")] = sideInfo(fronts);
        info[QStringLiteral("backMaterial")] = sideInfo(backs);
        // Poziționarea texturii (a primei fețe), dacă vreuna are o textură (material propriu sau al obiectului).
        for (geom::FaceId f : m_selection.faces) {
            const geom::Face* face = mesh.faces().find(f);
            const model::Material* material = face && face->material ? m_model.materials().find(face->material) : nullptr;
            if (material != nullptr && material->texture) {
                const geom::TextureTransform& t = face->texture;
                info[QStringLiteral("texture")] = QVariantMap { { QStringLiteral("ox"), t.offset.x },
                    { QStringLiteral("oy"), t.offset.y }, { QStringLiteral("rotation"), t.rotation },
                    { QStringLiteral("sx"), t.scale.x }, { QStringLiteral("sy"), t.scale.y } };
                break;
            }
        }
        info[QStringLiteral("kind")] = QStringLiteral("faces");
        info[QStringLiteral("title")]
            = m_selection.faces.size() == 1 ? QStringLiteral("Față") : plural(m_selection.faces.size(), "față", "fețe");
        details << detail(QStringLiteral("Arie"), formatArea(area));
    } else if (m_selection.instances.empty() && m_selection.faces.empty()) {
        double length = 0.0;
        std::set<model::TagId> edgeTags;
        for (geom::EdgeId e : m_selection.edges) {
            if (const geom::Edge* edge = mesh.edges().find(e)) {
                length += glm::length(mesh.position(edge->v1) - mesh.position(edge->v0));
                edgeTags.insert(edge->tag);
            }
        }
        info[QStringLiteral("tagId")] = edgeTags.size() == 1 ? static_cast<double>(edgeTags.begin()->value) : -1.0;
        info[QStringLiteral("kind")] = QStringLiteral("edges");
        info[QStringLiteral("title")] = m_selection.edges.size() == 1
            ? QStringLiteral("Muchie")
            : plural(m_selection.edges.size(), "muchie", "muchii");
        details << detail(QStringLiteral("Lungime"), QString::fromStdString(units::formatLength(length)));
    } else {
        info[QStringLiteral("kind")] = QStringLiteral("mixed");
        info[QStringLiteral("title")] = selectionSummary();
    }
    info[QStringLiteral("details")] = details;
    return info;
}

template <class Fn>
void Document::withTool(const view::Camera& camera, Fn&& fn)
{
    if (!m_tool) {
        return;
    }
    const geom::Selection before = m_selection;
    // În grupul editat, uneltele lucrează în coordonatele lui (camera face conversia).
    const view::Camera local = m_path.empty() ? camera : camera.inContext(contextTransform());
    tools::ToolRequests requests;
    tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, local };
    ctx.model = &m_model;
    ctx.context = activeDefinition();
    ctx.requests = &requests;
    ctx.paintMaterial = m_currentMaterial;
    ctx.toWorld = contextTransform();
    m_contextGuides.clear();
    for (const model::Guide& g : m_model.guides().values()) {
        if (g.context == activeDefinition()) {
            m_contextGuides.push_back({ g.point, g.direction });
        }
    }
    ctx.guides = m_contextGuides;
    if (m_snapToGrid) {
        ctx.gridStep = m_gridStep > 0.0 ? m_gridStep : inference::autoGridStep(camera.pose().distance);
    }
    ctx.libraryMaterial
        = m_libraryMaterial >= 0 ? &model::builtinMaterials()[static_cast<std::size_t>(m_libraryMaterial)] : nullptr;
    fn(*m_tool, ctx);
    // Paint Bucket: un material din bibliotecă a intrat în model, sau pipeta a ales unul.
    if (requests.addedMaterial) {
        m_currentMaterial = requests.addedMaterial;
        m_libraryMaterial = -1;
        emit materialChanged();
    }
    if (requests.sampledMaterial) {
        m_currentMaterial = *requests.sampledMaterial;
        m_libraryMaterial = -1;
        emit materialChanged();
    }
    if (requests.editAnnotation) {
        m_editingAnnotation = requests.editAnnotation;
        emit editingAnnotationChanged();
    }
    if (requests.enter && m_model.instances().contains(requests.enter)) {
        enterInstance(static_cast<double>(requests.enter.value));
    } else if (requests.exit) {
        exitContext();
    }
    afterInteraction(before);
}

void Document::afterInteraction(const geom::Selection& before)
{
    repairContext();
    if (!m_selection.empty()) {
        m_selectedCamera = {};
        m_selectedLight = {};
    }
    if (m_selection != before) {
        emit selectionChanged();
        emit outlineChanged();
    }
    emit toolStatusChanged();
    touch();
}

void Document::repairContext()
{
    if (std::exchange(m_contextLost, false) && m_tool) {
        const view::Camera camera;
        tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, camera };
        m_tool->cancel(ctx);
    }
    // Calea se oprește la prima instanță care nu mai există sau nu mai e în definiția de deasupra.
    std::size_t valid = 0;
    model::DefinitionId parent = m_model.root();
    for (; valid < m_path.size(); ++valid) {
        const model::Instance* inst = m_model.instances().find(m_path[valid]);
        if (inst == nullptr || inst->parent != parent) {
            break;
        }
        parent = inst->definition;
    }
    if (valid < m_path.size()) {
        setPath({ m_path.begin(), m_path.begin() + static_cast<std::ptrdiff_t>(valid) });
    }
    m_selection.prune(m_model.mesh(activeDefinition()));
    const model::DefinitionId active = activeDefinition();
    std::erase_if(m_selection.instances, [&](model::InstanceId id) {
        const model::Instance* inst = m_model.instances().find(id);
        return inst == nullptr || inst->parent != active;
    });
}

void Document::updateHiddenTags()
{
    std::set<model::TagId> hidden;
    for (std::size_t i = 0; i < m_model.tags().size(); ++i) {
        if (!m_model.tags().values()[i].visible) {
            hidden.insert(m_model.tags().keys()[i]);
        }
    }
    view::setHiddenTags(std::move(hidden));
}

void Document::dropStaleReferences()
{
    if (!m_model.cameras().contains(m_selectedCamera)) {
        m_selectedCamera = {};
    }
    if (!m_model.lights().contains(m_selectedLight)) {
        m_selectedLight = {};
    }
    const std::size_t valid = validPathLength();
    if (valid < m_path.size()) {
        // Contextul editat a dispărut: ieșim din el fără să atingem unealta (poate fi chiar ea în lucru);
        // repairContext() o anulează după operație.
        m_path.resize(valid);
        m_stack.setActiveDefinition(activeDefinition());
        m_selection.clear();
        m_contextLost = true;
        emit contextChanged();
        return;
    }
    m_selection.prune(m_model.mesh(activeDefinition()));
    const model::DefinitionId active = activeDefinition();
    std::erase_if(m_selection.instances, [&](model::InstanceId id) {
        const model::Instance* inst = m_model.instances().find(id);
        return inst == nullptr || inst->parent != active;
    });
}

void Document::setPath(std::vector<model::InstanceId> path)
{
    if (m_tool) {
        const view::Camera camera;
        tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, camera };
        m_tool->cancel(ctx);
    }
    m_path = std::move(path);
    m_stack.setActiveDefinition(activeDefinition());
    m_selection.clear();
    emit contextChanged();
    emit selectionChanged();
    emit outlineChanged();
    touch();
}

void Document::pointerDown(const view::Camera& camera, const tools::PointerEvent& e)
{
    withTool(camera, [&](tools::Tool& t, tools::ToolContext& ctx) { t.pointerDown(ctx, e); });
}

void Document::pointerMove(const view::Camera& camera, const tools::PointerEvent& e)
{
    m_lastCamera = camera;
    m_lastPointer = e;
    withTool(camera, [&](tools::Tool& t, tools::ToolContext& ctx) { t.pointerMove(ctx, e); });
}

void Document::pointerUp(const view::Camera& camera, const tools::PointerEvent& e)
{
    withTool(camera, [&](tools::Tool& t, tools::ToolContext& ctx) { t.pointerUp(ctx, e); });
}

void Document::escape()
{
    withTool(view::Camera {}, [](tools::Tool& t, tools::ToolContext& ctx) { t.key(ctx, { tools::Key::Escape, {} }); });
}

void Document::enter()
{
    withTool(view::Camera {}, [](tools::Tool& t, tools::ToolContext& ctx) { t.key(ctx, { tools::Key::Enter, {} }); });
}

void Document::replayPointer()
{
    if (m_tool && m_lastPointer) {
        const view::Camera local = m_path.empty() ? m_lastCamera : m_lastCamera.inContext(contextTransform());
        tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, local };
        ctx.model = &m_model;
        ctx.context = activeDefinition();
        m_tool->pointerMove(ctx, *m_lastPointer);
    }
}

bool Document::arrowKey(const QString& direction)
{
    tools::Key key = tools::Key::Other;
    if (direction == QLatin1String("left")) {
        key = tools::Key::Left;
    } else if (direction == QLatin1String("right")) {
        key = tools::Key::Right;
    } else if (direction == QLatin1String("up")) {
        key = tools::Key::Up;
    } else if (direction == QLatin1String("down")) {
        key = tools::Key::Down;
    }
    bool used = false;
    withTool(m_lastCamera, [&](tools::Tool& t, tools::ToolContext& ctx) {
        used = t.key(ctx, { key, {} });
        if (used) {
            replayPointer();
        }
    });
    return used;
}

bool Document::applyMeasurement(const QString& text)
{
    bool applied = false;
    withTool(m_lastCamera, [&](tools::Tool& t, tools::ToolContext& ctx) {
        applied = t.applyMeasurement(ctx, text.toStdString());
        if (applied) {
            replayPointer();
        }
    });
    return applied;
}

void Document::undo()
{
    const geom::Selection before = m_selection;
    m_stack.undo();
    afterInteraction(before);
}

void Document::redo()
{
    const geom::Selection before = m_selection;
    m_stack.redo();
    afterInteraction(before);
}

void Document::deleteSelection()
{
    if (m_selectedCamera || m_selectedLight) {
        deleteSelectedObject();
        return;
    }
    const geom::Selection before = m_selection;
    const view::Camera camera;
    tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, camera };
    ctx.model = &m_model;
    ctx.context = activeDefinition();
    tools::deleteSelection(ctx);
    afterInteraction(before);
}

void Document::selectAll()
{
    const geom::Selection before = m_selection;
    const geom::Mesh& mesh = m_model.mesh(activeDefinition());
    m_selection.clear();
    m_selection.edges.insert(mesh.edges().keys().begin(), mesh.edges().keys().end());
    m_selection.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
    for (model::InstanceId id : m_model.childrenOf(activeDefinition())) {
        if (m_model.isVisible(id)) {
            m_selection.instances.insert(id);
        }
    }
    afterInteraction(before);
}

void Document::clearSelection()
{
    if (m_selectedCamera || m_selectedLight) {
        selectObject(QString(), 0.0);
    }
    const geom::Selection before = m_selection;
    m_selection.clear();
    afterInteraction(before);
}

template <class Fn>
void Document::runOnModel(const std::string& name, Fn&& fn)
{
    const geom::Selection before = m_selection;
    if (m_macro) {
        // Într-un script: totul intră în tranzacția deschisă de beginMacro (un singur pas de undo).
        fn(m_model);
    } else {
        m_stack.run(name, [&](model::Model& model) { fn(model); });
    }
    afterInteraction(before);
    // Entity Info arată și proprietăți ale obiectelor (tag, nume, blocare), nu doar ce e selectat.
    emit selectionChanged();
    emit outlineChanged();
}

void Document::makeGroup()
{
    if (m_selection.empty()) {
        return;
    }
    if (m_selection.faces.empty() && m_selection.edges.empty()) {
        groupSelection(); // doar obiecte selectate: le grupăm
        return;
    }
    runOnModel("Creează obiect", [&](model::Model& model) {
        const model::InstanceId id = model::makeGroup(model, activeDefinition(), m_selection, false, "Obiect");
        m_selection.clear();
        m_selection.instances.insert(id);
    });
}

void Document::groupSelection()
{
    if (m_selection.instances.empty()) {
        setError(QStringLiteral("Selectează unul sau mai multe obiecte ca să le grupezi (formele devin întâi obiect, cu ⌘G)."));
        return;
    }
    runOnModel("Grupează", [&](model::Model& model) {
        const model::InstanceId id = model::groupInstances(model, activeDefinition(), m_selection.instances, "Grup");
        m_selection.clear();
        m_selection.instances.insert(id);
    });
}

void Document::duplicateLinked()
{
    if (m_selection.instances.empty()) {
        return;
    }
    runOnModel("Duplicare legată", [&](model::Model& model) {
        std::set<model::InstanceId> copies;
        for (model::InstanceId id : m_selection.instances) {
            const model::DefinitionId def = model.instance(id).definition;
            // Geometria devine partajată: copia și originalul se editează împreună.
            model::Definition d = model.definition(def);
            if (d.group && !d.collection) {
                d.group = false;
                model.setDefinition(def, d);
            }
            const auto [lo, hi] = model::bounds(model, def);
            const double offset = lo.x <= hi.x ? (hi.x - lo.x) * 1.2 : 1000.0;
            copies.insert(model::copyInstance(model, id, math::Transform::translation({ offset, 0.0, 0.0 })));
        }
        m_selection.clear();
        m_selection.instances = copies;
    });
}

void Document::moveInstanceTo(double instanceId, double targetInstance)
{
    const model::InstanceId id = toInstanceId(instanceId);
    if (!m_model.instances().contains(id)) {
        return;
    }
    model::DefinitionId target = m_model.root();
    if (targetInstance > 0) {
        const model::InstanceId t = toInstanceId(targetInstance);
        if (!m_model.instances().contains(t)) {
            return;
        }
        target = m_model.instance(t).definition;
    }
    bool moved = false;
    runOnModel("Mută în grup", [&](model::Model& model) { moved = model::moveToParent(model, id, target); });
    if (!moved) {
        setError(QStringLiteral("Obiectul nu poate fi mutat acolo (ar intra în el însuși sau ținta are copii)."));
    }
}

void Document::selectByTag(double tagId)
{
    const model::TagId tag = toTagId(tagId);
    const geom::Selection before = m_selection;
    m_selection.clear();
    for (model::InstanceId id : m_model.childrenOf(activeDefinition())) {
        if (m_model.instance(id).tag == tag) {
            m_selection.instances.insert(id);
        }
    }
    const geom::Mesh& mesh = m_model.mesh(activeDefinition());
    for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
        if (mesh.faces().values()[i].tag == tag) {
            m_selection.faces.insert(mesh.faces().keys()[i]);
        }
    }
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        if (mesh.edges().values()[i].tag == tag) {
            m_selection.edges.insert(mesh.edges().keys()[i]);
        }
    }
    afterInteraction(before);
}

void Document::makeComponent(const QString& name)
{
    if (m_selection.empty()) {
        return;
    }
    runOnModel("Creează obiect", [&](model::Model& model) {
        const model::InstanceId id
            = model::makeGroup(model, activeDefinition(), m_selection, true, name.trimmed().toStdString());
        m_selection.clear();
        m_selection.instances.insert(id);
    });
}

void Document::explodeSelection()
{
    if (m_selection.instances.empty()) {
        return;
    }
    runOnModel("Desface", [&](model::Model& model) {
        geom::Selection result;
        for (model::InstanceId id : std::set(m_selection.instances)) {
            if (model.instances().contains(id) && !model.instance(id).locked) {
                result.add(model::explode(model, id));
            }
        }
        m_selection = result;
    });
}

void Document::makeUniqueSelection()
{
    runOnModel("Fă unic", [&](model::Model& model) {
        for (model::InstanceId id : m_selection.instances) {
            model::makeUnique(model, id);
        }
    });
}

void Document::hideSelection()
{
    if (m_selection.instances.empty()) {
        return;
    }
    runOnModel("Ascunde", [&](model::Model& model) {
        for (model::InstanceId id : m_selection.instances) {
            model::Instance inst = model.instance(id);
            inst.hidden = true;
            model.setInstance(id, inst);
        }
        m_selection.instances.clear();
    });
}

void Document::unhideAll()
{
    runOnModel("Arată tot", [&](model::Model& model) {
        for (model::InstanceId id : std::vector(model.instances().keys())) {
            if (model.instance(id).hidden) {
                model::Instance inst = model.instance(id);
                inst.hidden = false;
                model.setInstance(id, inst);
            }
        }
    });
}

void Document::setSelectionLocked(bool locked)
{
    runOnModel(locked ? "Blochează" : "Deblochează", [&](model::Model& model) {
        for (model::InstanceId id : m_selection.instances) {
            model::Instance inst = model.instance(id);
            inst.locked = locked;
            model.setInstance(id, inst);
        }
    });
}

namespace {

// Calea de la rădăcină până la definiția `parent` (prima instanță găsită pe fiecare nivel).
std::optional<std::vector<model::InstanceId>> pathTo(const model::Model& model, model::DefinitionId parent)
{
    std::vector<model::InstanceId> path;
    while (parent != model.root()) {
        const auto owners = model.instancesOf(parent);
        if (owners.empty()) {
            return std::nullopt;
        }
        path.insert(path.begin(), owners.front());
        parent = model.instance(owners.front()).parent;
    }
    return path;
}

} // namespace

void Document::enterInstance(double instanceId)
{
    model::InstanceId id = toInstanceId(instanceId);
    if (!id && m_selection.instances.size() == 1) {
        id = *m_selection.instances.begin();
    }
    const model::Instance* inst = m_model.instances().find(id);
    if (inst == nullptr || inst->locked) {
        return;
    }
    auto path = pathTo(m_model, inst->parent);
    if (!path) {
        return;
    }
    path->push_back(id);
    setPath(std::move(*path));
}

void Document::exitContext()
{
    if (m_path.empty()) {
        return;
    }
    const model::InstanceId left = m_path.back();
    std::vector<model::InstanceId> path = m_path;
    path.pop_back();
    setPath(std::move(path));
    m_selection.instances.insert(left); // ca în SketchUp: grupul din care ai ieșit rămâne selectat
    emit selectionChanged();
    emit outlineChanged();
    touch();
}

void Document::selectInstance(double instanceId)
{
    const model::InstanceId id = toInstanceId(instanceId);
    const model::Instance* inst = m_model.instances().find(id);
    if (inst == nullptr) {
        return;
    }
    if (inst->parent != activeDefinition()) {
        // Din Outliner: intrăm în contextul în care stă instanța.
        auto path = pathTo(m_model, inst->parent);
        if (!path) {
            return;
        }
        setPath(std::move(*path));
    }
    const geom::Selection before = m_selection;
    m_selection.clear();
    m_selection.instances.insert(id);
    afterInteraction(before);
}

void Document::renameInstance(double instanceId, const QString& name)
{
    const model::InstanceId id = toInstanceId(instanceId);
    if (!m_model.instances().contains(id)) {
        return;
    }
    runOnModel("Redenumește", [&](model::Model& model) {
        model::Instance inst = model.instance(id);
        const model::Definition& def = model.definition(inst.definition);
        if (def.group) {
            inst.name = name.trimmed().toStdString();
            model.setInstance(id, inst);
        } else if (!name.trimmed().isEmpty()) {
            // O componentă se redenumește în definiție (toate copiile).
            model::Definition d = def;
            d.name = name.trimmed().toStdString();
            model.setDefinition(inst.definition, d);
        }
    });
}

void Document::setInstanceHidden(double instanceId, bool hidden)
{
    const model::InstanceId id = toInstanceId(instanceId);
    if (!m_model.instances().contains(id)) {
        return;
    }
    runOnModel(hidden ? "Ascunde" : "Arată", [&](model::Model& model) {
        model::Instance inst = model.instance(id);
        inst.hidden = hidden;
        model.setInstance(id, inst);
    });
}

void Document::setInstanceLocked(double instanceId, bool locked)
{
    const model::InstanceId id = toInstanceId(instanceId);
    if (!m_model.instances().contains(id)) {
        return;
    }
    runOnModel(locked ? "Blochează" : "Deblochează", [&](model::Model& model) {
        model::Instance inst = model.instance(id);
        inst.locked = locked;
        model.setInstance(id, inst);
    });
}

void Document::addTag(const QString& name)
{
    runOnModel("Tag nou", [&](model::Model& model) {
        const model::Color color = kTagPalette[model.tags().size() % std::size(kTagPalette)];
        model.addTag({ name.trimmed().toStdString(), color, true });
    });
}

void Document::renameTag(double tagId, const QString& name)
{
    const model::TagId id = toTagId(tagId);
    if (!m_model.tags().contains(id) || name.trimmed().isEmpty()) {
        return;
    }
    runOnModel("Redenumește tag-ul", [&](model::Model& model) {
        model::Tag tag = model.tag(id);
        tag.name = name.trimmed().toStdString();
        model.setTag(id, tag);
    });
}

void Document::setTagVisible(double tagId, bool visible)
{
    const model::TagId id = toTagId(tagId);
    if (!m_model.tags().contains(id)) {
        return;
    }
    runOnModel(visible ? "Arată tag-ul" : "Ascunde tag-ul", [&](model::Model& model) {
        model::Tag tag = model.tag(id);
        tag.visible = visible;
        model.setTag(id, tag);
    });
}

void Document::removeTag(double tagId)
{
    const model::TagId id = toTagId(tagId);
    if (!m_model.tags().contains(id)) {
        return;
    }
    runOnModel("Șterge tag-ul", [&](model::Model& model) { model.removeTag(id); });
}

void Document::assignTag(double tagId)
{
    const model::TagId id = toTagId(tagId);
    if (m_selection.empty() || (id && !m_model.tags().contains(id))) {
        return;
    }
    runOnModel("Schimbă tag-ul", [&](model::Model& model) {
        for (model::InstanceId i : m_selection.instances) {
            model::Instance inst = model.instance(i);
            inst.tag = id;
            model.setInstance(i, inst);
        }
        // Formele (fețe, muchii) pot avea și ele tag: de exemplu pereții din camere diferite.
        geom::Mesh& mesh = model.mesh(activeDefinition());
        for (geom::FaceId f : m_selection.faces) {
            if (mesh.faces().contains(f)) {
                mesh.setFaceTag(f, id);
            }
        }
        for (geom::EdgeId e : m_selection.edges) {
            if (mesh.edges().contains(e)) {
                mesh.setEdgeTag(e, id);
            }
        }
    });
}

void Document::selectForContextMenu(const view::Camera& camera, math::Vec2d screen)
{
    if (m_tool && m_toolId != QLatin1String("select")) {
        return; // în timpul unei operații (desen, mutare) meniul lucrează pe selecția existentă
    }
    const geom::Selection before = m_selection;
    const view::Camera local = m_path.empty() ? camera : camera.inContext(contextTransform());
    tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, local };
    ctx.model = &m_model;
    ctx.context = activeDefinition();
    const geom::Selection hit = tools::selectionAt(ctx, screen);
    const bool alreadySelected = !hit.empty()
        && std::all_of(hit.faces.begin(), hit.faces.end(), [&](geom::FaceId f) { return m_selection.contains(f); })
        && std::all_of(hit.edges.begin(), hit.edges.end(), [&](geom::EdgeId e) { return m_selection.contains(e); })
        && std::all_of(hit.instances.begin(), hit.instances.end(),
            [&](model::InstanceId i) { return m_selection.contains(i); });
    if (!alreadySelected) {
        m_selection = hit;
    }
    afterInteraction(before);
}

// --- Timeline ---

QVariantList Document::animations() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.animations().size(); ++i) {
        const model::Animation& a = m_model.animations().values()[i];
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(m_model.animations().keys()[i].value) },
            { QStringLiteral("name"), QString::fromStdString(a.name) }, { QStringLiteral("duration"), a.duration } });
    }
    return out;
}

const model::Animation* Document::currentAnimation() const
{
    return m_activeAnimation ? m_model.animations().find(m_activeAnimation) : nullptr;
}

double Document::animationDuration() const
{
    const model::Animation* a = currentAnimation();
    return a ? a->duration : 0.0;
}

void Document::setActiveAnimation(double id)
{
    const model::AnimationId next { static_cast<std::uint64_t>(id) };
    if (next == m_activeAnimation) {
        return;
    }
    m_activeAnimation = m_model.animations().contains(next) ? next : model::AnimationId {};
    m_playing = false;
    emit timelineChanged();
    emit changed();
}

QVariantMap Document::timeline() const
{
    const model::Animation* a = currentAnimation();
    if (a == nullptr) {
        return {};
    }
    QVariantList tracks;
    for (const model::ObjectTrack& t : a->tracks) {
        const model::Instance* inst = m_model.instances().find(t.instance);
        QString name = QStringLiteral("(șters)");
        if (inst != nullptr) {
            const model::Definition& def = m_model.definition(inst->definition);
            name = QString::fromStdString(inst->name.empty() ? def.name : inst->name);
        }
        QVariantList transform;
        for (const model::TransformKey& k : t.transform) {
            transform.append(QVariantMap { { QStringLiteral("time"), k.time }, { QStringLiteral("easing"), static_cast<int>(k.easing) } });
        }
        QVariantList color;
        for (const model::ColorKey& k : t.color) {
            color.append(QVariantMap { { QStringLiteral("time"), k.time }, { QStringLiteral("easing"), static_cast<int>(k.easing) } });
        }
        tracks.append(QVariantMap { { QStringLiteral("instance"), static_cast<double>(t.instance.value) },
            { QStringLiteral("name"), name }, { QStringLiteral("transform"), transform }, { QStringLiteral("color"), color },
            { QStringLiteral("selected"), m_selection.instances.contains(t.instance) } });
    }
    QVariantList camera;
    for (const model::CameraKey& k : a->camera) {
        camera.append(QVariantMap { { QStringLiteral("time"), k.time }, { QStringLiteral("easing"), static_cast<int>(k.easing) } });
    }
    return { { QStringLiteral("name"), QString::fromStdString(a->name) }, { QStringLiteral("duration"), a->duration },
        { QStringLiteral("fps"), a->fps }, { QStringLiteral("loop"), a->loop }, { QStringLiteral("tracks"), tracks },
        { QStringLiteral("camera"), camera } };
}

void Document::setTimelineTime(double seconds)
{
    const double t = std::clamp(seconds, 0.0, std::max(animationDuration(), 0.0));
    const bool wasPreviewing = m_previewing;
    m_timelineTime = t;
    m_previewing = currentAnimation() != nullptr;
    ++m_interactionRevision;
    emit timelineTimeChanged();
    if (wasPreviewing != m_previewing) {
        emit timelineChanged();
    }
    emit changed();
}

template <class Fn>
void Document::editAnimation(const char* name, Fn&& fn)
{
    if (!currentAnimation()) {
        return;
    }
    const model::AnimationId id = m_activeAnimation;
    runOnModel(name, [&](model::Model& model) {
        model::Animation a = model.animations().at(id);
        fn(a);
        model.setAnimation(id, a);
    });
    ++m_interactionRevision;
    emit timelineChanged();
}

void Document::createAnimation()
{
    model::AnimationId created;
    runOnModel("Animație nouă", [&](model::Model& model) {
        model::Animation a;
        a.name = "Animația " + std::to_string(model.animations().size() + 1);
        created = model.addAnimation(a);
    });
    m_activeAnimation = created;
    m_timelineTime = 0.0;
    emit timelineChanged();
    emit timelineTimeChanged();
}

void Document::renameAnimation(double id, const QString& name)
{
    const model::AnimationId a { static_cast<std::uint64_t>(id) };
    if (!m_model.animations().contains(a) || name.trimmed().isEmpty()) {
        return;
    }
    runOnModel("Redenumește animația", [&](model::Model& model) {
        model::Animation anim = model.animations().at(a);
        anim.name = name.trimmed().toStdString();
        model.setAnimation(a, anim);
    });
    emit timelineChanged();
}

void Document::removeAnimation(double id)
{
    const model::AnimationId a { static_cast<std::uint64_t>(id) };
    if (!m_model.animations().contains(a)) {
        return;
    }
    runOnModel("Șterge animația", [&](model::Model& model) { model.removeAnimation(a); });
    if (m_activeAnimation == a) {
        m_activeAnimation = {};
        m_previewing = false;
        m_playing = false;
    }
    emit timelineChanged();
    emit changed();
}

void Document::setAnimationSettings(double duration, int fps, bool loop)
{
    editAnimation("Setările animației", [&](model::Animation& a) {
        a.duration = std::clamp(duration, 0.1, 3600.0);
        a.fps = std::clamp(fps, 1, 120);
        a.loop = loop;
    });
}

void Document::keySelection()
{
    if (m_selection.instances.empty()) {
        setError(QStringLiteral("Selectează obiectele pe care vrei să le animezi, apoi pune keyframe-ul."));
        return;
    }
    const double t = m_timelineTime;
    editAnimation("Keyframe", [&](model::Animation& a) {
        for (model::InstanceId id : m_selection.instances) {
            model::setTransformKey(a, id, t, m_model.instance(id).transform);
        }
    });
}

void Document::keySelectionColor()
{
    if (m_selection.instances.empty()) {
        setError(QStringLiteral("Selectează obiectele, apoi pune keyframe-ul de culoare."));
        return;
    }
    const double t = m_timelineTime;
    editAnimation("Keyframe culoare", [&](model::Animation& a) {
        for (model::InstanceId id : m_selection.instances) {
            const model::Instance& inst = m_model.instance(id);
            model::Color c { 220, 220, 220, 255 };
            if (inst.material && m_model.materials().contains(inst.material)) {
                c = m_model.material(inst.material).color;
            }
            model::setColorKey(a, id, t, c);
        }
    });
}

void Document::keyCamera(const QVariantMap& camera)
{
    const double t = m_timelineTime;
    editAnimation("Keyframe cameră", [&](model::Animation& a) {
        model::setCameraKey(a, { t,
            { camera.value(QStringLiteral("tx")).toDouble(), camera.value(QStringLiteral("ty")).toDouble(),
                camera.value(QStringLiteral("tz")).toDouble() },
            camera.value(QStringLiteral("yaw")).toDouble(), camera.value(QStringLiteral("pitch")).toDouble(),
            camera.value(QStringLiteral("distance")).toDouble(), model::Easing::EaseInOut });
    });
}

namespace {

model::KeyKind keyKind(const QString& kind)
{
    return kind == QLatin1String("color") ? model::KeyKind::Color
        : kind == QLatin1String("camera") ? model::KeyKind::Camera
                                          : model::KeyKind::Transform;
}

} // namespace

void Document::removeKey(const QString& kind, double instance, double time)
{
    editAnimation("Șterge keyframe-ul", [&](model::Animation& a) {
        model::removeKey(a, keyKind(kind), toInstanceId(instance), time);
    });
}

void Document::moveKey(const QString& kind, double instance, double time, double newTime)
{
    editAnimation("Mută keyframe-ul", [&](model::Animation& a) {
        model::moveKey(a, keyKind(kind), toInstanceId(instance), time, newTime);
    });
}

void Document::setKeyEasing(const QString& kind, double instance, double time, int easing)
{
    const auto e = static_cast<model::Easing>(std::clamp(easing, 0, 4));
    const double tol = 0.5 / 30.0;
    editAnimation("Easing", [&](model::Animation& a) {
        if (keyKind(kind) == model::KeyKind::Camera) {
            for (model::CameraKey& k : a.camera) {
                if (std::abs(k.time - time) < tol) {
                    k.easing = e;
                }
            }
            return;
        }
        for (model::ObjectTrack& t : a.tracks) {
            if (t.instance != toInstanceId(instance)) {
                continue;
            }
            for (model::TransformKey& k : t.transform) {
                if (keyKind(kind) == model::KeyKind::Transform && std::abs(k.time - time) < tol) {
                    k.easing = e;
                }
            }
            for (model::ColorKey& k : t.color) {
                if (keyKind(kind) == model::KeyKind::Color && std::abs(k.time - time) < tol) {
                    k.easing = e;
                }
            }
        }
    });
}

void Document::play()
{
    if (!currentAnimation()) {
        return;
    }
    if (m_timelineTime >= animationDuration() - 1e-9) {
        m_timelineTime = 0.0;
    }
    m_playing = true;
    m_previewing = true;
    emit timelineChanged();
    emit animationStarted();
}

void Document::pause()
{
    if (m_playing) {
        m_playing = false;
        emit timelineChanged();
    }
}

void Document::endPreview()
{
    if (!m_previewing && !m_playing) {
        return;
    }
    m_playing = false;
    m_previewing = false;
    ++m_interactionRevision;
    emit timelineChanged();
    emit changed();
}

QVariantMap Document::animationCamera() const
{
    const model::Animation* a = currentAnimation();
    if (a == nullptr || !m_previewing) {
        return {};
    }
    const auto c = model::evaluateCamera(*a, m_timelineTime);
    if (!c) {
        return {};
    }
    return { { QStringLiteral("tx"), c->target.x }, { QStringLiteral("ty"), c->target.y }, { QStringLiteral("tz"), c->target.z },
        { QStringLiteral("yaw"), c->yaw }, { QStringLiteral("pitch"), c->pitch }, { QStringLiteral("distance"), c->distance } };
}

std::optional<model::Color> Document::displayColor(model::InstanceId instance) const
{
    const model::Animation* a = m_previewing ? currentAnimation() : nullptr;
    if (a == nullptr) {
        return std::nullopt;
    }
    const model::ObjectTrack* t = model::findTrack(*a, instance);
    return t ? model::evaluateColor(*t, m_timelineTime) : std::nullopt;
}

// --- Modul interacțiune ---

void Document::setInteractionMode(bool on)
{
    if (on == m_runtime.has_value()) {
        return;
    }
    if (on) {
        if (m_tool) {
            const view::Camera camera;
            tools::ToolContext ctx { m_model.mesh(activeDefinition()), m_stack, m_selection, camera };
            m_tool->cancel(ctx);
        }
        endPreview();
        m_runtime.emplace(m_model); // o copie: nimic din ce se întâmplă aici nu ajunge în document
        m_rules.reset();
        m_hoverInstance = {};
    } else {
        m_rules.reset();
        m_interactions.clear();
        m_drag.reset();
        m_runtime.reset();
    }
    ++m_interactionRevision;
    emit interactionModeChanged();
    emit toolStatusChanged();
    emit selectionChanged();
    emit changed();
}

namespace {

// Ce eveniment are o instanță (pentru cursor): drag > click > restul.
bool hasTrigger(const model::Instance& inst, model::Trigger t)
{
    return std::any_of(inst.rules.begin(), inst.rules.end(), [&](const model::Rule& r) { return r.trigger == t; });
}

} // namespace

model::InstanceId Document::ruleTargetAt(const view::Camera& camera, math::Vec2d screen) const
{
    // Cea mai adâncă instanță de sub cursor care are reguli sau un comportament (balama, glisare).
    model::InstanceId found;
    model::DefinitionId context = model().root();
    view::Camera local = camera;
    while (const auto hit = view::pickInstance(model(), context, local, screen)) {
        const model::Instance& inst = model().instance(hit->instance);
        if (!inst.rules.empty() || inst.behavior.kind != model::Behavior::Kind::None) {
            found = hit->instance;
        }
        local = local.inContext(inst.transform);
        context = inst.definition;
    }
    return found;
}

QString Document::interactionHover(const view::Camera& camera, math::Vec2d screen)
{
    if (!m_runtime) {
        return {};
    }
    const model::InstanceId hit = ruleTargetAt(camera, screen);
    if (hit != m_hoverInstance) {
        if (m_hoverInstance && m_runtime->instances().contains(m_hoverInstance)) {
            m_rules.fire(*m_runtime, m_hoverInstance, model::Trigger::HoverLeave);
        }
        if (hit) {
            m_rules.fire(*m_runtime, hit, model::Trigger::HoverEnter);
        }
        m_hoverInstance = hit;
        handleRuleRequests();
        if (m_rules.active()) {
            emit animationStarted();
        }
    }
    if (!hit) {
        return {};
    }
    const model::Instance& inst = m_runtime->instance(hit);
    if (hasTrigger(inst, model::Trigger::Drag)) {
        return QStringLiteral("drag");
    }
    if (hasTrigger(inst, model::Trigger::Click) || hasTrigger(inst, model::Trigger::DoubleClick)
        || hasTrigger(inst, model::Trigger::LongPress) || inst.behavior.kind != model::Behavior::Kind::None) {
        return QStringLiteral("click");
    }
    if (hasTrigger(inst, model::Trigger::RightClick)) {
        return QStringLiteral("context");
    }
    return QStringLiteral("hover");
}

bool Document::interactionEvent(const view::Camera& camera, math::Vec2d screen, const QString& event)
{
    if (!m_runtime) {
        return false;
    }
    const model::InstanceId hit = ruleTargetAt(camera, screen);
    if (!hit) {
        return false;
    }
    const model::Trigger trigger = event == QLatin1String("rightClick") ? model::Trigger::RightClick
        : event == QLatin1String("doubleClick")                       ? model::Trigger::DoubleClick
        : event == QLatin1String("longPress")                         ? model::Trigger::LongPress
                                                                       : model::Trigger::Click;
    bool fired = m_rules.fire(*m_runtime, hit, trigger);
    // Fără o regulă de click, o ușă sau un sertar (comportamentul simplu) se deschide / închide.
    if (!fired && trigger == model::Trigger::Click
        && m_runtime->instance(hit).behavior.kind != model::Behavior::Kind::None) {
        fired = interactAt(camera, screen);
    }
    handleRuleRequests();
    if (fired) {
        emit animationStarted();
    }
    return fired;
}

bool Document::interactionDragStart(const view::Camera& camera, math::Vec2d screen)
{
    if (!m_runtime) {
        return false;
    }
    const model::InstanceId hit = ruleTargetAt(camera, screen);
    if (!hit || !hasTrigger(m_runtime->instance(hit), model::Trigger::Drag)) {
        return false;
    }
    // Obiectul urmează cursorul pe planul orizontal care trece prin poziția lui.
    const math::Vec3d at = m_runtime->instance(hit).transform.applyToPoint({ 0, 0, 0 });
    const math::Ray ray = camera.rayAt(screen);
    const double t = std::abs(ray.direction.z) > 1e-9 ? (at.z - ray.origin.z) / ray.direction.z : 0.0;
    m_drag = DragState { hit, ray.at(std::max(t, 0.0)), at.z };
    return true;
}

void Document::interactionDragMove(const view::Camera& camera, math::Vec2d screen)
{
    if (!m_runtime || !m_drag || !m_runtime->instances().contains(m_drag->instance)) {
        return;
    }
    const math::Ray ray = camera.rayAt(screen);
    if (std::abs(ray.direction.z) < 1e-9) {
        return;
    }
    const double t = (m_drag->planeZ - ray.origin.z) / ray.direction.z;
    if (t <= 0.0) {
        return;
    }
    const math::Vec3d p = ray.at(t);
    model::Instance inst = m_runtime->instance(m_drag->instance);
    inst.transform = math::Transform::translation(p - m_drag->last) * inst.transform;
    m_runtime->setInstance(m_drag->instance, inst);
    m_drag->last = p;
    ++m_interactionRevision;
    emit changed();
}

void Document::interactionDragEnd()
{
    if (!m_runtime || !m_drag) {
        return;
    }
    const model::InstanceId id = m_drag->instance;
    m_drag.reset();
    if (m_rules.fire(*m_runtime, id, model::Trigger::Drag)) {
        handleRuleRequests();
        emit animationStarted();
    }
}

void Document::interactionKey(const QString& key)
{
    if (!m_runtime) {
        return;
    }
    bool fired = false;
    for (model::InstanceId id : std::vector(m_runtime->instances().keys().begin(), m_runtime->instances().keys().end())) {
        fired = m_rules.fire(*m_runtime, id, model::Trigger::Key, key.toStdString()) || fired;
    }
    handleRuleRequests();
    if (fired) {
        emit animationStarted();
    }
}

namespace {

const char* const kTriggerNames[] = { "click", "rightClick", "doubleClick", "hoverEnter", "hoverLeave", "longPress",
    "drag", "key" };
const char* const kActionNames[] = { "moveBy", "moveTo", "rotateBy", "scaleTo", "show", "hide", "toggleVisible",
    "setMaterial", "playAnimation", "goToScene", "wait", "alternate" };

template <std::size_t N>
int indexOf(const char* const (&names)[N], const QString& name)
{
    for (std::size_t i = 0; i < N; ++i) {
        if (name == QLatin1String(names[i])) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

QVariantMap actionToMap(const model::Action& a)
{
    QVariantList first;
    for (const model::Action& c : a.first) {
        first.append(actionToMap(c));
    }
    QVariantList second;
    for (const model::Action& c : a.second) {
        second.append(actionToMap(c));
    }
    return { { QStringLiteral("kind"), QString::fromLatin1(kActionNames[static_cast<int>(a.kind)]) },
        { QStringLiteral("target"), static_cast<double>(a.target.value) }, { QStringLiteral("x"), a.vector.x },
        { QStringLiteral("y"), a.vector.y }, { QStringLiteral("z"), a.vector.z }, { QStringLiteral("duration"), a.duration },
        { QStringLiteral("material"), static_cast<double>(a.material.value) },
        { QStringLiteral("animation"), static_cast<double>(a.animation.value) },
        { QStringLiteral("scene"), static_cast<double>(a.scene.value) }, { QStringLiteral("first"), first },
        { QStringLiteral("second"), second } };
}

model::Action actionFromMap(const QVariantMap& m)
{
    model::Action a;
    a.kind = static_cast<model::ActionKind>(indexOf(kActionNames, m.value(QStringLiteral("kind")).toString()));
    a.target = model::InstanceId { static_cast<std::uint64_t>(m.value(QStringLiteral("target")).toDouble()) };
    a.vector = { m.value(QStringLiteral("x")).toDouble(), m.value(QStringLiteral("y")).toDouble(),
        m.value(QStringLiteral("z")).toDouble() };
    a.duration = m.value(QStringLiteral("duration"), 0.5).toDouble();
    a.material = model::MaterialId { static_cast<std::uint64_t>(m.value(QStringLiteral("material")).toDouble()) };
    a.animation = model::AnimationId { static_cast<std::uint64_t>(m.value(QStringLiteral("animation")).toDouble()) };
    a.scene = model::SceneId { static_cast<std::uint64_t>(m.value(QStringLiteral("scene")).toDouble()) };
    for (const QVariant& c : m.value(QStringLiteral("first")).toList()) {
        a.first.push_back(actionFromMap(c.toMap()));
    }
    for (const QVariant& c : m.value(QStringLiteral("second")).toList()) {
        a.second.push_back(actionFromMap(c.toMap()));
    }
    return a;
}

} // namespace

QVariantList Document::rulesOf(double instanceId) const
{
    const model::Instance* inst = m_model.instances().find(toInstanceId(instanceId));
    QVariantList out;
    if (inst == nullptr) {
        return out;
    }
    for (const model::Rule& r : inst->rules) {
        QVariantList actions;
        for (const model::Action& a : r.actions) {
            actions.append(actionToMap(a));
        }
        out.append(QVariantMap { { QStringLiteral("trigger"), QString::fromLatin1(kTriggerNames[static_cast<int>(r.trigger)]) },
            { QStringLiteral("key"), QString::fromStdString(r.key) }, { QStringLiteral("actions"), actions } });
    }
    return out;
}

void Document::setRules(double instanceId, const QVariantList& rules)
{
    const model::InstanceId id = toInstanceId(instanceId);
    if (!m_model.instances().contains(id)) {
        return;
    }
    std::vector<model::Rule> converted;
    for (const QVariant& v : rules) {
        const QVariantMap m = v.toMap();
        model::Rule r;
        r.trigger = static_cast<model::Trigger>(indexOf(kTriggerNames, m.value(QStringLiteral("trigger")).toString()));
        r.key = m.value(QStringLiteral("key")).toString().toStdString();
        for (const QVariant& a : m.value(QStringLiteral("actions")).toList()) {
            r.actions.push_back(actionFromMap(a.toMap()));
        }
        converted.push_back(std::move(r));
    }
    runOnModel("Interacțiuni", [&](model::Model& model) {
        model::Instance inst = model.instance(id);
        inst.rules = std::move(converted);
        model.setInstance(id, inst);
    });
}

QVariantList Document::interactions() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.instances().size(); ++i) {
        const model::Instance& inst = m_model.instances().values()[i];
        if (inst.rules.empty()) {
            continue;
        }
        QVariantList rules;
        for (const model::Rule& r : inst.rules) {
            QStringList actions;
            for (const model::Action& a : r.actions) {
                actions.append(QString::fromLatin1(kActionNames[static_cast<int>(a.kind)]));
            }
            rules.append(QVariantMap { { QStringLiteral("trigger"), QString::fromLatin1(kTriggerNames[static_cast<int>(r.trigger)]) },
                { QStringLiteral("key"), QString::fromStdString(r.key) }, { QStringLiteral("actions"), actions } });
        }
        const model::Definition& def = m_model.definition(inst.definition);
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(m_model.instances().keys()[i].value) },
            { QStringLiteral("name"), QString::fromStdString(inst.name.empty() ? def.name : inst.name) },
            { QStringLiteral("shape"), QString::fromStdString(def.shape) }, { QStringLiteral("rules"), rules } });
    }
    return out;
}

void Document::handleRuleRequests()
{
    for (const model::HostRequest& r : m_rules.takeRequests()) {
        if (r.playAnimation && m_model.animations().contains(r.playAnimation)) {
            m_activeAnimation = r.playAnimation;
            m_timelineTime = 0.0;
            play();
        }
        if (r.goToScene && m_runtime && m_model.scenes().contains(r.goToScene)) {
            const model::Scene& scene = m_model.scenes().at(r.goToScene);
            model::applySceneState(*m_runtime, scene);
            emit cameraRequested(sceneCamera(static_cast<double>(r.goToScene.value)));
            ++m_interactionRevision;
            emit changed();
        }
    }
}

void Document::applyScene(double sceneId, double seconds)
{
    const model::SceneId id { static_cast<std::uint64_t>(sceneId) };
    if (!m_model.scenes().contains(id) || !m_model.scenes().at(id).hasState) {
        return;
    }
    const model::Scene& scene = m_model.scenes().at(id);
    m_sceneTransition = SceneTransition {};
    m_sceneTransition->scene = id;
    m_sceneTransition->duration = std::max(seconds, 0.0);
    for (const auto& [instance, to] : scene.transforms) {
        if (m_model.instances().contains(instance) && m_model.instance(instance).transform != to) {
            m_sceneTransition->moves[instance] = { m_model.instance(instance).transform, to };
        }
    }
    if (m_sceneTransition->duration <= 0.0 || m_sceneTransition->moves.empty()) {
        finishSceneTransition();
        return;
    }
    emit animationStarted();
}

void Document::finishSceneTransition()
{
    if (!m_sceneTransition) {
        return;
    }
    const model::SceneId id = m_sceneTransition->scene;
    m_sceneTransition.reset();
    if (m_model.scenes().contains(id)) {
        const model::Scene scene = m_model.scenes().at(id);
        runOnModel("Aplică scena", [&](model::Model& model) { model::applySceneState(model, scene); });
    }
}

bool Document::interactAt(const view::Camera& camera, math::Vec2d screen)
{
    const auto hit = view::interactiveInstanceAt(model(), camera, screen);
    if (!hit) {
        return false;
    }
    for (const InteractionAnimation& a : m_interactions) {
        if (a.instance == *hit) {
            return true; // deja în mișcare
        }
    }
    const model::Behavior& b = model().instance(*hit).behavior;
    m_interactions.push_back({ *hit, 0.0, std::max(b.duration, 0.0), !b.open });
    return true;
}

bool Document::advanceInteractions(double dt)
{
    bool active = false;
    bool moved = false;

    // Timeline-ul (redare).
    if (m_playing) {
        if (const model::Animation* a = currentAnimation()) {
            m_timelineTime += dt;
            if (m_timelineTime >= a->duration) {
                if (a->loop && a->duration > 0.0) {
                    m_timelineTime = std::fmod(m_timelineTime, a->duration);
                } else {
                    m_timelineTime = a->duration;
                    m_playing = false;
                    emit timelineChanged();
                }
            }
            emit timelineTimeChanged();
        } else {
            m_playing = false;
            emit timelineChanged();
        }
        moved = true;
        active = active || m_playing;
    }

    // Tranziția unei scene.
    if (m_sceneTransition) {
        m_sceneTransition->elapsed += dt;
        if (m_sceneTransition->elapsed >= m_sceneTransition->duration) {
            finishSceneTransition();
        }
        moved = true;
        active = active || m_sceneTransition.has_value();
    }

    // Ușile și sertarele (Interact): la sfârșit, starea nouă intră în model (cu undo) sau în copia Modului interacțiune.
    if (!m_interactions.empty()) {
        std::vector<model::InstanceId> finished;
        for (InteractionAnimation& a : m_interactions) {
            a.elapsed += dt;
            if (a.elapsed >= a.duration) {
                finished.push_back(a.instance);
            }
        }
        std::erase_if(m_interactions, [](const InteractionAnimation& a) { return a.elapsed >= a.duration; });
        for (model::InstanceId id : finished) {
            if (m_runtime) {
                if (m_runtime->instances().contains(id)) {
                    model::toggleBehavior(*m_runtime, id);
                }
            } else if (m_model.instances().contains(id)) {
                const bool opening = !m_model.instance(id).behavior.open;
                runOnModel(opening ? "Deschide" : "Închide", [id](model::Model& model) { model::toggleBehavior(model, id); });
            }
        }
        moved = true;
        active = active || !m_interactions.empty();
    }

    // Regulile (Modul interacțiune).
    if (m_runtime && m_rules.active()) {
        m_rules.update(*m_runtime, dt);
        handleRuleRequests();
        moved = true;
        active = active || m_rules.active();
    }

    if (moved) {
        ++m_interactionRevision;
        emit changed();
    }
    return active;
}

std::optional<math::Transform> Document::displayTransform(model::InstanceId instance) const
{
    if (m_previewing) {
        if (const model::Animation* a = currentAnimation()) {
            if (const model::ObjectTrack* t = model::findTrack(*a, instance)) {
                if (auto value = model::evaluateTransform(*t, m_timelineTime)) {
                    return value;
                }
            }
        }
    }
    if (m_sceneTransition) {
        const auto it = m_sceneTransition->moves.find(instance);
        if (it != m_sceneTransition->moves.end()) {
            const double k = m_sceneTransition->duration > 0.0
                ? std::clamp(m_sceneTransition->elapsed / m_sceneTransition->duration, 0.0, 1.0)
                : 1.0;
            const double eased = k < 0.5 ? 4 * k * k * k : 1 - std::pow(-2 * k + 2, 3) / 2;
            return math::interpolate(it->second.first, it->second.second, eased);
        }
    }
    for (const InteractionAnimation& a : m_interactions) {
        if (a.instance == instance && model().instances().contains(instance)) {
            const double k = a.duration > 0.0 ? std::clamp(a.elapsed / a.duration, 0.0, 1.0) : 1.0;
            const double eased = k * k * (3.0 - 2.0 * k);
            return model::transformAtOpenness(model(), instance, a.opening ? eased : 1.0 - eased);
        }
    }
    return std::nullopt;
}

void Document::setBehavior(const QVariantMap& behavior)
{
    if (m_selection.instances.size() != 1) {
        return;
    }
    const model::InstanceId id = *m_selection.instances.begin();
    const QString kind = behavior.value(QStringLiteral("kind")).toString();
    runOnModel("Comportament", [&](model::Model& model) {
        // Întâi închidem (transformarea „închisă” e referința pentru orice comportament nou).
        if (model.instance(id).behavior.open) {
            model::toggleBehavior(model, id);
        }
        model::Instance inst = model.instance(id);
        inst.behavior.kind = kind == QLatin1String("rotate") ? model::Behavior::Kind::Rotate
            : kind == QLatin1String("slide")                ? model::Behavior::Kind::Slide
                                                            : model::Behavior::Kind::None;
        inst.behavior.anchor
            = static_cast<model::Behavior::Anchor>(std::clamp(behavior.value(QStringLiteral("anchor")).toInt(), 0, 5));
        inst.behavior.amount = behavior.value(QStringLiteral("amount"), inst.behavior.amount).toDouble();
        inst.behavior.duration = std::clamp(behavior.value(QStringLiteral("duration"), 0.6).toDouble(), 0.0, 10.0);
        inst.behavior.open = false;
        model.setInstance(id, inst);
    });
}

std::optional<math::Vec3d> Document::surfacePoint(const view::Camera& camera, math::Vec2d screen) const
{
    std::optional<math::Vec3d> best;
    double bestDistance = std::numeric_limits<double>::infinity();
    const auto consider = [&](const math::Vec3d& p) {
        const double d = glm::dot(p - camera.eye(), camera.forward());
        if (d > 0.0 && d < bestDistance) {
            bestDistance = d;
            best = p;
        }
    };
    const view::PickResult hit = view::pick(m_model.mesh(m_model.root()), camera, screen);
    if (hit.hit()) {
        consider(hit.point);
    }
    if (const auto instance = view::pickInstance(m_model, m_model.root(), camera, screen)) {
        consider(instance->point);
    }
    return best;
}

std::optional<std::pair<math::Vec3d, math::Vec3d>> Document::selectionBounds() const
{
    if (m_selection.empty()) {
        return std::nullopt;
    }
    const math::Transform ctx = contextTransform();
    math::Vec3d lo { std::numeric_limits<double>::infinity() };
    math::Vec3d hi { -std::numeric_limits<double>::infinity() };
    const auto add = [&](const math::Vec3d& local) {
        const math::Vec3d p = ctx.applyToPoint(local);
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    };
    const geom::Mesh& mesh = m_model.mesh(activeDefinition());
    for (geom::VertexId v : geom::verticesOf(mesh, m_selection)) {
        add(mesh.position(v));
    }
    for (model::InstanceId id : m_selection.instances) {
        if (!m_model.instances().contains(id)) {
            continue;
        }
        const model::Instance& inst = m_model.instance(id);
        const auto [blo, bhi] = model::bounds(m_model, inst.definition);
        if (blo.x > bhi.x) {
            continue;
        }
        for (int k = 0; k < 8; ++k) {
            add(inst.transform.applyToPoint({ (k & 1) ? bhi.x : blo.x, (k & 2) ? bhi.y : blo.y, (k & 4) ? bhi.z : blo.z }));
        }
    }
    if (lo.x > hi.x) {
        return std::nullopt;
    }
    return std::pair { lo, hi };
}

QVariantList Document::contextMenuItems() const
{
    QVariantList items;
    const auto item = [&](const QString& id, const QString& name, const QString& shortcut = {}, bool enabled = true,
                          bool destructive = false) {
        items.append(QVariantMap { { QStringLiteral("id"), id }, { QStringLiteral("name"), name },
            { QStringLiteral("shortcut"), shortcut }, { QStringLiteral("enabled"), enabled },
            { QStringLiteral("destructive"), destructive } });
    };
    const auto separator = [&] {
        if (!items.isEmpty() && !items.last().toMap().value(QStringLiteral("separator")).toBool()) {
            items.append(QVariantMap { { QStringLiteral("separator"), true } });
        }
    };
    const bool anyHidden = std::any_of(m_model.instances().values().begin(), m_model.instances().values().end(),
        [](const model::Instance& i) { return i.hidden; });

    if (m_selection.empty()) {
        if (!m_path.empty()) {
            const bool group = m_model.definition(activeDefinition()).group;
            item(QStringLiteral("org.exit"), group ? QStringLiteral("Ieși din grup") : QStringLiteral("Ieși din obiect"),
                QStringLiteral("Escape"));
            separator();
        }
        item(QStringLiteral("edit.selectAll"), QStringLiteral("Selectează tot"), QStringLiteral("Ctrl+A"));
        if (!m_model.guides().empty()) {
            item(QStringLiteral("guides.clear"), QStringLiteral("Șterge ghidajele"));
        }
        if (anyHidden) {
            item(QStringLiteral("org.unhideAll"), QStringLiteral("Arată tot"));
        }
        separator();
        item(QStringLiteral("view.extents"), QStringLiteral("Zoom Extents"), QStringLiteral("Shift+Z"));
        return items;
    }

    const bool geometry = !m_selection.faces.empty() || !m_selection.edges.empty();
    const bool instances = !m_selection.instances.empty();
    bool allLocked = instances;
    bool anyLocked = false;
    for (model::InstanceId id : m_selection.instances) {
        allLocked = allLocked && m_model.instance(id).locked;
        anyLocked = anyLocked || m_model.instance(id).locked;
    }

    if (!geometry && m_selection.instances.size() == 1) {
        const model::InstanceId id = *m_selection.instances.begin();
        const model::Instance& inst = m_model.instance(id);
        const bool group = m_model.definition(inst.definition).group;
        item(QStringLiteral("org.enter"), group ? QStringLiteral("Editează grupul") : QStringLiteral("Editează obiectul"),
            {}, !inst.locked);
        separator();
        item(QStringLiteral("org.explode"), QStringLiteral("Desface"), {}, !inst.locked);
        if (!group && m_model.instancesOf(inst.definition).size() > 1) {
            item(QStringLiteral("org.makeUnique"), QStringLiteral("Fă unic"));
        }
        item(QStringLiteral("org.group"), QStringLiteral("Grupează"), QStringLiteral("Ctrl+Alt+G"));
        item(QStringLiteral("org.duplicateLinked"), QStringLiteral("Duplicare legată"), QStringLiteral("Alt+D"));
    } else {
        if (geometry) {
            item(QStringLiteral("org.makeGroup"), QStringLiteral("Creează obiect"), QStringLiteral("Ctrl+G"));
        }
        if (instances) {
            item(QStringLiteral("org.group"), QStringLiteral("Grupează"), QStringLiteral("Ctrl+Alt+G"));
        }
        if (instances) {
            item(QStringLiteral("org.explode"), QStringLiteral("Desface"), {}, !allLocked);
        }
    }
    if (!geometry && m_selection.instances.size() == 2) {
        separator();
        item(QStringLiteral("solid.union"), QStringLiteral("Reuniune"));
        item(QStringLiteral("solid.subtract"), QStringLiteral("Scade al doilea din primul"));
        item(QStringLiteral("solid.intersect"), QStringLiteral("Intersecție (partea comună)"));
    }
    separator();
    item(QStringLiteral("edit.mirrorX"), QStringLiteral("Oglindește pe X (roșu)"));
    item(QStringLiteral("edit.mirrorY"), QStringLiteral("Oglindește pe Y (verde)"));
    item(QStringLiteral("edit.mirrorZ"), QStringLiteral("Oglindește pe Z (albastru)"));
    item(QStringLiteral("edit.array"), QStringLiteral("Array (copii în linie sau în cerc)…"));
    item(QStringLiteral("edit.soften"), QStringLiteral("Soften / Smooth…"));
    if (geometry) {
        separator();
        item(QStringLiteral("edit.selectConnected"), QStringLiteral("Selectează ce e conectat"));
        if (!m_selection.faces.empty()) {
            item(QStringLiteral("edit.intersectModel"), QStringLiteral("Intersectează cu modelul"));
        }
        if (!m_selection.faces.empty()) {
            item(QStringLiteral("edit.reverseFaces"), m_selection.faces.size() == 1 ? QStringLiteral("Inversează fața")
                                                                                    : QStringLiteral("Inversează fețele"));
        }
    }
    if (instances) {
        separator();
        item(QStringLiteral("org.hide"), QStringLiteral("Ascunde"));
        if (anyLocked) {
            item(QStringLiteral("org.unlock"), QStringLiteral("Deblochează"));
        }
        if (!allLocked) {
            item(QStringLiteral("org.lock"), QStringLiteral("Blochează"));
        }
    }
    separator();
    item(QStringLiteral("view.zoomSelection"), QStringLiteral("Zoom la selecție"));
    separator();
    item(QStringLiteral("edit.delete"), QStringLiteral("Șterge"), QStringLiteral("Delete"), geometry || !allLocked, true);
    return items;
}

namespace {

QVariantMap materialEntry(const model::Material& m)
{
    return QVariantMap {
        { QStringLiteral("name"), QString::fromStdString(m.name) },
        { QStringLiteral("color"), QColor(m.color.r, m.color.g, m.color.b, m.color.a) },
        { QStringLiteral("roughness"), m.roughness },
        { QStringLiteral("metallic"), m.metallic },
    };
}

} // namespace

namespace {

// O miniatură (PNG mic, ca data URL) pentru panoul de materiale.
QString thumbnailUrl(const QImage& image)
{
    if (image.isNull()) {
        return {};
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.scaled(128, 128, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

} // namespace

QString Document::textureThumbnail(model::TextureId id) const
{
    const model::Texture* t = m_model.textures().find(id);
    if (t == nullptr || t->data.empty()) {
        return {};
    }
    const std::size_t key = t->data.size() ^ (id.value << 32);
    auto& cached = m_thumbnails[id.value];
    if (cached.first != key) {
        cached = { key, thumbnailUrl(QImage::fromData(t->data.data(), static_cast<int>(t->data.size()))) };
    }
    return cached.second;
}

QVariantList Document::libraryMaterials() const
{
    QVariantList out;
    const auto library = model::builtinMaterials();
    for (std::size_t i = 0; i < library.size(); ++i) {
        QVariantMap entry = materialEntry(library[i]);
        entry[QStringLiteral("index")] = static_cast<int>(i);
        if (const std::string_view key = model::builtinTexture(i); !key.empty()) {
            static std::map<std::string_view, QString> thumbnails;
            auto& url = thumbnails[key];
            if (url.isEmpty()) {
                url = thumbnailUrl(proceduralTexture(QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size())), 256));
            }
            entry[QStringLiteral("thumbnail")] = url;
        }
        out.append(entry);
    }
    return out;
}

QVariantList Document::modelMaterials() const
{
    std::map<model::MaterialId, int> counts;
    for (model::DefinitionId d : m_model.definitions().keys()) {
        for (const geom::Face& f : m_model.mesh(d).faces().values()) {
            if (f.material) {
                ++counts[f.material];
            }
        }
    }
    for (const model::Instance& inst : m_model.instances().values()) {
        if (inst.material) {
            ++counts[inst.material];
        }
    }
    QVariantList out;
    for (std::size_t i = 0; i < m_model.materials().size(); ++i) {
        const model::MaterialId id = m_model.materials().keys()[i];
        QVariantMap entry = materialEntry(m_model.materials().values()[i]);
        entry[QStringLiteral("id")] = static_cast<double>(id.value);
        entry[QStringLiteral("count")] = counts[id];
        const model::Material& m = m_model.materials().values()[i];
        entry[QStringLiteral("thumbnail")] = textureThumbnail(m.texture);
        entry[QStringLiteral("textureSource")] = m.texture && m_model.textures().contains(m.texture)
            ? QString::fromStdString(m_model.textures().at(m.texture).source) : QString();
        entry[QStringLiteral("textureWidth")] = m.textureSize.x;
        entry[QStringLiteral("textureHeight")] = m.textureSize.y;
        out.append(entry);
    }
    return out;
}

QVariantMap Document::currentMaterial() const
{
    if (m_libraryMaterial >= 0) {
        QVariantMap entry = materialEntry(model::builtinMaterials()[static_cast<std::size_t>(m_libraryMaterial)]);
        entry[QStringLiteral("source")] = QStringLiteral("library");
        entry[QStringLiteral("index")] = m_libraryMaterial;
        return entry;
    }
    if (m_currentMaterial && m_model.materials().contains(m_currentMaterial)) {
        QVariantMap entry = materialEntry(m_model.material(m_currentMaterial));
        entry[QStringLiteral("source")] = QStringLiteral("model");
        entry[QStringLiteral("id")] = static_cast<double>(m_currentMaterial.value);
        return entry;
    }
    return QVariantMap { { QStringLiteral("source"), QStringLiteral("default") },
        { QStringLiteral("name"), QStringLiteral("Implicit") }, { QStringLiteral("color"), QColor(0, 0, 0, 0) } };
}

void Document::setPlacementLock(int bit, bool locked)
{
    if (m_selection.instances.size() != 1 || bit < 0 || bit > 11) {
        return;
    }
    const model::InstanceId id = *m_selection.instances.begin();
    runOnModel(locked ? "Blochează valoarea" : "Deblochează valoarea", [&](model::Model& model) {
        model::Instance inst = model.instance(id);
        const auto mask = static_cast<std::uint16_t>(1u << bit);
        inst.placementLocks = static_cast<std::uint16_t>(locked ? (inst.placementLocks | mask) : (inst.placementLocks & ~mask));
        model.setInstance(id, inst);
    });
}

bool Document::hasTransformKey(double instanceId) const
{
    const model::Animation* a = currentAnimation();
    if (a == nullptr) {
        return false;
    }
    const model::InstanceId id { static_cast<std::uint64_t>(instanceId) };
    for (const model::ObjectTrack& track : a->tracks) {
        if (track.instance == id) {
            return std::any_of(track.transform.begin(), track.transform.end(),
                [&](const model::TransformKey& k) { return std::abs(k.time - m_timelineTime) < 1e-3; });
        }
    }
    return false;
}

void Document::setPlacement(const QVariantMap& placement)
{
    if (m_selection.instances.size() != 1) {
        return;
    }
    const model::InstanceId id = *m_selection.instances.begin();
    model::Placement p = model::placementOf(m_model, id);
    const auto value = [&](const char* key, double fallback) {
        return placement.value(QLatin1String(key), fallback).toDouble();
    };
    p.position = { value("px", p.position.x), value("py", p.position.y), value("pz", p.position.z) };
    p.rotation.x = value("rx", p.rotation.x);
    p.rotation.y = value("ry", p.rotation.y);
    p.rotation.z = value("rz", value("rotation", p.rotation.z));
    p.scale = { value("sx", p.scale.x), value("sy", p.scale.y), value("sz", p.scale.z) };
    // Dimensiunile se aplică doar dacă au fost cerute explicit (altfel ar anula o scară tocmai schimbată).
    p.size = { value("dx", 0.0), value("dy", 0.0), value("dz", 0.0) };
    runOnModel("Transformare", [&](model::Model& model) { model::setPlacement(model, id, p); });
}

namespace {

// Textura procedurală din model (o adaugă dacă lipsește): „procedural:brick” etc.
model::TextureId proceduralTextureIn(model::Model& model, const std::string& key)
{
    const std::string source = "procedural:" + key;
    for (std::size_t i = 0; i < model.textures().size(); ++i) {
        if (model.textures().values()[i].source == source) {
            return model.textures().keys()[i];
        }
    }
    const QByteArray png = proceduralTexturePng(QString::fromStdString(key));
    model::Texture texture;
    texture.name = key;
    texture.source = source;
    texture.data.assign(reinterpret_cast<const std::uint8_t*>(png.constData()),
        reinterpret_cast<const std::uint8_t*>(png.constData()) + png.size());
    return model.addTexture(std::move(texture));
}

// Materialul din bibliotecă, în model (după nume; cu textura lui, dacă are).
model::MaterialId libraryMaterialIn(model::Model& model, std::size_t index)
{
    const model::Material& lib = model::builtinMaterials()[index];
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        if (model.materials().values()[i].name == lib.name) {
            return model.materials().keys()[i];
        }
    }
    model::Material copy = lib;
    if (const std::string_view key = model::builtinTexture(index); !key.empty()) {
        copy.texture = proceduralTextureIn(model, std::string(key));
    }
    return model.addMaterial(copy);
}

} // namespace

void Document::assignMaterial(const QVariantMap& material, const QString& side)
{
    if (m_selection.empty()) {
        return;
    }
    const bool back = side == QLatin1String("back");
    runOnModel("Material", [&](model::Model& model) {
        model::MaterialId id { static_cast<std::uint64_t>(material.value(QStringLiteral("id"), 0.0).toDouble()) };
        if (material.contains(QStringLiteral("library"))) {
            const int index = material.value(QStringLiteral("library")).toInt();
            if (index >= 0 && index < static_cast<int>(model::builtinMaterials().size())) {
                id = libraryMaterialIn(model, static_cast<std::size_t>(index));
            }
        }
        if (id && !model.materials().contains(id)) {
            return;
        }
        for (model::InstanceId instance : m_selection.instances) {
            model::Instance inst = model.instance(instance);
            inst.material = id;
            model.setInstance(instance, inst);
        }
        geom::Mesh& mesh = model.mesh(activeDefinition());
        for (geom::FaceId face : m_selection.faces) {
            if (mesh.faces().contains(face)) {
                if (back) {
                    mesh.setFaceBackMaterial(face, id);
                } else {
                    mesh.setFaceMaterial(face, id);
                }
            }
        }
    });
}

void Document::selectLibraryMaterial(int index)
{
    if (index < 0 || index >= static_cast<int>(model::builtinMaterials().size())) {
        return;
    }
    // Dacă modelul are deja un material cu același nume, îl folosim pe acela.
    const std::string& name = model::builtinMaterials()[static_cast<std::size_t>(index)].name;
    for (std::size_t i = 0; i < m_model.materials().size(); ++i) {
        if (m_model.materials().values()[i].name == name) {
            selectModelMaterial(static_cast<double>(m_model.materials().keys()[i].value));
            return;
        }
    }
    // Cu textură: intră în model acum (Paint Bucket vopsește apoi cu un material din model).
    if (!model::builtinTexture(static_cast<std::size_t>(index)).empty()) {
        model::MaterialId added;
        runOnModel("Material din bibliotecă", [&](model::Model& model) {
            added = libraryMaterialIn(model, static_cast<std::size_t>(index));
        });
        selectModelMaterial(static_cast<double>(added.value));
        return;
    }
    m_libraryMaterial = index;
    m_currentMaterial = {};
    emit materialChanged();
}

void Document::selectModelMaterial(double materialId)
{
    const model::MaterialId id { static_cast<std::uint64_t>(materialId) };
    if (!m_model.materials().contains(id)) {
        return;
    }
    m_currentMaterial = id;
    m_libraryMaterial = -1;
    emit materialChanged();
}

namespace {

// Aplică cheile date (name, color, opacity, roughness, metallic) peste un material.
void applyMaterialProperties(model::Material& m, const QVariantMap& p)
{
    if (p.contains(QStringLiteral("name"))) {
        m.name = p.value(QStringLiteral("name")).toString().trimmed().toStdString();
    }
    if (p.contains(QStringLiteral("color"))) {
        const QColor c(p.value(QStringLiteral("color")).toString());
        if (c.isValid()) {
            m.color.r = static_cast<std::uint8_t>(c.red());
            m.color.g = static_cast<std::uint8_t>(c.green());
            m.color.b = static_cast<std::uint8_t>(c.blue());
        }
    }
    if (p.contains(QStringLiteral("opacity"))) {
        m.color.a = static_cast<std::uint8_t>(std::lround(std::clamp(p.value(QStringLiteral("opacity")).toDouble(), 0.0, 1.0) * 255.0));
    }
    if (p.contains(QStringLiteral("roughness"))) {
        m.roughness = static_cast<float>(std::clamp(p.value(QStringLiteral("roughness")).toDouble(), 0.0, 1.0));
    }
    if (p.contains(QStringLiteral("metallic"))) {
        m.metallic = static_cast<float>(std::clamp(p.value(QStringLiteral("metallic")).toDouble(), 0.0, 1.0));
    }
    if (p.contains(QStringLiteral("textureWidth"))) {
        m.textureSize.x = std::max(1.0, p.value(QStringLiteral("textureWidth")).toDouble());
    }
    if (p.contains(QStringLiteral("textureHeight"))) {
        m.textureSize.y = std::max(1.0, p.value(QStringLiteral("textureHeight")).toDouble());
    }
}

// Textura cerută de editor: "" = fără, „procedural:brick”, „file:/cale/imagine.jpg”. nullopt = neschimbată.
std::optional<model::TextureId> textureFromProperty(model::Model& model, const QVariantMap& p)
{
    if (!p.contains(QStringLiteral("texture"))) {
        return std::nullopt;
    }
    const QString value = p.value(QStringLiteral("texture")).toString();
    if (value.isEmpty()) {
        return model::TextureId {};
    }
    if (value.startsWith(QLatin1String("procedural:"))) {
        return proceduralTextureIn(model, value.mid(11).toStdString());
    }
    QString path = value.startsWith(QLatin1String("file:")) ? value.mid(5) : value;
    if (path.startsWith(QLatin1String("//"))) {
        path = QUrl(QStringLiteral("file:") + path).toLocalFile();
    }
    for (std::size_t i = 0; i < model.textures().size(); ++i) {
        if (model.textures().values()[i].source == path.toStdString()) {
            return model.textures().keys()[i];
        }
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    if (QImage::fromData(bytes).isNull()) {
        return std::nullopt;
    }
    model::Texture texture;
    texture.name = QFileInfo(path).completeBaseName().toStdString();
    texture.source = path.toStdString();
    texture.data.assign(reinterpret_cast<const std::uint8_t*>(bytes.constData()),
        reinterpret_cast<const std::uint8_t*>(bytes.constData()) + bytes.size());
    return model.addTexture(std::move(texture));
}

} // namespace

double Document::createMaterial(const QVariantMap& properties)
{
    model::MaterialId added;
    runOnModel("Material nou", [&](model::Model& model) {
        model::Material m;
        m.name = "Material " + std::to_string(model.materials().size() + 1);
        applyMaterialProperties(m, properties);
        if (const auto texture = textureFromProperty(model, properties)) {
            m.texture = *texture;
        }
        if (m.name.empty()) {
            m.name = "Material " + std::to_string(model.materials().size() + 1);
        }
        added = model.addMaterial(m);
    });
    selectModelMaterial(static_cast<double>(added.value));
    return static_cast<double>(added.value);
}

void Document::updateMaterial(double materialId, const QVariantMap& properties)
{
    const model::MaterialId id { static_cast<std::uint64_t>(materialId) };
    if (!m_model.materials().contains(id)) {
        return;
    }
    model::Material m = m_model.material(id);
    applyMaterialProperties(m, properties);
    if (m.name.empty() || (m == m_model.material(id) && !properties.contains(QStringLiteral("texture")))) {
        return;
    }
    runOnModel("Proprietățile materialului", [&](model::Model& model) {
        if (const auto texture = textureFromProperty(model, properties)) {
            m.texture = *texture;
        }
        model.setMaterial(id, m);
    });
    emit materialChanged();
}

void Document::selectDefaultMaterial()
{
    m_currentMaterial = {};
    m_libraryMaterial = -1;
    emit materialChanged();
}

void Document::renameMaterial(double materialId, const QString& name)
{
    const model::MaterialId id { static_cast<std::uint64_t>(materialId) };
    if (!m_model.materials().contains(id) || name.trimmed().isEmpty()) {
        return;
    }
    runOnModel("Redenumește materialul", [&](model::Model& model) {
        model::Material m = model.material(id);
        m.name = name.trimmed().toStdString();
        model.setMaterial(id, m);
    });
    emit materialChanged();
}

void Document::setMaterialColor(double materialId, const QColor& color)
{
    const model::MaterialId id { static_cast<std::uint64_t>(materialId) };
    if (!m_model.materials().contains(id)) {
        return;
    }
    runOnModel("Culoarea materialului", [&](model::Model& model) {
        model::Material m = model.material(id);
        m.color = { static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
            static_cast<std::uint8_t>(color.blue()), static_cast<std::uint8_t>(color.alpha()) };
        model.setMaterial(id, m);
    });
    emit materialChanged();
}

void Document::removeMaterial(double materialId)
{
    const model::MaterialId id { static_cast<std::uint64_t>(materialId) };
    if (!m_model.materials().contains(id)) {
        return;
    }
    runOnModel("Șterge materialul", [&](model::Model& model) { model.removeMaterial(id); });
    if (m_currentMaterial == id) {
        selectDefaultMaterial();
    }
}

QVariantList Document::scenes() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.scenes().size(); ++i) {
        const model::Scene& s = m_model.scenes().values()[i];
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(m_model.scenes().keys()[i].value) },
            { QStringLiteral("name"), QString::fromStdString(s.name) },
            { QStringLiteral("style"), QString::fromStdString(s.style) } });
    }
    return out;
}

QVariantList Document::sections() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.sections().size(); ++i) {
        const model::SectionPlane& s = m_model.sections().values()[i];
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(m_model.sections().keys()[i].value) },
            { QStringLiteral("name"), QString::fromStdString(s.name) }, { QStringLiteral("active"), s.active } });
    }
    return out;
}

void Document::setEditingAnnotation(double id)
{
    const model::AnnotationId annotation { static_cast<std::uint64_t>(id) };
    if (annotation == m_editingAnnotation) {
        return;
    }
    m_editingAnnotation = annotation;
    emit editingAnnotationChanged();
}

void Document::intersectSelection(bool withModel)
{
    if (m_selection.faces.empty()) {
        setError(QStringLiteral("Selectează fețele de intersectat."));
        return;
    }
    std::size_t count = 0;
    runOnModel(withModel ? "Intersectează cu modelul" : "Intersectează selecția", [&](model::Model& model) {
        count = geom::intersectFaces(model.mesh(activeDefinition()), m_selection.faces, withModel);
    });
    if (count == 0) {
        setError(QStringLiteral("Fețele selectate nu intersectează nimic."));
    }
}

void Document::booleanSelection(const QString& kind)
{
    if (m_selection.instances.size() != 2 || !m_selection.faces.empty() || !m_selection.edges.empty()) {
        setError(QStringLiteral("Selectează exact două grupuri sau obiecte."));
        return;
    }
    const model::InstanceId a = *m_selection.instances.begin();
    const model::InstanceId b = *std::next(m_selection.instances.begin());
    const model::BooleanKind k = kind == QLatin1String("union") ? model::BooleanKind::Union
        : kind == QLatin1String("subtract")                    ? model::BooleanKind::Subtract
                                                               : model::BooleanKind::Intersect;
    model::BooleanResult result;
    const char* name = k == model::BooleanKind::Union ? "Reuniune" : k == model::BooleanKind::Subtract ? "Scădere" : "Intersecție";
    runOnModel(name, [&](model::Model& model) {
        result = model::booleanOperation(model, a, b, k);
        m_selection.clear();
        if (result.instance) {
            m_selection.instances.insert(result.instance);
        }
    });
    if (!result.instance) {
        setError(QString::fromStdString(result.error));
    }
}

void Document::deleteGuides()
{
    if (m_model.guides().empty()) {
        return;
    }
    runOnModel("Șterge ghidajele", [&](model::Model& model) {
        for (model::GuideId id : std::vector(model.guides().keys())) {
            model.removeGuide(id);
        }
    });
}

void Document::setAnnotationText(double annotationId, const QString& text)
{
    const model::AnnotationId id { static_cast<std::uint64_t>(annotationId) };
    if (!m_model.annotations().contains(id)) {
        return;
    }
    if (m_model.annotations().at(id).text != text.toStdString()) {
        runOnModel("Editează textul", [&](model::Model& model) {
            model::Annotation a = model.annotations().at(id);
            a.text = text.toStdString();
            model.setAnnotation(id, a);
        });
    }
    setEditingAnnotation(0);
}

void Document::removeAnnotation(double annotationId)
{
    const model::AnnotationId id { static_cast<std::uint64_t>(annotationId) };
    if (m_model.annotations().contains(id)) {
        runOnModel("Șterge adnotarea", [&](model::Model& model) { model.removeAnnotation(id); });
    }
}

void Document::setSectionActive(double sectionId, bool active)
{
    const model::SectionId id { static_cast<std::uint64_t>(sectionId) };
    if (!m_model.sections().contains(id)) {
        return;
    }
    runOnModel(active ? "Activează secțiunea" : "Dezactivează secțiunea", [&](model::Model& model) {
        for (model::SectionId other : std::vector(model.sections().keys())) {
            model::SectionPlane s = model.sections().at(other);
            const bool wanted = other == id ? active : false;
            if (s.active != wanted) {
                s.active = wanted;
                model.setSection(other, s);
            }
        }
    });
}

void Document::moveSection(double sectionId, double distance)
{
    const model::SectionId id { static_cast<std::uint64_t>(sectionId) };
    if (!m_model.sections().contains(id)) {
        return;
    }
    runOnModel("Mută secțiunea", [&](model::Model& model) {
        model::SectionPlane s = model.sections().at(id);
        s.point += s.normal * distance;
        model.setSection(id, s);
    });
}

void Document::removeSection(double sectionId)
{
    const model::SectionId id { static_cast<std::uint64_t>(sectionId) };
    if (m_model.sections().contains(id)) {
        runOnModel("Șterge secțiunea", [&](model::Model& model) { model.removeSection(id); });
    }
}

namespace {

model::Scene sceneFrom(const QVariantMap& camera, const QString& style)
{
    model::Scene s;
    s.target = { camera.value(QStringLiteral("tx")).toDouble(), camera.value(QStringLiteral("ty")).toDouble(),
        camera.value(QStringLiteral("tz")).toDouble() };
    s.yaw = camera.value(QStringLiteral("yaw")).toDouble();
    s.pitch = camera.value(QStringLiteral("pitch")).toDouble();
    s.distance = camera.value(QStringLiteral("distance")).toDouble();
    s.orthographic = camera.value(QStringLiteral("ortho")).toBool();
    s.style = style.toStdString();
    return s;
}

} // namespace

void Document::addScene(const QVariantMap& camera, const QString& style)
{
    runOnModel("Scenă nouă", [&](model::Model& model) {
        model::Scene s = sceneFrom(camera, style);
        model::captureSceneState(model, s);
        s.name = "Scena " + std::to_string(model.scenes().size() + 1);
        model.addScene(s);
    });
}

void Document::updateScene(double sceneId, const QVariantMap& camera, const QString& style)
{
    const model::SceneId id { static_cast<std::uint64_t>(sceneId) };
    if (!m_model.scenes().contains(id)) {
        return;
    }
    runOnModel("Actualizează scena", [&](model::Model& model) {
        model::Scene s = sceneFrom(camera, style);
        model::captureSceneState(model, s);
        s.name = model.scenes().at(id).name;
        model.setScene(id, s);
    });
}

void Document::renameScene(double sceneId, const QString& name)
{
    const model::SceneId id { static_cast<std::uint64_t>(sceneId) };
    if (!m_model.scenes().contains(id) || name.trimmed().isEmpty()) {
        return;
    }
    runOnModel("Redenumește scena", [&](model::Model& model) {
        model::Scene s = model.scenes().at(id);
        s.name = name.trimmed().toStdString();
        model.setScene(id, s);
    });
}

void Document::removeScene(double sceneId)
{
    const model::SceneId id { static_cast<std::uint64_t>(sceneId) };
    if (m_model.scenes().contains(id)) {
        runOnModel("Șterge scena", [&](model::Model& model) { model.removeScene(id); });
    }
}

QVariantMap Document::sceneCamera(double sceneId) const
{
    const model::SceneId id { static_cast<std::uint64_t>(sceneId) };
    if (!m_model.scenes().contains(id)) {
        return {};
    }
    const model::Scene& s = m_model.scenes().at(id);
    return QVariantMap { { QStringLiteral("tx"), s.target.x }, { QStringLiteral("ty"), s.target.y },
        { QStringLiteral("tz"), s.target.z }, { QStringLiteral("yaw"), s.yaw }, { QStringLiteral("pitch"), s.pitch },
        { QStringLiteral("distance"), s.distance }, { QStringLiteral("ortho"), s.orthographic },
        { QStringLiteral("style"), QString::fromStdString(s.style) } };
}

void Document::reverseSelectedFaces()
{
    if (m_selection.faces.empty()) {
        return;
    }
    runOnModel(m_selection.faces.size() == 1 ? "Inversează fața" : "Inversează fețele", [&](model::Model& model) {
        geom::Mesh& mesh = model.mesh(activeDefinition());
        std::set<geom::FaceId> flipped;
        for (geom::FaceId f : m_selection.faces) {
            if (mesh.faces().contains(f)) {
                flipped.insert(geom::orientFace(mesh, f, -mesh.face(f).plane.normal));
            }
        }
        m_selection.faces = flipped;
    });
}

void Document::setFaceTexture(const QVariantMap& changes)
{
    if (m_selection.faces.empty()) {
        return;
    }
    runOnModel("Poziționează textura", [&](model::Model& model) {
        geom::Mesh& mesh = model.mesh(activeDefinition());
        for (geom::FaceId f : m_selection.faces) {
            if (!mesh.faces().contains(f)) {
                continue;
            }
            geom::TextureTransform t = changes.value(QStringLiteral("reset")).toBool() ? geom::TextureTransform {}
                                                                                      : mesh.face(f).texture;
            const auto value = [&](const char* key, double fallback) {
                return changes.value(QLatin1String(key), fallback).toDouble();
            };
            t.offset = { value("ox", t.offset.x), value("oy", t.offset.y) };
            t.rotation = value("rotation", t.rotation);
            t.scale = { std::max(value("sx", t.scale.x), 1e-3), std::max(value("sy", t.scale.y), 1e-3) };
            mesh.setFaceTexture(f, t);
        }
    });
}

QVariantList Document::generators() const
{
    QVariantList out;
    for (const model::GeneratorInfo& g : model::generators()) {
        QVariantList parameters;
        for (const model::ParameterSpec& spec : g.parameters) {
            parameters.append(QVariantMap { { QStringLiteral("key"), QString::fromStdString(spec.key) },
                { QStringLiteral("label"), QString::fromStdString(spec.label) },
                { QStringLiteral("unit"), QString::fromStdString(spec.unit) }, { QStringLiteral("value"), spec.value },
                { QStringLiteral("min"), spec.min }, { QStringLiteral("max"), spec.max }, { QStringLiteral("step"), spec.step } });
        }
        out.append(QVariantMap { { QStringLiteral("id"), QString::fromStdString(g.id) },
            { QStringLiteral("name"), QString::fromStdString(g.name) },
            { QStringLiteral("category"), QString::fromStdString(g.category) }, { QStringLiteral("parameters"), parameters } });
    }
    return out;
}

double Document::createParametric(const QString& generator, const QVector3D& at, const QVariantMap& parameters)
{
    const model::GeneratorInfo* info = model::findGenerator(generator.toStdString());
    if (info == nullptr) {
        return 0.0;
    }
    std::map<std::string, double> given;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        given[it.key().toStdString()] = it.value().toDouble();
    }
    // Punctul dat, în coordonatele contextului editat.
    const math::Vec3d point = contextTransform().inverse().applyToPoint({ at.x(), at.y(), at.z() });
    model::InstanceId created;
    runOnModel("Obiect dinamic: " + info->name, [&](model::Model& model) {
        created = model::createParametric(model, activeDefinition(), info->id, given, math::Transform::translation(point));
    });
    if (created) {
        const geom::Selection before = m_selection;
        m_selection.clear();
        m_selection.instances.insert(created);
        afterInteraction(before);
    }
    return static_cast<double>(created.value);
}

void Document::beginMacro(const std::string& name)
{
    if (m_macro || m_stack.inTransaction()) {
        return;
    }
    m_stack.beginTransaction(name);
    m_macro = true;
}

void Document::endMacro(bool keep)
{
    if (!m_macro) {
        return;
    }
    m_macro = false;
    const geom::Selection before = m_selection;
    if (keep) {
        m_stack.commit();
    } else {
        m_stack.abort();
    }
    dropStaleReferences();
    afterInteraction(before);
    emit selectionChanged();
    emit outlineChanged();
}

namespace {

model::InstanceId instanceIdOf(double id)
{
    return model::InstanceId { static_cast<std::uint64_t>(id) };
}

} // namespace

double Document::createPrimitive(const QString& kind, const QVector3D& corner, const QVector3D& size, const QString& name)
{
    static const QStringList kinds { QStringLiteral("box"), QStringLiteral("cylinder"), QStringLiteral("cone"),
        QStringLiteral("sphere") };
    static const char* const kNames[] = { "Cutie", "Cilindru", "Con", "Sferă" };
    const auto index = kinds.indexOf(kind);
    const math::Vec3d s { std::abs(size.x()), std::abs(size.y()), std::abs(size.z()) };
    if (index < 0 || s.x < 0.01 || s.y < 0.01 || s.z < 0.01) {
        return 0.0;
    }
    const math::Vec3d c = contextTransform().inverse().applyToPoint({ corner.x(), corner.y(), corner.z() });
    model::InstanceId created;
    runOnModel("Primitivă", [&](model::Model& model) {
        model::Definition def { name.isEmpty() ? std::string(kNames[index]) : name.toStdString(), true };
        def.shape = kind.toStdString();
        const model::DefinitionId d = model.addDefinition(def);
        geom::buildPrimitive(model.mesh(d), static_cast<geom::Primitive>(index), s, 24);
        model::Instance inst;
        inst.parent = activeDefinition();
        inst.definition = d;
        // Primitivele au baza centrată în origine: colțul minim ajunge în `corner`.
        inst.transform = math::Transform::translation(c + math::Vec3d { s.x / 2, s.y / 2, 0.0 });
        inst.name = def.name;
        created = model.addInstance(inst);
    });
    return static_cast<double>(created.value);
}

bool Document::setInstancePlacement(double instanceId, const QVariantMap& placement)
{
    const model::InstanceId id = instanceIdOf(instanceId);
    if (!m_model.instances().contains(id)) {
        return false;
    }
    model::Placement p = model::placementOf(m_model, id);
    const auto value = [&](const char* key, double fallback) {
        return placement.value(QLatin1String(key), fallback).toDouble();
    };
    p.position = { value("x", p.position.x), value("y", p.position.y), value("z", p.position.z) };
    p.rotation = { value("rx", p.rotation.x), value("ry", p.rotation.y), value("rz", p.rotation.z) };
    p.scale = { value("sx", p.scale.x), value("sy", p.scale.y), value("sz", p.scale.z) };
    p.size = { value("width", 0.0), value("depth", 0.0), value("height", 0.0) };
    runOnModel("Plasare", [&](model::Model& model) { model::setPlacement(model, id, p); });
    return true;
}

bool Document::transformInstance(double instanceId, const math::Transform& delta)
{
    const model::InstanceId id = instanceIdOf(instanceId);
    if (!m_model.instances().contains(id)) {
        return false;
    }
    runOnModel("Transformare", [&](model::Model& model) { model::transformInstances(model, { id }, delta); });
    return true;
}

bool Document::removeInstanceById(double instanceId)
{
    const model::InstanceId id = instanceIdOf(instanceId);
    if (!m_model.instances().contains(id)) {
        return false;
    }
    geom::Selection before = m_selection;
    m_selection.clear();
    m_selection.instances.insert(id);
    deleteSelection();
    before.instances.erase(id);
    m_selection = before;
    return true;
}

bool Document::setInstanceMaterialByName(double instanceId, const QString& nameOrColor)
{
    const model::InstanceId id = instanceIdOf(instanceId);
    if (!m_model.instances().contains(id)) {
        return false;
    }
    runOnModel("Material", [&](model::Model& model) {
        model::MaterialId material;
        for (std::size_t i = 0; i < model.materials().size(); ++i) {
            if (QString::fromStdString(model.materials().values()[i].name) == nameOrColor) {
                material = model.materials().keys()[i];
            }
        }
        if (!material) {
            const auto library = model::builtinMaterials();
            for (std::size_t i = 0; i < library.size(); ++i) {
                if (QString::fromStdString(library[i].name) == nameOrColor) {
                    material = libraryMaterialIn(model, i);
                }
            }
        }
        if (!material && QColor::isValidColorName(nameOrColor)) {
            const QColor c = QColor::fromString(nameOrColor);
            model::Material m;
            m.name = nameOrColor.toStdString();
            m.color = { static_cast<std::uint8_t>(c.red()), static_cast<std::uint8_t>(c.green()),
                static_cast<std::uint8_t>(c.blue()), static_cast<std::uint8_t>(c.alpha()) };
            material = model.addMaterial(m);
        }
        model::Instance inst = model.instance(id);
        inst.material = material;
        model.setInstance(id, inst);
    });
    return true;
}

bool Document::drawLineSegment(const QVector3D& a, const QVector3D& b)
{
    const math::Transform toLocal = contextTransform().inverse();
    const math::Vec3d p = toLocal.applyToPoint({ a.x(), a.y(), a.z() });
    const math::Vec3d q = toLocal.applyToPoint({ b.x(), b.y(), b.z() });
    if (glm::length(p - q) < 1e-6) {
        return false;
    }
    runOnModel("Linie", [&](model::Model& model) { geom::drawLine(model.mesh(activeDefinition()), p, q); });
    return true;
}

bool Document::drawFace(const QVariantList& points)
{
    const math::Transform toLocal = contextTransform().inverse();
    std::vector<math::Vec3d> polygon;
    for (const QVariant& v : points) {
        const QVariantList xyz = v.toList();
        if (xyz.size() >= 3) {
            polygon.push_back(toLocal.applyToPoint({ xyz[0].toDouble(), xyz[1].toDouble(), xyz[2].toDouble() }));
        }
    }
    if (polygon.size() < 3) {
        return false;
    }
    runOnModel("Față", [&](model::Model& model) { geom::drawPolygon(model.mesh(activeDefinition()), polygon); });
    return true;
}

QVariantList Document::objectList() const
{
    QVariantList out;
    for (model::InstanceId id : m_model.childrenOf(activeDefinition())) {
        const model::Instance& inst = m_model.instance(id);
        const model::Placement p = model::placementOf(m_model, id);
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(id.value) },
            { QStringLiteral("name"), QString::fromStdString(inst.name.empty() ? m_model.definition(inst.definition).name : inst.name) },
            { QStringLiteral("x"), p.position.x }, { QStringLiteral("y"), p.position.y }, { QStringLiteral("z"), p.position.z },
            { QStringLiteral("width"), p.size.x }, { QStringLiteral("depth"), p.size.y }, { QStringLiteral("height"), p.size.z } });
    }
    return out;
}

void Document::selectInstances(const QVariantList& ids)
{
    const geom::Selection before = m_selection;
    m_selection.clear();
    for (const QVariant& v : ids) {
        const model::InstanceId id = instanceIdOf(v.toDouble());
        if (m_model.instances().contains(id) && m_model.instance(id).parent == activeDefinition()) {
            m_selection.instances.insert(id);
        }
    }
    afterInteraction(before);
}

QString Document::libraryFolder() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Biblioteca");
    QDir().mkpath(dir);
    return dir;
}

QVariantList Document::libraryItems() const
{
    QVariantList out;
    for (const model::GeneratorInfo& g : model::generators()) {
        out.append(QVariantMap { { QStringLiteral("id"), QStringLiteral("gen:") + QString::fromStdString(g.id) },
            { QStringLiteral("name"), QString::fromStdString(g.name) },
            { QStringLiteral("category"), QString::fromStdString(g.category) }, { QStringLiteral("user"), false },
            { QStringLiteral("dynamic"), true } });
    }
    const QDir dir(libraryFolder());
    for (const QFileInfo& file : dir.entryInfoList({ QStringLiteral("*.aion") }, QDir::Files, QDir::Name)) {
        QFile f(file.absoluteFilePath());
        QString thumbnail;
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = f.readAll();
            const std::vector<std::uint8_t> png = io::readThumbnail(
                std::span(reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size())));
            if (!png.empty()) {
                thumbnail = QStringLiteral("data:image/png;base64,")
                    + QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(png.data()), qsizetype(png.size())).toBase64());
            }
        }
        out.append(QVariantMap { { QStringLiteral("id"), QStringLiteral("file:") + file.absoluteFilePath() },
            { QStringLiteral("name"), file.completeBaseName() }, { QStringLiteral("category"), QStringLiteral("Ale mele") },
            { QStringLiteral("thumbnail"), thumbnail }, { QStringLiteral("user"), true }, { QStringLiteral("dynamic"), false } });
    }
    return out;
}

void Document::placeLibraryItem(const QString& id, const QVector3D& at)
{
    if (id.startsWith(QLatin1String("gen:"))) {
        createParametric(id.mid(4), at);
        return;
    }
    if (!id.startsWith(QLatin1String("file:"))) {
        return;
    }
    const QString path = id.mid(5);
    const io::LoadResult loaded = io::loadFile(toPath(path));
    if (!loaded.model) {
        setError(QStringLiteral("„%1” nu a putut fi citit. %2").arg(QFileInfo(path).fileName(),
            QString::fromStdString(loaded.error)));
        return;
    }
    const math::Vec3d point = contextTransform().inverse().applyToPoint({ at.x(), at.y(), at.z() });
    const std::string name = QFileInfo(path).completeBaseName().toStdString();
    model::InstanceId created;
    runOnModel("Din bibliotecă: " + name, [&](model::Model& model) {
        created = model::insertModel(model, activeDefinition(), *loaded.model, name, math::Transform::translation(point));
    });
    if (created) {
        const geom::Selection before = m_selection;
        m_selection.clear();
        m_selection.instances.insert(created);
        afterInteraction(before);
    }
}

bool Document::saveSelectionToLibrary(const QString& name, const QByteArray& thumbnailPng)
{
    if (m_selection.instances.size() != 1 || name.trimmed().isEmpty()) {
        setError(QStringLiteral("Selectează un singur obiect ca să-l salvezi în bibliotecă."));
        return false;
    }
    const model::Model extracted = model::extractInstance(m_model, *m_selection.instances.begin());
    QString safe = name.trimmed();
    safe.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("-"));
    const QString path = QDir(libraryFolder()).filePath(safe + QStringLiteral(".aion"));
    std::string error;
    const bool ok = io::saveFile(toPath(path), extracted,
        std::span(reinterpret_cast<const std::uint8_t*>(thumbnailPng.constData()), static_cast<std::size_t>(thumbnailPng.size())),
        &error);
    if (!ok) {
        setError(QStringLiteral("Obiectul nu a putut fi salvat în bibliotecă. %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit libraryChanged();
    return true;
}

QVariantMap Document::libraryItemSize(const QString& id) const
{
    if (const auto it = m_librarySizes.find(id); it != m_librarySizes.end()) {
        return it->second;
    }
    // Obiectul construit într-un model temporar (generatorul cu parametrii impliciți, sau fișierul).
    std::optional<model::Model> temp;
    if (id.startsWith(QLatin1String("gen:"))) {
        temp.emplace();
        model::createParametric(*temp, temp->root(), id.mid(4).toStdString(), {}, {});
    } else if (id.startsWith(QLatin1String("file:"))) {
        io::LoadResult loaded = io::loadFile(toPath(id.mid(5)));
        if (loaded.model) {
            temp = std::move(*loaded.model);
        }
    }
    QVariantMap size;
    if (temp) {
        const auto [lo, hi] = model::bounds(*temp, temp->root());
        if (lo.x <= hi.x) {
            size = { { QStringLiteral("dx"), hi.x - lo.x }, { QStringLiteral("dy"), hi.y - lo.y },
                { QStringLiteral("dz"), hi.z - lo.z } };
        }
    }
    m_librarySizes[id] = size;
    return size;
}

void Document::removeLibraryItem(const QString& id)
{
    if (id.startsWith(QLatin1String("file:")) && QFile::remove(id.mid(5))) {
        emit libraryChanged();
    }
}

void Document::setParameter(const QString& key, double value)
{
    if (m_selection.instances.size() != 1) {
        return;
    }
    const model::DefinitionId def = m_model.instance(*m_selection.instances.begin()).definition;
    if (m_model.definition(def).generator.empty()) {
        return;
    }
    const auto it = m_model.definition(def).parameters.find(key.toStdString());
    if (it != m_model.definition(def).parameters.end() && std::abs(it->second - value) < 1e-9) {
        return;
    }
    runOnModel("Parametru", [&](model::Model& model) { model::setParameter(model, def, key.toStdString(), value); });
}

void Document::mirrorSelection(int axis)
{
    if (m_selection.empty()) {
        setError(QStringLiteral("Selectează ce vrei să oglindești."));
        return;
    }
    const auto [lo, hi] = model::selectionBounds(m_model, activeDefinition(), m_selection);
    if (lo.x > hi.x) {
        return;
    }
    static const char* const kNames[] = { "Oglindește pe X", "Oglindește pe Y", "Oglindește pe Z" };
    const int a = std::clamp(axis, 0, 2);
    const geom::Selection selection = m_selection;
    runOnModel(kNames[a], [&](model::Model& model) {
        m_selection = model::mirrorSelection(model, activeDefinition(), selection, a, (lo + hi) * 0.5);
    });
}

QVariantMap Document::selectionBox() const
{
    const geom::Selection& s = m_previewApplied ? m_previewSelection : m_selection;
    const auto [lo, hi] = model::selectionBounds(m_model, activeDefinition(), s);
    if (lo.x > hi.x) {
        return {};
    }
    const math::Vec3d c = (lo + hi) * 0.5;
    const math::Vec3d d = hi - lo;
    return { { QStringLiteral("x"), c.x }, { QStringLiteral("y"), c.y }, { QStringLiteral("z"), c.z },
        { QStringLiteral("dx"), d.x }, { QStringLiteral("dy"), d.y }, { QStringLiteral("dz"), d.z } };
}

void Document::previewArray(const QVariantMap& options)
{
    if (!m_previewApplied && m_selection.empty()) {
        setError(QStringLiteral("Selectează obiectele sau geometria pe care vrei s-o multiplici."));
        return;
    }
    if (m_previewApplied) {
        m_stack.undo();
        m_selection = m_previewSelection;
    } else {
        m_previewSelection = m_selection;
    }
    const int count = std::clamp(options.value(QStringLiteral("count"), 3).toInt(), 1, 500);
    const geom::Selection source = m_previewSelection;
    const bool polar = options.value(QStringLiteral("mode")).toString() == QStringLiteral("polar");
    runOnModel(polar ? "Array polar" : "Array liniar", [&](model::Model& model) {
        if (polar) {
            math::Vec3d axis { 0.0 };
            axis[std::clamp(options.value(QStringLiteral("axis"), 2).toInt(), 0, 2)] = 1.0;
            math::Vec3d center { 0.0 };
            if (!options.value(QStringLiteral("aroundOrigin")).toBool()) {
                // În jurul unui punct la o latură de selecție, ca obiectele să se rotească „în cerc” (scaune la masă).
                const auto [lo, hi] = model::selectionBounds(model, activeDefinition(), source);
                center = (lo + hi) * 0.5;
                const int sideAxis = axis.x != 0.0 ? 1 : 0;
                center[sideAxis] = lo[sideAxis] - options.value(QStringLiteral("radius"), 1000.0).toDouble();
            }
            model::polarArray(model, activeDefinition(), source, center, axis,
                glm::radians(options.value(QStringLiteral("angle"), 90.0).toDouble()), count,
                options.value(QStringLiteral("rotate"), true).toBool());
        } else {
            const math::Vec3d step { options.value(QStringLiteral("dx")).toDouble(), options.value(QStringLiteral("dy")).toDouble(),
                options.value(QStringLiteral("dz")).toDouble() };
            model::linearArray(model, activeDefinition(), source, step, count);
        }
    });
    m_selection = source;
    m_previewApplied = true;
    emit selectionChanged();
}

void Document::previewSoften(double maxAngleDegrees, bool includeCoplanar)
{
    if (!m_previewApplied && m_selection.empty()) {
        setError(QStringLiteral("Selectează muchiile, fețele sau obiectele de netezit."));
        return;
    }
    if (m_previewApplied) {
        m_stack.undo();
        m_selection = m_previewSelection;
    } else {
        m_previewSelection = m_selection;
    }
    const geom::Selection source = m_previewSelection;
    runOnModel("Soften / Smooth", [&](model::Model& model) {
        // Muchiile selectate și ale fețelor selectate, în context.
        geom::Mesh& mesh = model.mesh(activeDefinition());
        std::set<geom::EdgeId> edges = source.edges;
        for (geom::FaceId f : source.faces) {
            if (!mesh.faces().contains(f)) {
                continue;
            }
            for (const geom::EdgeUse& u : mesh.face(f).outer.uses) {
                edges.insert(u.edge);
            }
            for (const geom::Loop& h : mesh.face(f).inner) {
                for (const geom::EdgeUse& u : h.uses) {
                    edges.insert(u.edge);
                }
            }
        }
        model::softenEdges(mesh, edges, maxAngleDegrees, includeCoplanar);
        // Obiectele selectate: toate muchiile din definiția lor.
        std::set<model::DefinitionId> definitions;
        for (model::InstanceId id : source.instances) {
            definitions.insert(model.instance(id).definition);
        }
        for (model::DefinitionId d : definitions) {
            geom::Mesh& inner = model.mesh(d);
            const std::set<geom::EdgeId> all(inner.edges().keys().begin(), inner.edges().keys().end());
            model::softenEdges(inner, all, maxAngleDegrees, includeCoplanar);
        }
    });
    m_selection = source;
    m_previewApplied = true;
    emit selectionChanged();
}

void Document::commitOperation()
{
    m_previewApplied = false;
}

void Document::cancelOperation()
{
    if (m_previewApplied) {
        m_stack.undo();
        m_selection = m_previewSelection;
        m_previewApplied = false;
        emit selectionChanged();
        emit outlineChanged();
        touch();
    }
}

void Document::selectConnected()
{
    const geom::Selection before = m_selection;
    const geom::Selection seed { m_selection.edges, m_selection.faces, {} };
    geom::Selection connected = geom::connectedTo(m_model.mesh(activeDefinition()), seed);
    connected.instances = m_selection.instances;
    m_selection = connected;
    afterInteraction(before);
}

void Document::retriangulate(model::DefinitionId definition)
{
    auto& tris = m_triangles[definition];
    tris.clear();
    const geom::Mesh& mesh = m_model.mesh(definition);
    for (geom::FaceId f : mesh.faces().keys()) {
        tris[f] = geom::triangulateFace(mesh, f);
    }
}

void Document::meshChanged(model::DefinitionId definition, const geom::Mesh& mesh, const geom::ChangeSummary& summary)
{
    // Fețele adăugate sau modificate se retriangulează; la fel cele ale căror vârfuri s-au mutat.
    auto& tris = m_triangles[definition];
    std::set<geom::FaceId> dirty(summary.faces.added.begin(), summary.faces.added.end());
    dirty.insert(summary.faces.modified.begin(), summary.faces.modified.end());
    for (geom::VertexId v : summary.vertices.modified) {
        if (const geom::Vertex* vertex = mesh.vertices().find(v)) {
            for (geom::EdgeId e : vertex->edges) {
                for (geom::FaceId f : mesh.edge(e).faces) {
                    dirty.insert(f);
                }
            }
        }
    }
    for (geom::FaceId f : summary.faces.removed) {
        tris.erase(f);
    }
    for (geom::FaceId f : dirty) {
        if (mesh.faces().contains(f)) {
            tris[f] = geom::triangulateFace(mesh, f);
        }
    }
    emit modelChanged();
    touch();
}

// --- Camere și lumini ---

namespace {

const std::array<QString, 4> kLightKinds { QStringLiteral("point"), QStringLiteral("spot"), QStringLiteral("directional"),
    QStringLiteral("area") };

QString colorName(const model::Color& c)
{
    return QColor(c.r, c.g, c.b).name();
}

QVariantList vec3List(const math::Vec3d& v)
{
    return { v.x, v.y, v.z };
}

math::Vec3d vec3From(const QVariant& value, const math::Vec3d& fallback)
{
    const QVariantList l = value.toList();
    return l.size() == 3 ? math::Vec3d { l[0].toDouble(), l[1].toDouble(), l[2].toDouble() } : fallback;
}

// Ochiul și ținta vederii date ca { tx, ty, tz, yaw, pitch, distance }.
std::pair<math::Vec3d, math::Vec3d> eyeAndTarget(const QVariantMap& camera)
{
    const math::Vec3d target { camera.value(QStringLiteral("tx")).toDouble(), camera.value(QStringLiteral("ty")).toDouble(),
        camera.value(QStringLiteral("tz")).toDouble() };
    const double yaw = camera.value(QStringLiteral("yaw")).toDouble();
    const double pitch = camera.value(QStringLiteral("pitch")).toDouble();
    const double distance = camera.value(QStringLiteral("distance"), 10000.0).toDouble();
    const math::Vec3d forward { std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch) };
    return { target - forward * distance, target };
}

} // namespace

QVariantList Document::cameras() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.cameras().size(); ++i) {
        const model::CameraObject& c = m_model.cameras().values()[i];
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(m_model.cameras().keys()[i].value) },
            { QStringLiteral("name"), QString::fromStdString(c.name) }, { QStringLiteral("kind"), QStringLiteral("camera") },
            { QStringLiteral("visible"), c.visible }, { QStringLiteral("enabled"), true } });
    }
    return out;
}

QVariantList Document::lights() const
{
    QVariantList out;
    for (std::size_t i = 0; i < m_model.lights().size(); ++i) {
        const model::Light& l = m_model.lights().values()[i];
        out.append(QVariantMap { { QStringLiteral("id"), static_cast<double>(m_model.lights().keys()[i].value) },
            { QStringLiteral("name"), QString::fromStdString(l.name) },
            { QStringLiteral("kind"), kLightKinds[static_cast<std::size_t>(l.kind)] },
            { QStringLiteral("visible"), l.visible }, { QStringLiteral("enabled"), l.enabled },
            { QStringLiteral("color"), colorName(l.color) } });
    }
    return out;
}

QVariantMap Document::selectedObject() const
{
    if (const model::CameraObject* c = m_model.cameras().find(m_selectedCamera)) {
        return { { QStringLiteral("kind"), QStringLiteral("camera") },
            { QStringLiteral("id"), static_cast<double>(m_selectedCamera.value) },
            { QStringLiteral("name"), QString::fromStdString(c->name) },
            { QStringLiteral("position"), vec3List(c->position) }, { QStringLiteral("target"), vec3List(c->target) },
            { QStringLiteral("fov"), c->fov }, { QStringLiteral("orthographic"), c->orthographic },
            { QStringLiteral("nearClip"), c->nearClip }, { QStringLiteral("farClip"), c->farClip },
            { QStringLiteral("visible"), c->visible } };
    }
    if (const model::Light* l = m_model.lights().find(m_selectedLight)) {
        return { { QStringLiteral("kind"), QStringLiteral("light") },
            { QStringLiteral("id"), static_cast<double>(m_selectedLight.value) },
            { QStringLiteral("name"), QString::fromStdString(l->name) },
            { QStringLiteral("lightKind"), kLightKinds[static_cast<std::size_t>(l->kind)] },
            { QStringLiteral("position"), vec3List(l->position) }, { QStringLiteral("direction"), vec3List(l->direction) },
            { QStringLiteral("color"), colorName(l->color) }, { QStringLiteral("intensity"), l->intensity },
            { QStringLiteral("range"), l->range }, { QStringLiteral("cone"), l->cone },
            { QStringLiteral("softness"), l->softness }, { QStringLiteral("width"), l->width },
            { QStringLiteral("height"), l->height }, { QStringLiteral("castShadows"), l->castShadows },
            { QStringLiteral("enabled"), l->enabled }, { QStringLiteral("visible"), l->visible } };
    }
    return { { QStringLiteral("kind"), QStringLiteral("none") } };
}

void Document::addCamera(const QVariantMap& camera, double fovDegrees)
{
    model::CameraId added;
    runOnModel("Cameră nouă", [&](model::Model& model) {
        model::CameraObject c;
        std::tie(c.position, c.target) = eyeAndTarget(camera);
        c.fov = fovDegrees > 0.0 ? fovDegrees : camera.value(QStringLiteral("fov"), 35.0).toDouble();
        c.orthographic = camera.value(QStringLiteral("ortho")).toBool();
        c.name = "Cameră " + std::to_string(model.cameras().size() + 1);
        added = model.addCamera(c);
    });
    selectObject(QStringLiteral("camera"), static_cast<double>(added.value));
}

void Document::addLight(const QString& kind, const QVector3D& at)
{
    const auto index = static_cast<std::size_t>(std::max<qsizetype>(0, std::find(kLightKinds.begin(), kLightKinds.end(), kind) - kLightKinds.begin()));
    model::LightId added;
    runOnModel("Lumină nouă", [&](model::Model& model) {
        model::Light l;
        l.kind = static_cast<model::Light::Kind>(std::min<std::size_t>(index, 3));
        const math::Vec3d ground { at.x(), at.y(), at.z() };
        switch (l.kind) {
        case model::Light::Kind::Point:
            l.name = "Lumină punctuală";
            l.position = ground + math::Vec3d { 0.0, 0.0, 2200.0 };
            break;
        case model::Light::Kind::Spot:
            l.name = "Spot";
            l.position = ground + math::Vec3d { -1500.0, -1500.0, 3000.0 };
            l.direction = glm::normalize(ground - l.position);
            l.range = 9000.0;
            break;
        case model::Light::Kind::Directional:
            l.name = "Lumină direcțională";
            l.position = ground + math::Vec3d { 0.0, 0.0, 4000.0 };
            l.direction = glm::normalize(math::Vec3d { 0.5, 0.3, -1.0 });
            l.intensity = 0.6;
            break;
        case model::Light::Kind::Area:
            l.name = "Panou luminos";
            l.position = ground + math::Vec3d { 0.0, 0.0, 2600.0 };
            l.color = { 255, 255, 255, 255 };
            break;
        }
        l.name += " " + std::to_string(model.lights().size() + 1);
        added = model.addLight(l);
    });
    selectObject(QStringLiteral("light"), static_cast<double>(added.value));
}

void Document::selectObject(const QString& kind, double id)
{
    m_selectedCamera = {};
    m_selectedLight = {};
    if (kind == QStringLiteral("camera")) {
        m_selectedCamera = model::CameraId { static_cast<std::uint64_t>(id) };
    } else if (kind == QStringLiteral("light")) {
        m_selectedLight = model::LightId { static_cast<std::uint64_t>(id) };
    }
    if (m_selectedCamera || m_selectedLight) {
        m_selection.clear();
    }
    emit selectionChanged();
    emit outlineChanged();
    touch();
}

void Document::setObjectProperties(const QVariantMap& changes)
{
    const auto num = [&](const char* key, double& value) {
        if (changes.contains(QLatin1String(key))) {
            value = changes.value(QLatin1String(key)).toDouble();
        }
    };
    const auto flag = [&](const char* key, bool& value) {
        if (changes.contains(QLatin1String(key))) {
            value = changes.value(QLatin1String(key)).toBool();
        }
    };
    if (const model::CameraObject* found = m_model.cameras().find(m_selectedCamera)) {
        model::CameraObject c = *found;
        if (changes.contains(QStringLiteral("name"))) {
            c.name = changes.value(QStringLiteral("name")).toString().toStdString();
        }
        c.position = vec3From(changes.value(QStringLiteral("position")), c.position);
        c.target = vec3From(changes.value(QStringLiteral("target")), c.target);
        num("fov", c.fov);
        c.fov = std::clamp(c.fov, 5.0, 150.0);
        num("nearClip", c.nearClip);
        num("farClip", c.farClip);
        flag("orthographic", c.orthographic);
        flag("visible", c.visible);
        if (c == *found) {
            return;
        }
        const model::CameraId id = m_selectedCamera;
        runOnModel("Proprietățile camerei", [&](model::Model& model) { model.setCamera(id, c); });
        return;
    }
    if (const model::Light* found = m_model.lights().find(m_selectedLight)) {
        model::Light l = *found;
        if (changes.contains(QStringLiteral("name"))) {
            l.name = changes.value(QStringLiteral("name")).toString().toStdString();
        }
        if (changes.contains(QStringLiteral("lightKind"))) {
            const auto it = std::find(kLightKinds.begin(), kLightKinds.end(), changes.value(QStringLiteral("lightKind")).toString());
            if (it != kLightKinds.end()) {
                l.kind = static_cast<model::Light::Kind>(it - kLightKinds.begin());
            }
        }
        l.position = vec3From(changes.value(QStringLiteral("position")), l.position);
        const math::Vec3d direction = vec3From(changes.value(QStringLiteral("direction")), l.direction);
        if (glm::length(direction) > 1e-9) {
            l.direction = glm::normalize(direction);
        }
        if (changes.contains(QStringLiteral("color"))) {
            const QColor qc(changes.value(QStringLiteral("color")).toString());
            if (qc.isValid()) {
                l.color = { static_cast<std::uint8_t>(qc.red()), static_cast<std::uint8_t>(qc.green()),
                    static_cast<std::uint8_t>(qc.blue()), 255 };
            }
        }
        num("intensity", l.intensity);
        l.intensity = std::max(l.intensity, 0.0);
        num("range", l.range);
        l.range = std::max(l.range, 1.0);
        num("cone", l.cone);
        l.cone = std::clamp(l.cone, 1.0, 179.0);
        num("softness", l.softness);
        l.softness = std::clamp(l.softness, 0.0, 1.0);
        num("width", l.width);
        num("height", l.height);
        flag("castShadows", l.castShadows);
        flag("enabled", l.enabled);
        flag("visible", l.visible);
        if (l == *found) {
            return;
        }
        const model::LightId id = m_selectedLight;
        runOnModel("Proprietățile luminii", [&](model::Model& model) { model.setLight(id, l); });
    }
}

void Document::deleteSelectedObject()
{
    const model::CameraId camera = m_selectedCamera;
    const model::LightId light = m_selectedLight;
    if (m_model.cameras().contains(camera)) {
        runOnModel("Șterge camera", [&](model::Model& model) { model.removeCamera(camera); });
    } else if (m_model.lights().contains(light)) {
        runOnModel("Șterge lumina", [&](model::Model& model) { model.removeLight(light); });
    }
    selectObject(QString(), 0.0);
}

QVariantMap Document::cameraView(double cameraId) const
{
    const model::CameraObject* c = m_model.cameras().find(model::CameraId { static_cast<std::uint64_t>(cameraId) });
    if (c == nullptr) {
        return {};
    }
    const view::CameraPose pose = view::poseLookingAt(c->position, c->target);
    return { { QStringLiteral("tx"), pose.target.x }, { QStringLiteral("ty"), pose.target.y },
        { QStringLiteral("tz"), pose.target.z }, { QStringLiteral("yaw"), pose.yaw },
        { QStringLiteral("pitch"), pose.pitch }, { QStringLiteral("distance"), pose.distance },
        { QStringLiteral("ortho"), c->orthographic }, { QStringLiteral("fov"), c->fov } };
}

void Document::placeSelectedObjectAtView(const QVariantMap& camera)
{
    const auto [eye, target] = eyeAndTarget(camera);
    if (m_model.cameras().contains(m_selectedCamera)) {
        setObjectProperties({ { QStringLiteral("position"), vec3List(eye) }, { QStringLiteral("target"), vec3List(target) },
            { QStringLiteral("orthographic"), camera.value(QStringLiteral("ortho")) } });
    } else if (const model::Light* l = m_model.lights().find(m_selectedLight)) {
        // Lumina vine în ținta vederii (la înălțimea ei), îndreptată ca privirea.
        QVariantMap changes { { QStringLiteral("direction"), vec3List(glm::normalize(target - eye)) } };
        if (l->kind != model::Light::Kind::Directional) {
            changes.insert(QStringLiteral("position"), vec3List(eye));
        }
        setObjectProperties(changes);
    }
}

void Document::structureChanged(const model::Model& model)
{
    // Definițiile noi primesc triangularea; cele dispărute o pierd.
    for (model::DefinitionId d : model.definitions().keys()) {
        if (!m_triangles.contains(d)) {
            retriangulate(d);
        }
    }
    std::erase_if(m_triangles, [&](const auto& entry) { return !model.definitions().contains(entry.first); });
    // Înainte ca interfața să citească selecția (selectionSummary, Entity Info): după un undo, instanțele selectate
    // sau cele din calea contextului pot să nu mai existe.
    dropStaleReferences();
    updateHiddenTags();
    emit outlineChanged();
    emit selectionChanged(); // Entity Info (de exemplu un tag schimbat prin undo)
    emit modelChanged();
    touch();
}

} // namespace aion::render
