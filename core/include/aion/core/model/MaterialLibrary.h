#pragma once

#include "aion/core/model/Model.h"

#include <optional>
#include <span>
#include <string_view>

// Materialele incluse în aplicație (biblioteca din panoul Materiale). Un material din bibliotecă intră în model
// abia când e folosit.
namespace aion::model {

[[nodiscard]] std::span<const Material> builtinMaterials();

// Textura procedurală a materialului din bibliotecă („brick”, „parquet”, „wood”, „concrete”, „tiles”), gol dacă
// materialul are doar culoare. Imaginea o generează aplicația (core nu desenează), la mărimea `textureSize`.
[[nodiscard]] std::string_view builtinTexture(std::size_t index);

// Indicele materialului inclus cu cheia dată (ADR 0044).
[[nodiscard]] std::optional<std::size_t> libraryMaterialByKey(std::string_view key);

} // namespace aion::model
