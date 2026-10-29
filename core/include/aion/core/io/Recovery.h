#pragma once

#include "aion/core/model/Model.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Copiile de recuperare (ADR 0040). Fiecare sesiune a aplicației își autosalvează documentul în dosarul de recuperare,
// în fișierele ei: `<sesiune>.aion` (modelul) și `<sesiune>.json` (de unde vine documentul și când s-a salvat).
// O sesiune încheiată normal își șterge fișierele; ce rămâne de la o sesiune care nu mai rulează (o închidere bruscă)
// se oferă la pornire. Cine rulează încă hotărăște apelantul (aplicația ține un lock pe sesiune: `<sesiune>.lock`).
namespace aion::io {

struct RecoveryInfo {
    std::string source; // fișierul documentului (UTF-8); gol = un document nesalvat niciodată
    std::int64_t savedAt = 0; // secunde Unix ale autosalvării
};

struct RecoveryEntry {
    std::string session;
    std::filesystem::path model; // fișierul `.aion` al copiei
    RecoveryInfo info;
};

[[nodiscard]] std::filesystem::path recoveryModelPath(const std::filesystem::path& dir, std::string_view session);
[[nodiscard]] std::filesystem::path recoveryInfoPath(const std::filesystem::path& dir, std::string_view session);
[[nodiscard]] std::filesystem::path recoveryLockPath(const std::filesystem::path& dir, std::string_view session);

[[nodiscard]] std::string recoveryInfoToJson(const RecoveryInfo& info);
// nullopt dacă textul nu e JSON-ul unei copii.
[[nodiscard]] std::optional<RecoveryInfo> recoveryInfoFromJson(std::string_view json);

// Scrie copia sesiunii: întâi modelul (prin `saveFile`, deci fără fișiere pe jumătate scrise), apoi informațiile.
bool writeRecovery(const std::filesystem::path& dir, std::string_view session, const model::Model& model,
    const RecoveryInfo& info, std::span<const std::uint8_t> thumbnailPng = {}, std::string* error = nullptr);

// Copiile din dosar, cele mai noi întâi. Contează doar sesiunile care au modelul; fără `.json` (o scriere întreruptă
// între cele două fișiere), sursa rămâne necunoscută și timpul e al fișierului. `include(sesiune)` = false sare
// sesiunea (de exemplu cea curentă sau una care încă rulează); e chemat cel mult o dată pe sesiune.
[[nodiscard]] std::vector<RecoveryEntry> listRecoveries(
    const std::filesystem::path& dir, const std::function<bool(const std::string& session)>& include = {});

// Șterge modelul și informațiile sesiunii (lock-ul rămâne în grija celui care îl ține).
void removeRecovery(const std::filesystem::path& dir, std::string_view session);

// Ce copii se pot șterge ca să rămână cel mult `maxCount`, niciuna mai veche de `maxAgeSeconds` față de `now`
// (`entries` sunt cele mai noi întâi, ca din `listRecoveries`).
[[nodiscard]] std::vector<std::string> recoveriesToPrune(
    std::span<const RecoveryEntry> entries, std::size_t maxCount, std::int64_t maxAgeSeconds, std::int64_t now);

} // namespace aion::io
