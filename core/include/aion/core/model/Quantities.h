#pragma once

#include "aion/core/model/Model.h"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

// Rapoarte de cantități: unul general (arii, volume, materiale) și lista de debitare pentru mobilier
// (piese cu lungime × lățime × grosime, grupate, plus o estimare a numărului de plăci). Raportul general numără doar ce
// e vizibil; lista de debitare arată și piesele ascunse, separat (ADR 0044).
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
    double sheetLength = 2800.0; // placa standard pentru piesele fără tip (PAL, MDF)
    double sheetWidth = 2070.0;
    double waste = 0.15; // pierderi la tăiere și fibră (15%)
    double maxThickness = 100.0; // piesele fără date mai groase nu sunt plăci (de exemplu un picior masiv)
    // Rândurile (după `CutPart::rowKey`) scoase din totaluri și din CSV (bifele din fereastră, ADR 0044).
    std::set<std::string> excluded;
    // CSV-ul trimite dimensiunea de tăiere (fără cantul care „se scade”) în loc de cea finită.
    bool cutDimensions = false;
    bool headerRow = true; // primul rând al CSV-ului spune ce dimensiuni trimite
    double bandAllowance = 50.0; // mm de cant adăugați pe fiecare muchie cu cant
    std::map<std::string, double> bandAllowanceByType; // adaosul pe tip de cant, dacă diferă
};

// Secțiunea listei: plăci, feronerie (bucăți), alte componente (bucăți, după categoria materialului) și piese
// nedefinite (lemn fără tip, tip necunoscut).
enum class CutSection { Board, Hardware, Other, Undefined };

struct CutPart {
    std::string name; // numele primei piese; toate numele sunt în `usedIn`
    std::string material; // decorul (materialul predominant)
    double length = 0.0; // dimensiunea finită (cu cant)
    double width = 0.0;
    double thickness = 0.0; // măsurată
    int count = 0;
    bool panel = true; // intră în necesarul de plăci brute
    bool flat = false; // fără grosime (o față grupată, nu un volum)
    // --- Mobilier (ADR 0044) ---
    CutSection section = CutSection::Board;
    std::string typeKey; // tipul de placă sau de feronerie; "" = fără date
    double nominalThickness = 0.0;
    double cutLength = 0.0; // lungimea / lățimea de tăiere: fără cantul care „se scade”
    double cutWidth = 0.0;
    std::array<std::string, 4> bands {}; // cantul pe L1, L2, W1, W2 (cheia tipului)
    std::array<std::string, 4> bandDecors {}; // decorul cantului pe L1, L2, W1, W2 (numele materialului)
    bool hidden = false; // o piesă ascunsă (ea, tag-ul ei sau un părinte)
    std::string rowKey; // cheia stabilă a rândului (pentru bife)
    std::vector<std::string> usedIn; // numele pieselor din rând
    std::vector<std::string> flags; // motivele: „grosime nepotrivită”, „formă neregulată”, „tip nedefinit”…
};

struct SheetNeed {
    std::string material;
    double thickness = 0.0;
    double partsArea = 0.0; // mm², fără pierderi
    int sheets = 0; // cu pierderi, rotunjit în sus
    std::string typeKey; // tipul de placă ("" = piese fără tip, cu placa din opțiuni)
    double sheetLength = 0.0;
    double sheetWidth = 0.0;
};

// Cantul necesar pe tip și decor: lungimea laturilor cu cant (finite) plus adaosul pe muchie.
struct BandNeed {
    std::string bandKey;
    std::string decor;
    double length = 0.0; // mm
    int edges = 0;
};

struct HardwareNeed {
    std::string typeKey;
    std::string label;
    int count = 0; // bucăți (glisierele: perechi)
};

struct CutList {
    std::vector<CutPart> parts;
    std::vector<SheetNeed> sheets;
    double edgeBanding = 0.0; // cant estimat (perimetrul) pentru plăcile fără date, mm
    std::vector<BandNeed> bands;
    std::vector<HardwareNeed> hardware;
    int hiddenPieces = 0; // piese ascunse neincluse (pentru mesajul din fereastră)
};

// Lista de debitare v1.5 (ADR 0044): plăcile cu date (tip, grosime, cant, fibră), feroneria pe bucăți, celelalte
// obiecte după categoria materialului; piesele ascunse apar pe rânduri separate. Piesele fără date: regula din ADR 0017
// (fața cea mai mare). Totalurile iau doar rândurile care nu sunt în `options.excluded`.
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
