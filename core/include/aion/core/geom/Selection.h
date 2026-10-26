#pragma once

#include "aion/core/geom/Id.h"
#include "aion/core/model/Ids.h"

#include <set>

namespace aion::geom {

class Mesh;

// Selecția curentă: muchii și fețe (ca în SketchUp, vârfurile nu se selectează direct).
struct Selection {
    std::set<EdgeId> edges;
    std::set<FaceId> faces;
    // Grupurile și componentele selectate (instanțe plasate în contextul de editare).
    std::set<model::InstanceId> instances;

    [[nodiscard]] bool empty() const { return edges.empty() && faces.empty() && instances.empty(); }
    [[nodiscard]] bool contains(EdgeId e) const { return edges.contains(e); }
    [[nodiscard]] bool contains(FaceId f) const { return faces.contains(f); }
    [[nodiscard]] bool contains(model::InstanceId i) const { return instances.contains(i); }
    void clear()
    {
        edges.clear();
        faces.clear();
        instances.clear();
    }

    void add(const Selection& other);
    void remove(const Selection& other);
    // Adaugă ce nu era selectat și scoate ce era (Shift în SketchUp).
    void toggle(const Selection& other);
    // Scoate entitățile care nu mai există (după undo, ștergere, împărțire).
    void prune(const Mesh& mesh);

    bool operator==(const Selection&) const = default;
};

// O față împreună cu muchiile ei (dublu-click pe față).
[[nodiscard]] Selection faceWithEdges(const Mesh& mesh, FaceId face);
// O muchie împreună cu fețele care o folosesc (dublu-click pe muchie).
[[nodiscard]] Selection edgeWithFaces(const Mesh& mesh, EdgeId edge);
// Toată geometria conectată prin muchii (triplu-click).
[[nodiscard]] Selection connectedTo(const Mesh& mesh, const Selection& seed);

} // namespace aion::geom
