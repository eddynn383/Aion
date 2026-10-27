#pragma once

#include "aion/core/geom/Id.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/math/Vec.h"
#include "aion/core/geom/Topology.h"
#include "aion/core/model/Ids.h"
#include "aion/core/view/Camera.h"

#include <set>

namespace aion::geom {
class Mesh;
}

// Ce se află sub cursor sau într-un dreptunghi de selecție.
namespace aion::view {

struct PickResult {
    enum class Kind { None, Vertex, Edge, Face, Instance };

    Kind kind = Kind::None;
    geom::VertexId vertex;
    geom::EdgeId edge;
    geom::FaceId face; // fața de sub cursor, dacă există (și când s-a ales o muchie sau un vârf pe ea)
    model::InstanceId instance; // pentru Kind::Instance: grupul sau componenta de sub cursor
    math::Vec3d point { 0.0 }; // punctul 3D atins

    [[nodiscard]] bool hit() const { return kind != Kind::None; }
};

struct PickOptions {
    double vertexRadiusPx = 7.0;
    double edgeRadiusPx = 5.0;
};

// Tag-urile ascunse (le setează aplicația când se schimbă vizibilitatea): fețele și muchiile cu un tag ascuns nu se
// aleg, nu se ating prin snap și nu se desenează. Un vârf e ascuns dacă toate muchiile lui sunt ascunse.
void setHiddenTags(std::set<model::TagId> tags);
[[nodiscard]] const std::set<model::TagId>& hiddenTags();
[[nodiscard]] bool isHidden(const geom::Face& face);
[[nodiscard]] bool isHidden(const geom::Edge& edge);
[[nodiscard]] bool isHidden(const geom::Mesh& mesh, geom::VertexId vertex);

// Raza atinge cutia aliniată cu axele (cu o margine)? Testul rapid de dinaintea triangulării unei fețe.
[[nodiscard]] bool rayHitsBox(const math::Ray& ray, const math::Vec3d& lo, const math::Vec3d& hi, double margin = 0.0);

// Prioritate: vârf, apoi muchie, apoi față. Muchiile și vârfurile ascunse în spatele unei fețe nu se aleg.
[[nodiscard]] PickResult pick(const geom::Mesh& mesh, const Camera& camera, Vec2d screen, PickOptions options = {});

// Selecția prin dreptunghi (colțuri pe ecran). `crossing` = dreapta → stânga: tot ce atinge dreptunghiul;
// altfel (stânga → dreapta) doar ce e complet în interior.
[[nodiscard]] geom::Selection pickInRectangle(
    const geom::Mesh& mesh, const Camera& camera, Vec2d corner0, Vec2d corner1, bool crossing);

} // namespace aion::view
