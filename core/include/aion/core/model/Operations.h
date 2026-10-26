#pragma once

#include "aion/core/geom/Selection.h"
#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <string>
#include <vector>

// Operațiile pe structura modelului: grupuri, componente, explode (vezi ADR 0008).
namespace aion::model {

// Transformarea locală → globală a unui context de editare: produsul transformărilor instanțelor din cale
// (de la rădăcină spre interior).
[[nodiscard]] math::Transform worldTransform(const Model& model, const std::vector<InstanceId>& path);

// Make Group / Make Component: geometria selectată din `context` (și instanțele selectate de acolo) trece
// într-o definiție nouă, plasată pe loc printr-o instanță (transformare identitate). Muchiile folosite și de
// fețe rămase în context rămân și acolo. Întoarce noua instanță (nul dacă selecția e goală).
InstanceId makeGroup(Model& model, DefinitionId context, const geom::Selection& selection, bool component,
    std::string name = {});

// Explode: geometria definiției (transformată) se lipește de geometria părintelui, instanțele din ea devin
// copii ai părintelui; definiția dispare dacă nu mai are instanțe. Întoarce entitățile apărute în părinte.
geom::Selection explode(Model& model, InstanceId instance);

// Make Unique: instanța primește o copie proprie a definiției (celelalte copii ale componentei nu se mai schimbă
// împreună cu ea). Întoarce definiția nouă.
DefinitionId makeUnique(Model& model, InstanceId instance);

// O copie a instanței (aceeași definiție: o componentă rămâne legată de celelalte copii; un grup primește
// o definiție proprie, ca în SketchUp), mutată cu `delta` (în coordonatele părintelui).
InstanceId copyInstance(Model& model, InstanceId instance, const math::Transform& delta);

// Șterge o instanță. Definiția unui grup rămas fără instanțe dispare și ea (cu tot ce conține);
// definițiile componentelor rămân în model (biblioteca „În model”).
void eraseInstance(Model& model, InstanceId instance);

// Mută, rotește sau scalează instanțele (în coordonatele părintelui).
void transformInstances(Model& model, const std::set<InstanceId>& instances, const math::Transform& delta);

// Un solid: fiecare muchie a geometriei (cu tot ce conține) e folosită de exact două fețe — un volum închis.
[[nodiscard]] bool isSolid(const Model& model, DefinitionId definition);

enum class BooleanKind { Union, Subtract, Intersect };

struct BooleanResult {
    InstanceId instance; // grupul nou (nul dacă operația a eșuat)
    std::string error;
};

// Operații booleene pe două grupuri sau obiecte solide din `context` (Manifold): rezultatul e un grup nou,
// iar cele două dispar. Subtract scade `b` din `a`.
BooleanResult booleanOperation(Model& model, InstanceId a, InstanceId b, BooleanKind kind);

// Cutia (min, max) a geometriei unei definiții, cu tot ce conține, în coordonatele ei.
[[nodiscard]] std::pair<math::Vec3d, math::Vec3d> bounds(const Model& model, DefinitionId definition);

} // namespace aion::model
