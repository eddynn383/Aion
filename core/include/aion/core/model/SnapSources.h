#pragma once

#include "aion/core/inference/SnapScene.h"
#include "aion/core/model/Model.h"

#include <set>
#include <vector>

namespace aion::model {

// Sursele de inferență pentru contextul `path` (calea instanțelor editate, ca la worldTransform; gol = rădăcina):
// toate instanțele vizibile (Model::isVisible, recursiv) de la rădăcină, plus geometria liberă a contextelor
// părinte. Fără: mesh-ul contextului însuși (e ctx.mesh), instanțele din `path` (cea editată e ctx.mesh),
// instanțele din `exclude` cu tot ce conțin, definițiile colecție fără geometrie, mesh-urile goale.
[[nodiscard]] inference::SnapScene snapScene(
    const Model& model, const std::vector<InstanceId>& path, const std::set<InstanceId>& exclude);

} // namespace aion::model
