#include "aion/render/Document.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/inference/Inference.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/io/Exchange.h"
#include "aion/core/model/MaterialLibrary.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Quantities.h"
#include "aion/core/units/Length.h"
#include "aion/core/view/ModelPicking.h"
#include "aion/core/view/Picking.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <span>
#include <limits>
#include <set>

namespace aion::render {

namespace {

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

// „1.2 × 0.6 × 2.4 m” sau „155.4 × 155.4 × 186.3 mm”: o singură unitate, după cea mai mare dimensiune.
QString formatSize(const math::Vec3d& size)
{
    const bool meters = std::max({ size.x, size.y, size.z }) >= 1000.0;
    const auto number = [&](double mm) {
        QString s = QString::number(meters ? mm / 1000.0 : mm, 'f', meters ? 3 : 1);
        while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.')))) {
            s.chop(1);
        }
        return s;
    };
    return number(size.x) + QStringLiteral(" × ") + number(size.y) + QStringLiteral(" × ") + number(size.z)
        + (meters ? QStringLiteral(" m") : QStringLiteral(" mm"));
}

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
    m_hasRecovery = QFile::exists(autosavePath());
    m_autosaveTimer.setInterval(60'000);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &Document::autosave);
    m_autosaveTimer.start();
}

Document::~Document()
{
    // O închidere normală fără modificări nesalvate nu lasă nimic de recuperat.
    if (!modified() && !m_hasRecovery) {
        removeAutosave();
    }
    m_model.removeObserver(this);
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
    const io::LoadResult result = io::loadFile(std::filesystem::path(path.toStdString()));
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
    if (!io::saveFile(std::filesystem::path(target.toStdString()), m_model,
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
            return io::importGltf(model, activeDefinition(), std::filesystem::path(path.toStdString()));
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
            const std::string mtlText = io::exportMtl();
            ok = obj.write(objText.data(), qint64(objText.size())) == qint64(objText.size())
                && mtl.write(mtlText.data(), qint64(mtlText.size())) == qint64(mtlText.size());
        }
    } else if (ext == QLatin1String("gltf") || ext == QLatin1String("glb")) {
        ok = io::exportGltf(m_model, std::filesystem::path(path.toStdString()), ext == QLatin1String("glb"), {}, &error);
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
    if (io::saveFile(std::filesystem::path(autosavePath().toStdString()), m_model)) {
        QSettings settings;
        settings.setValue(kRecoverySourceKey, m_filePath);
        settings.setValue(kRecoveryTimeKey, QDateTime::currentDateTime());
    }
}

void Document::removeAutosave()
{
    QFile::remove(autosavePath());
    QSettings settings;
    settings.remove(kRecoverySourceKey);
    settings.remove(kRecoveryTimeKey);
    if (m_hasRecovery) {
        m_hasRecovery = false;
        emit recoveryChanged();
    }
}

void Document::recoverAutosave()
{
    const QString source = QSettings().value(kRecoverySourceKey).toString();
    const io::LoadResult result = io::loadFile(std::filesystem::path(autosavePath().toStdString()));
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

model::DefinitionId Document::activeDefinition() const
{
    return m_path.empty() ? m_model.root() : m_model.instance(m_path.back()).definition;
}

math::Transform Document::contextTransform() const
{
    return model::worldTransform(m_model, m_path);
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
    return m_tool ? QString::fromStdString(m_tool->status()) : QString();
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
        groups += m_model.definition(m_model.instance(id).definition).group ? 1 : 0;
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
                { QStringLiteral("kind"), def.group ? QStringLiteral("group") : QStringLiteral("component") },
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
    if (m_selection.instances.size() == 1 && m_selection.faces.empty() && m_selection.edges.empty()) {
        const model::InstanceId id = *m_selection.instances.begin();
        const model::Instance& inst = m_model.instance(id);
        const model::Definition& def = m_model.definition(inst.definition);
        info[QStringLiteral("kind")] = QStringLiteral("instance");
        info[QStringLiteral("title")] = def.group ? QStringLiteral("Grup") : QStringLiteral("Obiect");
        info[QStringLiteral("instanceId")] = static_cast<double>(id.value);
        info[QStringLiteral("name")] = QString::fromStdString(def.group ? inst.name : def.name);
        info[QStringLiteral("tagId")] = static_cast<double>(inst.tag.value);
        info[QStringLiteral("locked")] = inst.locked;
        info[QStringLiteral("copies")] = static_cast<int>(m_model.instancesOf(inst.definition).size());
        if (!def.group) {
            details << detail(QStringLiteral("Copii"), QString::number(m_model.instancesOf(inst.definition).size()));
        }
        const auto [lo, hi] = model::bounds(m_model, inst.definition);
        if (lo.x <= hi.x) {
            details << detail(QStringLiteral("Dimensiuni"), formatSize(hi - lo));
        }
        if (inst.material && m_model.materials().contains(inst.material)) {
            details << detail(QStringLiteral("Material"), QString::fromStdString(m_model.material(inst.material).name));
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
        if (m_selection.faces.size() == 1) {
            const geom::Face& face = mesh.face(*m_selection.faces.begin());
            details << detail(QStringLiteral("Material"),
                face.material && m_model.materials().contains(face.material)
                    ? QString::fromStdString(m_model.material(face.material).name)
                    : QStringLiteral("Implicit"));
        }
        info[QStringLiteral("kind")] = QStringLiteral("faces");
        info[QStringLiteral("title")]
            = m_selection.faces.size() == 1 ? QStringLiteral("Față") : plural(m_selection.faces.size(), "față", "fețe");
        details << detail(QStringLiteral("Arie"), formatArea(area));
    } else if (m_selection.instances.empty() && m_selection.faces.empty()) {
        double length = 0.0;
        for (geom::EdgeId e : m_selection.edges) {
            if (const geom::Edge* edge = mesh.edges().find(e)) {
                length += glm::length(mesh.position(edge->v1) - mesh.position(edge->v0));
            }
        }
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
    if (m_selection != before) {
        emit selectionChanged();
        emit outlineChanged();
    }
    emit toolStatusChanged();
    touch();
}

void Document::repairContext()
{
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
    const geom::Selection before = m_selection;
    m_selection.clear();
    afterInteraction(before);
}

template <class Fn>
void Document::runOnModel(const std::string& name, Fn&& fn)
{
    const geom::Selection before = m_selection;
    m_stack.run(name, [&](model::Model& model) { fn(model); });
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
    runOnModel("Creează grup", [&](model::Model& model) {
        const model::InstanceId id = model::makeGroup(model, activeDefinition(), m_selection, false);
        m_selection.clear();
        m_selection.instances.insert(id);
    });
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
    if (m_selection.instances.empty() || (id && !m_model.tags().contains(id))) {
        return;
    }
    runOnModel("Schimbă tag-ul", [&](model::Model& model) {
        for (model::InstanceId i : m_selection.instances) {
            model::Instance inst = model.instance(i);
            inst.tag = id;
            model.setInstance(i, inst);
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
        item(QStringLiteral("org.makeGroup"), QStringLiteral("Creează grup"), QStringLiteral("Ctrl+G"));
    } else {
        item(QStringLiteral("org.makeGroup"), QStringLiteral("Creează grup"), QStringLiteral("Ctrl+G"));
        item(QStringLiteral("org.makeComponent"), QStringLiteral("Creează obiect"), QStringLiteral("Ctrl+Shift+G"));
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

QVariantList Document::libraryMaterials() const
{
    QVariantList out;
    const auto library = model::builtinMaterials();
    for (std::size_t i = 0; i < library.size(); ++i) {
        QVariantMap entry = materialEntry(library[i]);
        entry[QStringLiteral("index")] = static_cast<int>(i);
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

void Document::structureChanged(const model::Model& model)
{
    // Definițiile noi primesc triangularea; cele dispărute o pierd.
    for (model::DefinitionId d : model.definitions().keys()) {
        if (!m_triangles.contains(d)) {
            retriangulate(d);
        }
    }
    std::erase_if(m_triangles, [&](const auto& entry) { return !model.definitions().contains(entry.first); });
    emit outlineChanged();
    emit selectionChanged(); // Entity Info (de exemplu un tag schimbat prin undo)
    emit modelChanged();
    touch();
}

} // namespace aion::render
