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

// „850 mm”, „12.5 mm”, „4 m”, „3.25 m”.
[[nodiscard]] std::string formatLength(double mm);

} // namespace aion::units
