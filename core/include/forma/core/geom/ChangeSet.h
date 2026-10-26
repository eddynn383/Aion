#pragma once

#include "forma/core/geom/Id.h"
#include "forma/core/geom/Topology.h"

#include <map>
#include <optional>
#include <vector>

namespace forma::geom {

// Starea unei entități înainte și după o operație (nullopt = nu exista).
template <class T>
struct EntityChange {
    std::optional<T> before;
    std::optional<T> after;
};

// Toate schimbările făcute de o operație. Undo restaurează `before`, redo `after`.
struct ChangeSet {
    std::map<VertexId, EntityChange<Vertex>> vertices;
    std::map<EdgeId, EntityChange<Edge>> edges;
    std::map<FaceId, EntityChange<Face>> faces;

    [[nodiscard]] bool empty() const { return vertices.empty() && edges.empty() && faces.empty(); }
};

enum class ChangeDirection { Forward, Backward };

// Rezumatul pentru observatori (randare, UI): ce entități au apărut, s-au schimbat sau au dispărut.
template <class IdT>
struct IdChanges {
    std::vector<IdT> added;
    std::vector<IdT> modified;
    std::vector<IdT> removed;

    [[nodiscard]] bool empty() const { return added.empty() && modified.empty() && removed.empty(); }
};

struct ChangeSummary {
    IdChanges<VertexId> vertices;
    IdChanges<EdgeId> edges;
    IdChanges<FaceId> faces;

    [[nodiscard]] bool empty() const { return vertices.empty() && edges.empty() && faces.empty(); }
};

[[nodiscard]] ChangeSummary summarize(const ChangeSet& changes, ChangeDirection direction);

class Mesh;

class MeshObserver {
public:
    virtual ~MeshObserver() = default;
    virtual void meshChanged(const Mesh& mesh, const ChangeSummary& summary) = 0;
};

} // namespace forma::geom
