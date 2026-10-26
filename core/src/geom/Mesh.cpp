#include "aion/core/geom/Mesh.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cassert>

namespace aion::geom {

namespace {

template <class T>
void eraseValue(std::vector<T>& v, const T& value)
{
    v.erase(std::remove(v.begin(), v.end(), value), v.end());
}

template <class IdT, class T>
void touchIn(std::map<IdT, EntityChange<T>>& changes, const SlotMap<IdT, T>& map, IdT id)
{
    if (changes.contains(id)) {
        return;
    }
    EntityChange<T>& change = changes[id];
    if (const T* current = map.find(id)) {
        change.before = *current;
    }
}

template <class IdT, class T>
void finishChanges(std::map<IdT, EntityChange<T>>& changes, const SlotMap<IdT, T>& map)
{
    for (auto it = changes.begin(); it != changes.end();) {
        if (const T* current = map.find(it->first)) {
            it->second.after = *current;
        }
        // Atinsă, dar neschimbată (sau creată și ștearsă în aceeași operație): nu contează.
        if (it->second.before == it->second.after) {
            it = changes.erase(it);
        } else {
            ++it;
        }
    }
}

template <class IdT, class T>
void applyChanges(const std::map<IdT, EntityChange<T>>& changes, SlotMap<IdT, T>& map, ChangeDirection direction)
{
    for (const auto& [id, change] : changes) {
        const std::optional<T>& target = direction == ChangeDirection::Forward ? change.after : change.before;
        if (target) {
            if (T* existing = map.find(id)) {
                *existing = *target;
            } else {
                map.insertWithId(id, *target);
            }
        } else {
            map.erase(id);
        }
    }
}

template <class IdT, class T>
void summarizeInto(IdChanges<IdT>& out, const std::map<IdT, EntityChange<T>>& changes, ChangeDirection direction)
{
    for (const auto& [id, change] : changes) {
        const bool before = direction == ChangeDirection::Forward ? change.before.has_value() : change.after.has_value();
        const bool after = direction == ChangeDirection::Forward ? change.after.has_value() : change.before.has_value();
        if (!before && after) {
            out.added.push_back(id);
        } else if (before && !after) {
            out.removed.push_back(id);
        } else {
            out.modified.push_back(id);
        }
    }
}

template <class Fn>
void forEachLoop(const Face& face, Fn&& fn)
{
    fn(face.outer);
    for (const Loop& loop : face.inner) {
        fn(loop);
    }
}

} // namespace

ChangeSummary summarize(const ChangeSet& changes, ChangeDirection direction)
{
    ChangeSummary s;
    summarizeInto(s.vertices, changes.vertices, direction);
    summarizeInto(s.edges, changes.edges, direction);
    summarizeInto(s.faces, changes.faces, direction);
    return s;
}

std::string ValidationReport::text() const
{
    std::string out;
    for (const std::string& e : errors) {
        out += e;
        out += '\n';
    }
    return out;
}

// --- Citire ---

EdgeId Mesh::findEdge(VertexId a, VertexId b) const
{
    const Vertex* va = m_vertices.find(a);
    if (va == nullptr) {
        return {};
    }
    for (EdgeId e : va->edges) {
        if (m_edges.at(e).other(a) == b) {
            return e;
        }
    }
    return {};
}

VertexId Mesh::findVertexNear(const Vec3d& p, double tolerance) const
{
    VertexId best;
    double bestDistance = tolerance;
    for (std::size_t i = 0; i < m_vertices.size(); ++i) {
        const double d = glm::length(m_vertices.values()[i].position - p);
        if (d <= bestDistance) {
            bestDistance = d;
            best = m_vertices.keys()[i];
        }
    }
    return best;
}

VertexId Mesh::useStart(const EdgeUse& use) const
{
    const Edge& e = m_edges.at(use.edge);
    return use.reversed ? e.v1 : e.v0;
}

VertexId Mesh::useEnd(const EdgeUse& use) const
{
    const Edge& e = m_edges.at(use.edge);
    return use.reversed ? e.v0 : e.v1;
}

std::vector<VertexId> Mesh::loopVertices(const Loop& loop) const
{
    std::vector<VertexId> out;
    out.reserve(loop.uses.size());
    for (const EdgeUse& use : loop.uses) {
        out.push_back(useStart(use));
    }
    return out;
}

std::vector<Vec3d> Mesh::loopPoints(const Loop& loop) const
{
    std::vector<Vec3d> out;
    out.reserve(loop.uses.size());
    for (const EdgeUse& use : loop.uses) {
        out.push_back(position(useStart(use)));
    }
    return out;
}

// --- Primitive ---

void Mesh::touch(VertexId id)
{
    if (m_recording) {
        touchIn(m_recording->vertices, m_vertices, id);
    }
}

void Mesh::touch(EdgeId id)
{
    if (m_recording) {
        touchIn(m_recording->edges, m_edges, id);
    }
}

void Mesh::touch(FaceId id)
{
    if (m_recording) {
        touchIn(m_recording->faces, m_faces, id);
    }
}

VertexId Mesh::addVertex(const Vec3d& position)
{
    const VertexId id = m_vertices.insert(Vertex { position, {} });
    if (m_recording) {
        m_recording->vertices[id]; // înainte: nu exista
    }
    return id;
}

void Mesh::removeVertex(VertexId id)
{
    assert(m_vertices.at(id).edges.empty());
    touch(id);
    m_vertices.erase(id);
}

EdgeId Mesh::addEdge(VertexId a, VertexId b)
{
    assert(a != b && !findEdge(a, b));
    const EdgeId id = m_edges.insert(Edge { a, b, {} });
    if (m_recording) {
        m_recording->edges[id];
    }
    touch(a);
    touch(b);
    m_vertices.at(a).edges.push_back(id);
    m_vertices.at(b).edges.push_back(id);
    return id;
}

void Mesh::removeEdge(EdgeId id)
{
    const Edge& e = m_edges.at(id);
    assert(e.faces.empty());
    touch(id);
    touch(e.v0);
    touch(e.v1);
    eraseValue(m_vertices.at(e.v0).edges, id);
    eraseValue(m_vertices.at(e.v1).edges, id);
    m_edges.erase(id);
}

void Mesh::attachFace(FaceId id, const Face& face)
{
    forEachLoop(face, [&](const Loop& loop) {
        for (const EdgeUse& use : loop.uses) {
            touch(use.edge);
            std::vector<FaceId>& list = m_edges.at(use.edge).faces;
            if (std::find(list.begin(), list.end(), id) == list.end()) {
                list.push_back(id);
            }
        }
    });
}

void Mesh::detachFace(FaceId id, const Face& face)
{
    forEachLoop(face, [&](const Loop& loop) {
        for (const EdgeUse& use : loop.uses) {
            touch(use.edge);
            eraseValue(m_edges.at(use.edge).faces, id);
        }
    });
}

FaceId Mesh::addFace(const math::Plane& plane, Loop outer, std::vector<Loop> inner, model::MaterialId material)
{
    const FaceId id = m_faces.insert(Face { plane, std::move(outer), std::move(inner), material });
    if (m_recording) {
        m_recording->faces[id];
    }
    attachFace(id, m_faces.at(id));
    return id;
}

void Mesh::removeFace(FaceId id)
{
    touch(id);
    detachFace(id, m_faces.at(id));
    m_faces.erase(id);
}

void Mesh::setPosition(VertexId id, const Vec3d& position)
{
    touch(id);
    m_vertices.at(id).position = position;
}

void Mesh::setEdgeStyle(EdgeId id, bool soft, std::uint32_t curve, bool smoothCurve)
{
    Edge& e = m_edges.at(id);
    if (e.soft == soft && e.curve == curve && e.smoothCurve == smoothCurve) {
        return;
    }
    touch(id);
    e.soft = soft;
    e.curve = curve;
    e.smoothCurve = smoothCurve;
}

std::uint32_t Mesh::newCurveId() const
{
    std::uint32_t highest = 0;
    for (const Edge& e : m_edges.values()) {
        highest = std::max(highest, e.curve);
    }
    return highest + 1;
}

void Mesh::setFaceMaterial(FaceId id, model::MaterialId material)
{
    if (m_faces.at(id).material == material) {
        return;
    }
    touch(id);
    m_faces.at(id).material = material;
}

void Mesh::setFacePlane(FaceId id, const math::Plane& plane)
{
    touch(id);
    m_faces.at(id).plane = plane;
}

std::pair<EdgeId, EdgeId> Mesh::splitEdge(EdgeId id, VertexId at)
{
    const Edge original = m_edges.at(id);
    assert(!original.hasVertex(at));

    // Fețele se desprind de muchia veche, primesc buclele noi, apoi se reatașează.
    for (FaceId f : original.faces) {
        touch(f);
        detachFace(f, m_faces.at(f));
    }

    touch(id);
    touch(original.v0);
    touch(original.v1);
    eraseValue(m_vertices.at(original.v0).edges, id);
    eraseValue(m_vertices.at(original.v1).edges, id);
    m_edges.erase(id);

    const EdgeId first = addEdge(original.v0, at);
    const EdgeId second = addEdge(at, original.v1);
    // Bucățile păstrează stilul muchiei (soft, curba).
    for (EdgeId piece : { first, second }) {
        Edge& e = m_edges.at(piece);
        e.soft = original.soft;
        e.curve = original.curve;
        e.smoothCurve = original.smoothCurve;
    }

    for (FaceId f : original.faces) {
        Face& face = m_faces.at(f);
        const auto replace = [&](Loop& loop) {
            std::vector<EdgeUse> uses;
            uses.reserve(loop.uses.size() + 1);
            for (const EdgeUse& use : loop.uses) {
                if (use.edge != id) {
                    uses.push_back(use);
                } else if (!use.reversed) {
                    uses.push_back({ first, false });
                    uses.push_back({ second, false });
                } else {
                    uses.push_back({ second, true });
                    uses.push_back({ first, true });
                }
            }
            loop.uses = std::move(uses);
        };
        replace(face.outer);
        for (Loop& loop : face.inner) {
            replace(loop);
        }
        attachFace(f, face);
    }
    return { first, second };
}

// --- Înregistrare ---

void Mesh::beginRecording()
{
    assert(!m_recording);
    m_recording.emplace();
}

ChangeSet Mesh::endRecording()
{
    assert(m_recording);
    ChangeSet changes = std::move(*m_recording);
    m_recording.reset();
    finishChanges(changes.vertices, m_vertices);
    finishChanges(changes.edges, m_edges);
    finishChanges(changes.faces, m_faces);
    if (!changes.empty()) {
        notify(summarize(changes, ChangeDirection::Forward));
    }
    return changes;
}

void Mesh::apply(const ChangeSet& changes, ChangeDirection direction)
{
    assert(!m_recording);
    applyChanges(changes.vertices, m_vertices, direction);
    applyChanges(changes.edges, m_edges, direction);
    applyChanges(changes.faces, m_faces, direction);
    if (!changes.empty()) {
        notify(summarize(changes, direction));
    }
}

void Mesh::addObserver(MeshObserver* observer)
{
    m_observers.push_back(observer);
}

void Mesh::removeObserver(MeshObserver* observer)
{
    eraseValue(m_observers, observer);
}

void Mesh::notify(const ChangeSummary& summary)
{
    for (MeshObserver* o : m_observers) {
        o->meshChanged(*this, summary);
    }
}

bool Mesh::sameAs(const Mesh& other) const
{
    const auto same = [](const auto& a, const auto& b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            const auto* theirs = b.find(a.keys()[i]);
            if (theirs == nullptr) {
                return false;
            }
            // Listele de adiacență pot avea altă ordine după undo; le comparăm ca mulțimi.
            auto mine = a.values()[i];
            auto copy = *theirs;
            if constexpr (requires { mine.edges; }) {
                std::sort(mine.edges.begin(), mine.edges.end());
                std::sort(copy.edges.begin(), copy.edges.end());
            }
            if constexpr (requires { mine.faces; }) {
                std::sort(mine.faces.begin(), mine.faces.end());
                std::sort(copy.faces.begin(), copy.faces.end());
            }
            if (!(mine == copy)) {
                return false;
            }
        }
        return true;
    };
    return same(m_vertices, other.m_vertices) && same(m_edges, other.m_edges) && same(m_faces, other.m_faces);
}

} // namespace aion::geom
