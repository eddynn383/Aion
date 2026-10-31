#pragma once

#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Corpurile de mobilier din bibliotecă (pasul 6, ADR 0043; designul: docs/plans/mobilier-pe-corp.md, §4):
// „Corp la sol”, „Corp suspendat”, „Soldat”, „Corp pentru electrocasnice”, „Dulap cu uși”, „Raft”. Corpul e o
// definiție cu generator și parametri; piesele lui sunt plăci parametrice și feronerie cu `roleKey`, după
// care refacerea le recunoaște: o piesă existentă își păstrează decorul, cantul și varianta de feronerie.
namespace aion::model {

// Generatorul e unul dintre corpurile de mai sus.
[[nodiscard]] bool isCarcassGenerator(std::string_view id);

// O piesă cerută de generator, în coordonatele părintelui ei (corpul sau grupul ușii).
struct DesiredPiece {
    enum class Kind { Board, Hardware, Group };
    std::string role; // "side.left", "door.1", "door.1.hinge.2"
    std::string parentRole; // "" = direct în corp; altfel rolul grupului (ușa)
    Kind kind = Kind::Board;
    std::string typeKey; // tipul plăcii sau al feroneriei
    double thickness = 0.0;
    double length = 0.0; // pe fibră (axa X a plăcii)
    double width = 0.0;
    math::Transform transform;
    std::string nameKey;
    std::optional<double> relativePosition {};
    Behavior behavior {}; // grupurile ușilor
};

// Piesele cerute de generatorul corpului `body` cu parametrii lui (și separatoarele puse cu clic, la
// fracțiile lor).
[[nodiscard]] std::vector<DesiredPiece> desiredPieces(const Model& model, DefinitionId body);

// Reface corpul: potrivește piesele cerute cu cele existente după rol (§4, „Reconcilierea”).
void reconcileCarcass(Model& model, DefinitionId body);

// Înainte de o schimbare de câmp: fracția fiecărui separator pus cu clic, din poziția lui curentă (o mutare
// de mână se păstrează).
void refreshDividerPositions(Model& model, DefinitionId body);

// Vreo piesă cu rol diferă de ce ar produce generatorul (formă, dimensiuni sau poziție): o refacere ar pierde
// modificări de mână.
[[nodiscard]] bool rolePiecesDiffer(const Model& model, DefinitionId body);

// „Actualizează” un corp vechi: `cabinet` → `wardrobe`, `shelf` → `rack`, cu aceleași dimensiuni (adâncimea
// fără uși). Piesele vechi se șterg. False dacă generatorul nu e unul vechi.
bool upgradeGenerator(Model& model, DefinitionId definition);

// „Transformă în corp obișnuit”: corpul nu se mai reface (plăcile rămân parametrice).
void makePlainBody(Model& model, DefinitionId definition);

// Rezultatul lui `addDivider`.
enum class DividerResult { Added, NotCarcass, TooClose, Outside };

// „Adaugă separator”: în spațiul corpului care conține `pointInBody` (coordonatele corpului), la mijlocul lui
// sau
// (`atClick`) la poziția punctului. Refuză la mai puțin de 50 mm de alt separator sau de o laterală.
DividerResult addDivider(Model& model, DefinitionId body, const math::Vec3d& pointInBody, bool atClick,
    InstanceId* created = nullptr);

// Corpul din calea de instanțe (de la rădăcină spre interior) cel mai adânc; nul dacă nu există.
[[nodiscard]] InstanceId carcassInPath(const Model& model, std::span<const InstanceId> path);

// O ușă a unui corp din bibliotecă: transformarea ei deschisă (animația ușii la final), pentru afișarea „uși
// deschise” din Modelare; nullopt pentru o instanță care nu e ușă.
[[nodiscard]] std::optional<math::Transform> doorOpenTransform(const Model& model, InstanceId instance);
// Modelul are cel puțin o ușă de corp.
[[nodiscard]] bool hasDoors(const Model& model);

// Numele afișat al unei piese: numele dat de utilizator, apoi cel dat de program (`nameKey`, în română până
// la traducere), apoi numele definiției.
[[nodiscard]] std::string partName(const Model& model, InstanceId instance);

} // namespace aion::model
