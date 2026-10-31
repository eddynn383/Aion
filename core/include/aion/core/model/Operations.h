#pragma once

#include "aion/core/geom/Selection.h"
#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <glm/mat3x3.hpp>

#include <cstdint>
#include <set>

#include <string>
#include <unordered_map>
#include <utility>
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
// Explode ar pierde date de mobilier (date de piesă, cheia unei componente salvate sau parametrii unei plăci); interfața
// cere confirmare înainte (ADR 0044).
[[nodiscard]] bool explodeLosesPartData(const Model& model, InstanceId instance);

// Copierea (unică, legată) și „Fă unic”: model/Copying.h.

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

// Plasarea unei instanțe, ca în Inspector: poziția pivotului (în coordonatele părintelui; aceeași ca în timeline),
// rotația (grade), scara pe axele proprii, dimensiunile rezultate (mm) și pivotul.
struct Placement {
    math::Vec3d position { 0.0 };
    // Unghiurile Euler în grade: întâi în jurul lui X, apoi Y, apoi Z (axele lumii), ca în Blender (XYZ).
    math::Vec3d rotation { 0.0 };
    math::Vec3d scale { 1.0 };
    math::Vec3d size { 0.0 };
    // Pivotul: distanța de la colțul minim, pe axele obiectului, în mm (cu scara aplicată). Gol = neschimbat.
    std::optional<math::Vec3d> pivot {};
};

[[nodiscard]] Placement placementOf(const Model& model, InstanceId instance);

// M = T · R · S · T(−lo): rotația (coloane ortonormate, o rotație adevărată) și scara (lungimile coloanelor) dintr-o
// transformare fără forfecare. O oglindire apare ca scară negativă pe X.
struct Decomposed {
    glm::dmat3 rotation { 1.0 };
    math::Vec3d scale { 1.0 };
};
[[nodiscard]] Decomposed decompose(const math::Transform& t);
// Unghiurile Euler (grade) ale rotației R = Rz · Ry · Rx (la ±90° pe Y, gimbal lock, X devine 0) și invers.
[[nodiscard]] math::Vec3d eulerDegrees(const glm::dmat3& rotation);
[[nodiscard]] glm::dmat3 rotationFromEuler(const math::Vec3d& degrees);

// Pivotul instanței în coordonatele definiției (cel ales sau centrul cutiei) și în coordonatele părintelui.
[[nodiscard]] math::Vec3d pivotLocal(const Model& model, InstanceId instance);
[[nodiscard]] math::Vec3d pivotPoint(const Model& model, InstanceId instance);
// Pivotul nou al instanței (coordonatele definiției). Obiectul nu se mișcă, nici în animații: cheile de poziție ale
// animațiilor (poziția pivotului) se recalculează.
void setPivot(Model& model, InstanceId instance, const math::Vec3d& local);

// O cameră sau o lumină (obiectele scenei), ca țintă a uneltelor Move / Rotate / Scale.
struct SceneObjectRef {
    CameraId camera;
    LightId light;
    bool operator==(const SceneObjectRef&) const = default;
};
// Move / Rotate: poziția și ținta (camera) sau direcția (lumina) urmează transformarea; înclinarea camerei se păstrează
// față de ea, deci o rotație în jurul direcției de privire schimbă roll-ul.
void transformSceneObject(Model& model, const SceneObjectRef& object, const math::Transform& transform);
// Scale (factorii uneltei pe X, Y, Z): camera în perspectivă — mărimea gizmo-ului; ortografică — scara cadrului; panoul
// luminos — laturile; spotul — conul; celelalte lumini — raza. Poziția nu se schimbă.
void scaleSceneObject(Model& model, const SceneObjectRef& object, const math::Vec3d& factors);
// Punctele de prindere ale obiectului (Move): poziția și, la cameră, ținta. Gol dacă obiectul nu există.
[[nodiscard]] std::vector<math::Vec3d> sceneObjectPoints(const Model& model, const SceneObjectRef& object);

// Biții din Instance::placementLocks.
enum PlacementLock : std::uint16_t {
    LockPositionX = 1 << 0, LockPositionY = 1 << 1, LockPositionZ = 1 << 2,
    LockSizeX = 1 << 3, LockSizeY = 1 << 4, LockSizeZ = 1 << 5,
    LockRotationX = 1 << 6, LockRotationY = 1 << 7, LockRotationZ = 1 << 8,
    LockScaleX = 1 << 9, LockScaleY = 1 << 10, LockScaleZ = 1 << 11,
};

// După o transformare, readuce valorile blocate ale instanței la cele din `before`.
void restoreLockedPlacement(Model& model, InstanceId instance, const Placement& before);

// Aplică poziția, rotația și scara cerute. `size`, dacă e nenul pe o axă, are prioritate față de `scale` pe acea axă
// (scara se calculează din dimensiunea dorită). O rotație egală cu cea curentă păstrează exact matricea existentă.
// O poziție nouă mută obiectul ca pivotul să ajungă acolo; rotația, scara și dimensiunile țin pivotul pe loc (ca în
// Blender / Figma). Un pivot nou (`wanted.pivot`) nu mută obiectul.
void setPlacement(Model& model, InstanceId instance, const Placement& wanted);

// Organizarea ca în Blender: un grup (colecție) din obiectele date, în `context`. Transformările lumii nu se schimbă.
InstanceId groupInstances(Model& model, DefinitionId context, const std::set<InstanceId>& instances, std::string name);

// Mută instanța în alt grup sau obiect (`target`), păstrându-i poziția în lume. `target` trebuie să aibă o singură
// apariție și să nu fie chiar instanța sau ceva din interiorul ei. Întoarce false dacă mutarea nu e posibilă.
bool moveToParent(Model& model, InstanceId instance, DefinitionId target);

// Transformarea lumii a (primei apariții a) unei definiții; identitate pentru rădăcină.
[[nodiscard]] math::Transform definitionWorldTransform(const Model& model, DefinitionId definition);

// Scenele ca stări: capturează pozițiile obiectelor, obiectele ascunse și tag-urile ascunse, sau le aplică
// (obiectele care între timp au dispărut se sar; cele noi rămân cum sunt).
void captureSceneState(const Model& model, Scene& scene);
void applySceneState(Model& model, const Scene& scene);

// Cutia (min, max) a geometriei unei definiții, cu tot ce conține, în coordonatele ei.
[[nodiscard]] std::pair<math::Vec3d, math::Vec3d> bounds(const Model& model, DefinitionId definition);
// La fel, cu cutiile definițiilor deja măsurate (o definiție folosită de multe instanțe se măsoară o dată). Cache-ul
// e valabil cât timp modelul nu se schimbă.
using BoundsCache = std::unordered_map<std::uint64_t, std::pair<math::Vec3d, math::Vec3d>>;
[[nodiscard]] std::pair<math::Vec3d, math::Vec3d> bounds(const Model& model, DefinitionId definition, BoundsCache& cache);

} // namespace aion::model
