#pragma once

#include "aion/core/model/Model.h"

// Interacțiunile (unealta Interact): ușile se rotesc pe balama, sertarele glisează. Totul se calculează din
// comportamentul instanței și din cutia definiției ei (coordonate locale), deci rămâne corect după Move/Rotate.
namespace aion::model {

// Mișcarea locală pentru deschiderea `fraction` (0 = închis, 1 = complet deschis), în coordonatele definiției.
[[nodiscard]] math::Transform behaviorMotion(const Model& model, InstanceId instance, double fraction);

// Transformarea instanței (local → părinte) la deschiderea dată, pornind de la starea salvată (`behavior.open`).
[[nodiscard]] math::Transform transformAtOpenness(const Model& model, InstanceId instance, double fraction);

// Comută starea (deschis ↔ închis): transformarea și `behavior.open`. Pentru o comandă cu undo.
void toggleBehavior(Model& model, InstanceId instance);

} // namespace aion::model
