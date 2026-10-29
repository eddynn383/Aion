#pragma once

#include "aion/core/geom/Selection.h"
#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <utility>

// Operații de modelare pe selecție (în definiția `context`): oglindire, array liniar și polar, soften / smooth.
// Selecția poate avea instanțe (obiecte, grupuri) și geometrie liberă (muchii, fețe) din context. Vezi ADR 0023.
namespace aion::model {

// Cutia selecției, în coordonatele contextului (geometria liberă și cutiile instanțelor transformate).
// first > second pe X dacă selecția e goală.
[[nodiscard]] std::pair<math::Vec3d, math::Vec3d> selectionBounds(
    const Model& model, DefinitionId context, const geom::Selection& selection);

// Oglindește selecția față de planul prin `center`, perpendicular pe axa `axis` (0 = X roșu, 1 = Y verde, 2 = Z albastru).
// Fețele rămân cu exteriorul în afară. Întoarce selecția după oglindire (fețele pot primi ID-uri noi).
geom::Selection mirrorSelection(Model& model, DefinitionId context, const geom::Selection& selection, int axis,
    const math::Vec3d& center);

// `copies` copii ale selecției, a k-a transformată cu `step` aplicat de k ori (instanțele: copii legate; geometria
// liberă: copiată în context). Întoarce copiile (fără original).
geom::Selection arraySelection(Model& model, DefinitionId context, const geom::Selection& selection,
    const math::Transform& step, int copies);

// Array liniar: copiile la `offset`, 2 × `offset`, …
geom::Selection linearArray(Model& model, DefinitionId context, const geom::Selection& selection,
    const math::Vec3d& offset, int copies);

// Array polar: copiile rotite în jurul axei (`center`, `axis`) cu `stepRadians`, 2 × `stepRadians`, …
// Fără `rotateCopies`, copiile doar se deplasează pe cerc (orientarea rămâne cea a originalului).
geom::Selection polarArray(Model& model, DefinitionId context, const geom::Selection& selection,
    const math::Vec3d& center, const math::Vec3d& axis, double stepRadians, int copies, bool rotateCopies);

// Soften / Smooth: muchiile dintre două fețe cu unghiul dintre normale cel mult `maxAngleDegrees` devin soft
// (ascunse, cu umbrire netedă); celelalte (cu două fețe) devin vizibile. `includeCoplanar` = false lasă vizibile
// muchiile dintre fețe coplanare. Întoarce câte muchii au devenit soft.
int softenEdges(geom::Mesh& mesh, const std::set<geom::EdgeId>& edges, double maxAngleDegrees, bool includeCoplanar);

// Segmentele unei sfere / unui cilindru / con (numărul de laturi ale cercurilor), păstrate în
// `Definition::parameters["segments"]` (implicit 24). Se pot schimba doar cât geometria e încă cea a primitivei
// (nemodificată: fără Push/Pull, Subdivide…); altfel, și pentru alte definiții, `primitiveSegments` întoarce 0.
inline constexpr int kDefaultPrimitiveSegments = 24;
[[nodiscard]] int primitiveSegments(const Model& model, DefinitionId definition);
// Reface primitiva cu `segments` (3–128), cu aceleași dimensiuni și același material. Întoarce false dacă nu se poate.
bool setPrimitiveSegments(Model& model, DefinitionId definition, int segments);

} // namespace aion::model
