#pragma once

#include "aion/core/model/Model.h"

#include <map>
#include <utility>
#include <vector>

// Copierea obiectelor (design: docs/plans/copii-si-animatii-interactive.md). O copie e unică, în adâncime (⌘C / ⌘V,
// Move / Rotate cu Option); doar ⌥D și Array fac copii legate (aceeași definiție). Toate funcțiile care schimbă
// modelul se apelează într-o comandă (un pas de undo).
namespace aion::model {

// Ce a produs o copiere: copiile de sus (în ordinea surselor) și perechile sursă → copie pentru toate instanțele
// copiate (subarborele întreg, cu rădăcinile).
struct CopyResult {
    std::vector<InstanceId> roots;
    std::map<InstanceId, InstanceId> instances;
};

enum class CopyKind { Copy, Unique };

// Copii unice, în adâncime: fiecare definiție din subarbore primește o copie; două instanțe care împart o definiție o
// împart și în copie. Regulile vin odată cu instanțele (țintele din subarbore trec pe copii). `source` poate fi chiar
// `target`. `delta` e în coordonatele contextului.
CopyResult copyDeep(Model& target, DefinitionId context, const Model& source, const std::vector<InstanceId>& sources,
    const math::Transform& delta);

// Copie legată (⌥D, Array): aceeași definiție, pentru orice fel de obiect; o definiție `group` care nu e colecție
// devine `group = false` (un obiect cu copii).
InstanceId copyLinked(Model& model, InstanceId instance, const math::Transform& delta);

// Fă unic, în adâncime: orice definiție din subarbore folosită și în afara lui primește o copie (definițiile proprii
// subarborelui și instanțele lor rămân). `roots` = { instance }; `instances` = instanțele vechi din definițiile copiate
// → cele noi (plus instance → instance).
CopyResult makeUniqueDeep(Model& model, InstanceId instance);
[[nodiscard]] bool canMakeUnique(const Model& model, InstanceId instance);

// Animațiile copiilor (secțiunea 2 din design). `source` = modelul din care s-a copiat (poate fi `target`); `delta` =
// deplasarea copiei. Copy: o animație pornită de o regulă din subarbore → „<nume> (copie)” cu pistele subarborelui (pe
// copii; pista obiectului de sus își mută pozițiile cu `delta`), iar regulile copiilor trec pe ea; celelalte animații cu
// piste în subarbore primesc pistele copiilor (aceeași animație). Căile și cheile de cameră nu se copiază.
// Unique (după makeUniqueDeep): la fel, iar o regulă din afara obiectului care pornea animația separată trece pe copie
// dacă își conține doar copiile, rămâne dacă își conține doar originalele, altfel primește un bloc în plus pentru copie.
void copyAnimations(
    Model& target, const Model& source, const CopyResult& copy, const math::Transform& delta, CopyKind kind);

// Muchiile obiectelor (cu tot ce conțin), în coordonatele părintelui lor: previzualizarea lipirii.
[[nodiscard]] std::vector<std::pair<math::Vec3d, math::Vec3d>> instanceOutline(
    const Model& model, const std::vector<InstanceId>& instances);

} // namespace aion::model
