#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Lungimile tastate în Measurements box și afișate în viewport. Unitatea internă e milimetrul.
namespace aion::units {

// O lungime: „2500” (mm), „2.5m”, „250 cm”, „25mm”, „3'”, „3'6"”, „3' 6"”, „6"”, „-200”.
// Separatorul zecimal e punctul (virgula separă valorile, ca la dreptunghi).
[[nodiscard]] std::optional<double> parseLength(std::string_view text);

// Mai multe lungimi separate prin virgulă sau punct și virgulă („3000,2000”).
// Un câmp gol („,2000”) devine nullopt: valoarea respectivă rămâne neschimbată.
// Întoarce nullopt dacă vreun câmp e invalid sau sunt mai mult de `maxCount` câmpuri.
[[nodiscard]] std::optional<std::vector<std::optional<double>>> parseLengths(std::string_view text, std::size_t maxCount);

// Unitatea de afișare (Preferințe): Auto = mm sub 1 m, metri peste.
enum class Unit { Auto, Millimeter, Centimeter, Meter, Inch, Foot };

struct DisplaySettings {
    Unit unit = Unit::Auto;
    int decimals = -1; // -1 = implicit (1 pentru mm/cm, 3 pentru m, 2 pentru inci)
};

// Setările de afișare sunt globale (o singură interfață per proces); implicit Auto.
void setDisplaySettings(const DisplaySettings& settings);
[[nodiscard]] const DisplaySettings& displaySettings();

// „850 mm”, „12.5 mm”, „4 m”, „3.25 m” (după unitatea de afișare). Un număr tastat fără unitate e interpretat
// în unitatea de afișare (mm pentru Auto).
[[nodiscard]] std::string formatLength(double mm);

} // namespace aion::units
