#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

// Căutare fuzzy pentru command palette: literele din interogare trebuie să apară în text,
// în ordine, dar nu neapărat una lângă alta. Ignoră majusculele și diacriticele românești
// („comuta” găsește „Comută tema”).
namespace forma::text {

struct FuzzyMatch {
    // Mai mare = potrivire mai bună. Are sens doar pentru comparații între rezultate.
    int score = 0;
    // Indicii caracterelor potrivite din text, în code point-uri (pentru evidențiere în UI).
    std::vector<std::size_t> positions;
};

// Textele sunt UTF-8. Interogarea goală se potrivește cu orice text, cu scor 0.
[[nodiscard]] std::optional<FuzzyMatch> fuzzyMatch(std::string_view query, std::string_view text);

} // namespace forma::text
