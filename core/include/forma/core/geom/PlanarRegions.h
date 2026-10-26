#pragma once

#include "forma/core/geom/Topology.h"
#include "forma/core/math/Plane.h"

#include <span>
#include <vector>

namespace forma::geom {

class Mesh;

// O regiune mărginită a aranjamentului plan al unor muchii: conturul exterior (sens trigonometric
// față de normala planului) și găurile (sens orar) formate de componentele aflate în interior.
struct PlanarRegion {
    Loop outer;
    std::vector<Loop> holes;
};

// Regiunile mărginite formate de muchiile date, toate aflate în `plane` și fără intersecții între ele.
// Punțile (muchiile care nu fac parte din niciun ciclu) se ignoră: rămân muchii libere pe față.
[[nodiscard]] std::vector<PlanarRegion> findPlanarRegions(
    const Mesh& mesh, const math::Plane& plane, std::span<const EdgeId> edges);

// Inversează sensul unei bucle (ordinea și direcția fiecărei muchii).
[[nodiscard]] Loop reversed(const Loop& loop);

} // namespace forma::geom
