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

// Un tip de placă: grosimile permise, formatul plăcii brute (0 = fără, de exemplu lemnul masiv), fibra și dacă merge la
// firma de debitare. Valori provizorii, de verificat la testare (docs/plans/mobilier-de-verificat.md).
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

// Un tip de cant: grosimea, tipurile de placă pe care se pune și dacă grosimea lui se scade din dimensiunea de tăiere.
struct BandType {
    std::string key;
    std::string label;
    double thickness = 0.0;
    std::vector<std::string> boardTypes;
    bool deduct = true;
};

// Un tip de feronerie, cu forma simplă (o cutie) folosită în model: lungime (X) × lățime (Y) × înălțime (Z), cu baza
// centrată în origine.
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

// Grosimea permisă cea mai apropiată de `value` pentru tipul de placă; la egalitate, cea mai mare. `value` dacă tipul
// nu există.
[[nodiscard]] double nearestThickness(std::string_view boardTypeKey, double value);

// O bucată de feronerie nouă în `context` (o definiție proprie, cu PartSpec de feronerie și forma tipului). Întoarce
// instanța (nulă dacă tipul nu există).
InstanceId createHardware(Model& model, DefinitionId context, std::string_view typeKey, const math::Transform& placement);

// Dimensiunile unei piese (mm), măsurate pe geometria proprie a definiției (fără sub-piese), cu transformarea dată
// (scările se înmulțesc). O placă (PartSpec de placă): grosimea pe axa grosimii, lungimea pe axa fibrei (chiar dacă e
// latura mai scurtă). Fără PartSpec: regula din ADR 0017 (fața cea mai mare: grosimea pe normala ei, lungimea =
// latura mai lungă).
struct PartMeasure {
    double length = 0.0;
    double width = 0.0;
    double thickness = 0.0;
    bool box = false; // o cutie simplă: 6 fețe, 8 vârfuri, toate în colțurile cutiei
    bool extraShells = false; // vârfuri în interiorul cutiei (geometrie desenată în piesă)
    // Plăcile: axele locale ale lungimii și lățimii (pentru numele laturilor de cant); -1 pentru piesele fără date.
    int lengthAxis = -1;
    int widthAxis = -1;
};

// Indicii din `PartSpec::bands` pentru laturile L1, L2, W1, W2 ale unei plăci măsurate (docs/plans/mobilier-pe-corp.md,
// §5): U, V = axele locale diferite de axa grosimii, crescător; bands = −V, +V, −U, +U.
[[nodiscard]] std::array<int, 4> bandSlots(const PartSpec& part, const PartMeasure& measure);
[[nodiscard]] PartMeasure measurePart(const Model& model, DefinitionId definition, const math::Transform& toWorld);

// Geometria e o cutie simplă aliniată la axele definiției (6 fețe, 8 vârfuri în colțurile cutiei).
[[nodiscard]] bool isSimpleBox(const geom::Mesh& mesh);

// „Fă placă”: pune PartSpec de placă pe definiția obiectului (axa grosimii = dimensiunea cea mai mică, fibra = cea mai
// lungă dintre celelalte două, grosimea nominală = cea permisă cea mai apropiată). Refuză (false) un obiect fără fețe
// proprii sau cu sub-piese.
bool makeBoard(Model& model, InstanceId instance, std::string_view boardTypeKey = "pal");

// Fișierele dinainte de formatul v6: materialele fără categorie care au numele unui material inclus primesc categoria
// și cheia lui (ADR 0044).
void backfillMaterials(Model& model);

} // namespace aion::model
