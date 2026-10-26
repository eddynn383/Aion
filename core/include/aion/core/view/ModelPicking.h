#pragma once

#include "aion/core/model/Model.h"
#include "aion/core/view/Camera.h"

#include <optional>
#include <set>

// Picking pe instanțele (grupuri, componente) plasate într-un context de editare.
namespace aion::view {

struct InstanceHit {
    model::InstanceId instance;
    double t = 0.0; // distanța de-a lungul razei contextului
    math::Vec3d point { 0.0 }; // punctul atins, în coordonatele contextului
};

// Cea mai apropiată instanță vizibilă (cu tot ce conține) sub cursor. `camera` e în coordonatele contextului.
[[nodiscard]] std::optional<InstanceHit> pickInstance(
    const model::Model& model, model::DefinitionId context, const Camera& camera, Vec2d screen);

// Instanțele din dreptunghiul de selecție, după cutiile lor: complet în interior (window) sau atinse (crossing).
[[nodiscard]] std::set<model::InstanceId> pickInstancesInRectangle(const model::Model& model,
    model::DefinitionId context, const Camera& camera, Vec2d corner0, Vec2d corner1, bool crossing);

// Instanța interactivă de sub cursor (unealta Interact): cea mai adâncă din ierarhie care are un comportament.
// `camera` e camera globală.
[[nodiscard]] std::optional<model::InstanceId> interactiveInstanceAt(
    const model::Model& model, const Camera& camera, Vec2d screen);

} // namespace aion::view
