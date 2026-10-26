#pragma once

#include "aion/core/model/Model.h"

#include <string>
#include <vector>

// Rapoarte de cantități: unul general (arii, volume, materiale) și lista de debitare pentru mobilier
// (piese cu lungime × lățime × grosime, grupate, plus o estimare a numărului de plăci). Doar ce e vizibil.
// Toate lungimile sunt în mm, ariile în mm², volumele în mm³ (conversia pentru afișare o face interfața sau CSV-ul).
namespace aion::model {

// --- Raportul general ---

struct ObjectQuantity {
    std::string name; // calea în ierarhie: „Dulap / Ușă stânga”
    bool component = false; // obiect (cu copii legate) sau grup
    int count = 0; // aparițiile (copiile unui obiect cu aceeași cale)
    double area = 0.0; // aria fețelor proprii, pentru toate aparițiile
    double volume = 0.0; // pentru toate aparițiile; 0 dacă nu e solid
    bool solid = false;
    math::Vec3d size { 0.0 }; // dimensiunile cutiei unei apariții, în coordonatele modelului
    std::string material; // materialul predominant
    std::string tag;
};

struct NamedArea {
    std::string name;
    double area = 0.0;
};

struct QuantityReport {
    std::vector<ObjectQuantity> objects; // primul rând: geometria liberă din model, dacă există
    std::vector<NamedArea> materials; // aria pe materiale (fețele fără material: „Implicit”)
    std::vector<NamedArea> tags; // aria pe tag-uri („Fără tag”)
    double totalArea = 0.0;
    double totalVolume = 0.0; // suma volumelor obiectelor solide
    double horizontalArea = 0.0; // fețele orizontale orientate în sus (pardoseli, blaturi)
    std::size_t faceCount = 0;
    double edgeLength = 0.0; // lungimea totală a muchiilor
};

[[nodiscard]] QuantityReport quantityReport(const Model& model);

// --- Lista de debitare (mobilier) ---

struct CutListOptions {
    double sheetLength = 2800.0; // placa standard (PAL, MDF)
    double sheetWidth = 2070.0;
    double waste = 0.15; // pierderi la tăiere și fibră (15%)
    double maxThickness = 100.0; // piesele mai groase nu sunt plăci (de exemplu un picior masiv): apar, dar nu în necesarul de plăci
};

struct CutPart {
    std::string name;
    std::string material;
    double length = 0.0; // cea mai mare dimensiune a feței principale
    double width = 0.0;
    double thickness = 0.0; // pe normala feței principale
    int count = 0;
    bool panel = true; // grosimea ≤ maxThickness (și nenulă)
    bool flat = false; // fără grosime (o față grupată, nu un volum): apare, dar nu intră în necesarul de plăci
};

struct SheetNeed {
    std::string material;
    double thickness = 0.0;
    double partsArea = 0.0; // mm², fără pierderi
    int sheets = 0; // cu pierderi, rotunjit în sus
};

struct CutList {
    std::vector<CutPart> parts;
    std::vector<SheetNeed> sheets;
    double edgeBanding = 0.0; // cant maxim (perimetrul tuturor pieselor), mm
};

// Fiecare grup sau obiect cu fețe proprii e o piesă. Dimensiunile se măsoară pe fața cea mai mare (grosimea pe
// normala ei, lungimea pe direcția celei mai lungi muchii), deci și piesele rotite au dimensiunile corecte.
[[nodiscard]] CutList cutList(const Model& model, const CutListOptions& options = {});

// --- CSV ---

// Implicit, ca pentru Excel în română: separator „;” și virgulă zecimală. Fișierul începe cu BOM UTF-8.
struct CsvFormat {
    char separator = ';';
    char decimal = ',';
};

[[nodiscard]] std::string toCsv(const QuantityReport& report, CsvFormat format = {});
[[nodiscard]] std::string toCsv(const CutList& list, const CutListOptions& options, CsvFormat format = {});

} // namespace aion::model
