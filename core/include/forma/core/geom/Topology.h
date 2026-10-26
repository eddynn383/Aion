#pragma once

#include "forma/core/geom/Id.h"
#include "forma/core/math/Plane.h"
#include "forma/core/math/Vec.h"

#include <vector>

// Topologia non-manifold (radial-edge simplificat, vezi ADR 0003): muchiile pot avea 0..n fețe.
namespace forma::geom {

using math::Vec3d;

struct Vertex {
    Vec3d position { 0.0 };
    std::vector<EdgeId> edges; // muchiile care pornesc din vârf

    bool operator==(const Vertex&) const = default;
};

struct Edge {
    VertexId v0;
    VertexId v1;
    std::vector<FaceId> faces; // fețele care folosesc muchia (radial)

    [[nodiscard]] VertexId other(VertexId v) const { return v == v0 ? v1 : v0; }
    [[nodiscard]] bool hasVertex(VertexId v) const { return v == v0 || v == v1; }

    bool operator==(const Edge&) const = default;
};

// O muchie folosită într-o buclă; `reversed` = parcursă de la v1 la v0.
struct EdgeUse {
    EdgeId edge;
    bool reversed = false;

    bool operator==(const EdgeUse&) const = default;
};

// Ciclu închis de muchii: capătul fiecărei utilizări e începutul următoarei.
struct Loop {
    std::vector<EdgeUse> uses;

    bool operator==(const Loop&) const = default;
};

// Față plană: bucla exterioară în sens trigonometric față de normală, găurile în sens orar.
struct Face {
    math::Plane plane;
    Loop outer;
    std::vector<Loop> inner;

    bool operator==(const Face&) const = default;
};

} // namespace forma::geom
