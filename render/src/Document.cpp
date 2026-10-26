#include "aion/render/Document.h"

#include "aion/core/model/Operations.h"
#include "aion/core/units/Length.h"

#include <QColor>

#include <glm/geometric.hpp>

#include <cmath>
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
    m_stack.setOnChanged([this] { emit historyChanged(); });
    // În debug, un model invalid oprește aplicația (CommandStack); în release nu se verifică.
    retriangulate(m_model.root());
    setActiveTool(QStringLiteral("select"));
}

Document::~Document()
{
    m_model.removeObserver(this);
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
    if (!m_selection.instances.empty()) {
        parts << plural(m_selection.instances.size(), "obiect", "obiecte");
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
        info[QStringLiteral("title")] = def.group ? QStringLiteral("Grup") : QStringLiteral("Componentă");
        info[QStringLiteral("instanceId")] = static_cast<double>(id.value);
        info[QStringLiteral("name")] = QString::fromStdString(def.group ? inst.name : def.name);
        info[QStringLiteral("tagId")] = static_cast<double>(inst.tag.value);
        info[QStringLiteral("locked")] = inst.locked;
        if (!def.group) {
            details << detail(QStringLiteral("Copii în model"), QString::number(m_model.instancesOf(inst.definition).size()));
        }
        const auto [lo, hi] = model::bounds(m_model, inst.definition);
        if (lo.x <= hi.x) {
            const math::Vec3d size = hi - lo;
            details << detail(QStringLiteral("Dimensiuni"),
                QString::fromStdString(units::formatLength(size.x) + " × " + units::formatLength(size.y) + " × "
                    + units::formatLength(size.z)));
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
    fn(*m_tool, ctx);
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
    emit outlineChanged();
}

void Document::makeGroup()
{
    if (m_selection.empty()) {
        return;
    }
    runOnModel("Make Group", [&](model::Model& model) {
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
    runOnModel("Make Component", [&](model::Model& model) {
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
    runOnModel("Explode", [&](model::Model& model) {
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
    runOnModel("Make Unique", [&](model::Model& model) {
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
    runOnModel("Hide", [&](model::Model& model) {
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
    runOnModel("Unhide All", [&](model::Model& model) {
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
    runOnModel(locked ? "Lock" : "Unlock", [&](model::Model& model) {
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
    runOnModel("Rename", [&](model::Model& model) {
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
    runOnModel(hidden ? "Hide" : "Unhide", [&](model::Model& model) {
        model::Instance inst = model.instance(id);
        inst.hidden = hidden;
        model.setInstance(id, inst);
    });
}

void Document::addTag(const QString& name)
{
    runOnModel("Add Tag", [&](model::Model& model) {
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
    runOnModel("Rename Tag", [&](model::Model& model) {
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
    runOnModel(visible ? "Show Tag" : "Hide Tag", [&](model::Model& model) {
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
    runOnModel("Delete Tag", [&](model::Model& model) { model.removeTag(id); });
}

void Document::assignTag(double tagId)
{
    const model::TagId id = toTagId(tagId);
    if (m_selection.instances.empty() || (id && !m_model.tags().contains(id))) {
        return;
    }
    runOnModel("Tag", [&](model::Model& model) {
        for (model::InstanceId i : m_selection.instances) {
            model::Instance inst = model.instance(i);
            inst.tag = id;
            model.setInstance(i, inst);
        }
    });
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
    emit modelChanged();
    touch();
}

} // namespace aion::render
