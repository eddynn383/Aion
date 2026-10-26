#include "aion/render/Document.h"

#include <set>

namespace aion::render {

namespace {

QString plural(std::size_t n, const char* one, const char* many)
{
    return n == 1 ? QStringLiteral("1 %1").arg(QString::fromUtf8(one))
                  : QStringLiteral("%1 %2").arg(n).arg(QString::fromUtf8(many));
}

} // namespace

Document::Document(QObject* parent)
    : QObject(parent)
{
    m_mesh.addObserver(this);
    m_stack.setOnChanged([this] { emit historyChanged(); });
    // În debug, un model invalid oprește aplicația (CommandStack); în release nu se verifică.
    setActiveTool(QStringLiteral("select"));
}

Document::~Document()
{
    m_mesh.removeObserver(this);
}

void Document::touch()
{
    ++m_revision;
    emit changed();
}

void Document::setActiveTool(const QString& id)
{
    if (id == m_toolId) {
        return;
    }
    if (m_tool) {
        // Unealta veche nu are nevoie de cameră ca să renunțe; îi dăm una oarecare.
        const view::Camera camera;
        tools::ToolContext ctx { m_mesh, m_stack, m_selection, camera };
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
    if (!m_selection.faces.empty()) {
        parts << plural(m_selection.faces.size(), "față", "fețe");
    }
    if (!m_selection.edges.empty()) {
        parts << plural(m_selection.edges.size(), "muchie", "muchii");
    }
    const std::size_t total = m_selection.faces.size() + m_selection.edges.size();
    return parts.join(QStringLiteral(", ")) + (total == 1 ? QStringLiteral(" selectată") : QStringLiteral(" selectate"));
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

template <class Fn>
void Document::withTool(const view::Camera& camera, Fn&& fn)
{
    if (!m_tool) {
        return;
    }
    const geom::Selection before = m_selection;
    tools::ToolContext ctx { m_mesh, m_stack, m_selection, camera };
    fn(*m_tool, ctx);
    afterInteraction(before);
}

void Document::afterInteraction(const geom::Selection& before)
{
    m_selection.prune(m_mesh);
    if (m_selection != before) {
        emit selectionChanged();
    }
    emit toolStatusChanged();
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
        tools::ToolContext ctx { m_mesh, m_stack, m_selection, m_lastCamera };
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
    tools::ToolContext ctx { m_mesh, m_stack, m_selection, camera };
    tools::deleteSelection(ctx);
    afterInteraction(before);
}

void Document::selectAll()
{
    const geom::Selection before = m_selection;
    m_selection.clear();
    for (geom::EdgeId e : m_mesh.edges().keys()) {
        m_selection.edges.insert(e);
    }
    for (geom::FaceId f : m_mesh.faces().keys()) {
        m_selection.faces.insert(f);
    }
    afterInteraction(before);
}

void Document::clearSelection()
{
    const geom::Selection before = m_selection;
    m_selection.clear();
    afterInteraction(before);
}

void Document::meshChanged(const geom::Mesh& mesh, const geom::ChangeSummary& summary)
{
    // Fețele adăugate sau modificate se retriangulează; la fel cele ale căror vârfuri s-au mutat.
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
        m_triangles.erase(f);
    }
    for (geom::FaceId f : dirty) {
        if (mesh.faces().contains(f)) {
            m_triangles[f] = geom::triangulateFace(mesh, f);
        }
    }
    emit modelChanged();
    touch();
}

} // namespace aion::render
