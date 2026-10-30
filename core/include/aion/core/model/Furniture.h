#pragma once

#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

// Mobilier pe corp (ADR 0043, 0044; designul: docs/plans/mobilier-pe-corp.md): tipurile de plăci, canturi și
// feronerie, feroneria construită în cod, completarea categoriilor materialelor din fișierele vechi.
namespace aion::model {

enum class Unit { SquareMeters, LinearMeters, Piece, Pair };

// Un tip de placă: grosimile permise, formatul plăcii brute (0 = fără, de exemplu lemnul masiv), fibra și
// dacă merge la firma de debitare. Valori provizorii, de verificat la testare
// (docs/plans/mobilier-de-verificat.md).
struct BoardType {
    std::string key;
    std::string label;
    std::vector<double> thicknesses;
    double defaultThickness = 0.0;
    double sheetLength = 0.0;
    double sheetWidth = 0.0;
    bool grain = true;
    bool toCutter = true;
    Unit unit = Unit::SquareMeters;
};

// Un tip de cant: grosimea, tipurile de placă pe care se pune și dacă grosimea lui se scade din dimensiunea
// de tăiere.
struct BandType {
    std::string key;
    std::string label;
    double thickness = 0.0;
    std::vector<std::string> boardTypes;
    bool deduct = true;
};

// Un tip de feronerie, cu forma simplă (o cutie) folosită în model: lungime (X) × lățime (Y) × înălțime (Z),
// cu baza centrată în origine.
struct HardwareType {
    std::string key;
    std::string label;
    Unit unit = Unit::Piece;
    math::Vec3d size { 0.0 };
};

[[nodiscard]] std::span<const BoardType> boardTypes();
[[nodiscard]] std::span<const BandType> bandTypes();
[[nodiscard]] std::span<const HardwareType> hardwareTypes();
[[nodiscard]] const BoardType* findBoardType(std::string_view key);
[[nodiscard]] const BandType* findBandType(std::string_view key);
[[nodiscard]] const HardwareType* findHardwareType(std::string_view key);
// Cantul se poate pune pe tipul de placă dat.
[[nodiscard]] bool bandFitsBoard(std::string_view bandKey, std::string_view boardTypeKey);

// Grosimea permisă cea mai apropiată de `value` pentru tipul de placă; la egalitate, cea mai mare. `value`
// dacă tipul nu există.
[[nodiscard]] double nearestThickness(std::string_view boardTypeKey, double value);

// O bucată de feronerie nouă în `context` (o definiție proprie, cu PartSpec de feronerie și forma tipului).
// Întoarce instanța (nulă dacă tipul nu există).
InstanceId createHardware(
    Model& model, DefinitionId context, std::string_view typeKey, const math::Transform& placement);

// Altă variantă de feronerie pe definiție (deci pe toate instanțele ei): forma, `typeKey`, numele. False dacă
// definiția nu e feronerie sau tipul nu există.
bool replaceHardwareVariant(Model& model, DefinitionId definition, std::string_view typeKey);
// Familia unui tip de feronerie: "hinge", "slide", "handle" (prefixul cheii).
[[nodiscard]] std::string_view hardwareFamily(std::string_view typeKey);

// Dimensiunile unei piese (mm), măsurate pe geometria proprie a definiției (fără sub-piese), cu transformarea
// dată (scările se înmulțesc). O placă (PartSpec de placă): grosimea pe axa grosimii, lungimea pe axa fibrei
// (chiar dacă e latura mai scurtă). Fără PartSpec: regula din ADR 0017 (fața cea mai mare: grosimea pe
// normala ei, lungimea = latura mai lungă).
struct PartMeasure {
    double length = 0.0;
    double width = 0.0;
    double thickness = 0.0;
    bool box = false; // o cutie simplă: 6 fețe, 8 vârfuri, toate în colțurile cutiei
    bool extraShells = false; // vârfuri în interiorul cutiei (geometrie desenată în piesă)
    // Plăcile: axele locale ale lungimii și lățimii (pentru numele laturilor de cant); -1 pentru piesele fără
    // date.
    int lengthAxis = -1;
    int widthAxis = -1;
};

// Indicii din `PartSpec::bands` pentru laturile L1, L2, W1, W2 ale unei plăci măsurate
// (docs/plans/mobilier-pe-corp.md, §5): U, V = axele locale diferite de axa grosimii, crescător; bands = −V,
// +V, −U, +U.
[[nodiscard]] std::array<int, 4> bandSlots(const PartSpec& part, const PartMeasure& measure);
[[nodiscard]] PartMeasure measurePart(
    const Model& model, DefinitionId definition, const math::Transform& toWorld);

// --- Cantul (pasul 7) ---
// Laturile unei plăci, cum le vede utilizatorul: 0 = L1, 1 = L2 (laturile paralele cu fibra), 2 = W1, 3 = W2.
[[nodiscard]] std::string_view bandSideName(int side);
// Cantul pe latura `side` (L1…W2) a plăcii `definition`: `bandKey` gol = fără; `decor` = cheia unui material
// inclus, gol = „ca placa”. False dacă definiția nu e o placă sau cantul nu se potrivește tipului plăcii.
bool setBand(
    Model& model, DefinitionId definition, int side, std::string_view bandKey, std::string_view decor = {});
// Cantul plăcii, în ordinea L1, L2, W1, W2.
[[nodiscard]] std::array<BandSide, 4> boardBands(const Model& model, DefinitionId definition);

struct BandPreset {
    std::string name;
    std::array<BandSide, 4> sides {}; // L1, L2, W1, W2
};
// „Fără”, „Toate 2 mm”, „L1 2 mm, restul 0,4 mm”.
[[nodiscard]] std::span<const BandPreset> builtinBandPresets();
// O presetare pe placă: laturile cu un cant care nu se potrivește tipului rămân fără cant. Întoarce câte
// laturi au rămas fără cant din cauza tipului.
int applyBandPreset(Model& model, DefinitionId definition, const std::array<BandSide, 4>& sides);

// Geometria e o cutie simplă aliniată la axele definiției (6 fețe, 8 vârfuri în colțurile cutiei).
[[nodiscard]] bool isSimpleBox(const geom::Mesh& mesh);

// „Fă placă”: pune PartSpec de placă pe definiția obiectului (axa grosimii = dimensiunea cea mai mică, fibra
// = cea mai lungă dintre celelalte două, grosimea nominală = cea permisă cea mai apropiată). Refuză (false)
// un obiect fără fețe proprii sau cu sub-piese.
bool makeBoard(Model& model, InstanceId instance, std::string_view boardTypeKey = "pal");

// --- Placa parametrică (pasul 5, ADR 0044) ---
// O placă din bibliotecă: `generator = "board"`, `parameters = {length, width}`, PartSpec cu tipul și
// grosimea; geometria (o cutie) e în mesh-ul definiției, cu originea în colțul minim, X = lungime, Y =
// lățime, Z = grosime.

inline constexpr const char* kBoardGenerator = "board";

// O placă nouă în `context`. Întoarce instanța (nulă dacă tipul nu există). Grosimea se aduce la cea permisă.
InstanceId createBoard(Model& model, DefinitionId context, std::string_view typeKey, double thickness,
    double length, double width, const math::Transform& placement);

// Reface cutia din parametri și grosimea nominală (decorul e pe instanță, deci rămâne).
void rebuildBoard(Model& model, DefinitionId definition);

enum class BoardState { Matches, Modified, Detached, NotBoard };
enum class BoardReason { None, Geometry, Thickness, Scaled, ScaledParent, Children };
struct BoardStatus {
    BoardState state = BoardState::NotBoard;
    BoardReason reason = BoardReason::None;
};
// Starea plăcii unei instanțe, cu transformarea ei până la lume (pentru scara părinților). Se calculează, nu
// se salvează.
[[nodiscard]] BoardStatus boardState(const Model& model, InstanceId instance, const math::Transform& toWorld);

// Schimbă un parametru al plăcii ("length", "width", "thickness") sau tipul; reface placa și ține pivotul
// fiecărei instanțe pe loc în lume (DS-1). False dacă definiția nu e o placă parametrică.
bool setBoardValue(Model& model, DefinitionId definition, std::string_view key, double value);
bool setBoardType(Model& model, DefinitionId definition, std::string_view typeKey);

// Mută originea definiției cu `d` (în coordonatele ei): vârfurile cu −d, iar instanțele, pivoții explicați,
// scenele, ancorele căilor, cotele și curbele Pen se compensează, ca nimic să nu se miște în lume.
void shiftBoardOrigin(Model& model, DefinitionId definition, const math::Vec3d& d);

// După un Push/Pull pe un capăt: dacă placa e încă o cutie cu grosimea nominală, dimensiunile intră în
// parametri (D20). Nimic dacă e deja în acord. Întoarce true dacă a schimbat ceva.
bool adoptBoardGeometry(Model& model, DefinitionId definition);
// Finalizatorul din CommandStack: adoptBoardGeometry pe plăcile atinse.
void finalizeBoards(Model& model, std::span<const DefinitionId> changed);

// „Readu la parametri”: reface cutia și aduce |scara| instanței la 1 (semnul rămâne), cu pivotul pe loc.
void restoreBoard(Model& model, InstanceId instance);
// „Desprinde de parametri”: placa rămâne o cutie cu PartSpec, măsurată din geometrie.
void detachBoard(Model& model, DefinitionId definition);

// Fișierele dinainte de formatul v6: materialele fără categorie care au numele unui material inclus primesc
// categoria și cheia lui (ADR 0044).
void backfillMaterials(Model& model);

} // namespace aion::model
