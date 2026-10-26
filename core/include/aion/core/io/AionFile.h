#pragma once

#include "aion/core/model/Model.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Formatul propriu `.aion` (ADR 0009): o arhivă ZIP cu `model.json` (tot modelul, cu ID-urile lui, ca o încărcare
// să dea exact același model) și, opțional, `thumbnail.png`.
namespace aion::io {

// Crește la fiecare schimbare incompatibilă; fișierele vechi se migrează la încărcare.
inline constexpr int kFormatVersion = 1;

struct LoadResult {
    std::optional<model::Model> model;
    std::string error; // în română, pentru utilizator, dacă `model` lipsește
};

[[nodiscard]] std::string toJson(const model::Model& model);
[[nodiscard]] LoadResult fromJson(std::string_view json);

[[nodiscard]] std::vector<std::uint8_t> writeArchive(
    const model::Model& model, std::span<const std::uint8_t> thumbnailPng = {});
[[nodiscard]] LoadResult readArchive(std::span<const std::uint8_t> bytes);
// Imaginea de previzualizare din arhivă (gol dacă nu are).
[[nodiscard]] std::vector<std::uint8_t> readThumbnail(std::span<const std::uint8_t> bytes);

// Scrie întâi într-un fișier temporar alăturat, apoi îl redenumește: un fișier existent nu rămâne pe jumătate scris.
bool saveFile(const std::filesystem::path& path, const model::Model& model,
    std::span<const std::uint8_t> thumbnailPng = {}, std::string* error = nullptr);
[[nodiscard]] LoadResult loadFile(const std::filesystem::path& path);

} // namespace aion::io
