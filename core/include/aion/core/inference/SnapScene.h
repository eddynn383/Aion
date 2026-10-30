#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/math/Transform.h"
#include "aion/core/math/Vec.h"
#include "aion/core/model/Model.h"

#include <limits>
#include <vector>

// Geometria pe care o vede inferența: nu doar mesh-ul contextului editat, ci și obiectele din model.
namespace aion::inference {

struct SnapSource {
    const geom::Mesh* mesh = nullptr;
    math::Transform toContext; // local (definiția sursei) → coordonatele contextului editat
    // Instanța cea mai de sus din contextul editat care conține sursa; goală pentru geometria liberă a unui context părinte.
    model::InstanceId instance;
    // Cutia mesh-ului în coordonatele contextului (calculată o dată, la construirea scenei): inferența sare sursele
    // departe de cursor. `lo > hi` = cutie necunoscută; sursa nu se sare niciodată.
    math::Vec3d lo { std::numeric_limits<double>::infinity() };
    math::Vec3d hi { -std::numeric_limits<double>::infinity() };
};

struct SnapScene {
    std::vector<SnapSource> sources;
};

} // namespace aion::inference
