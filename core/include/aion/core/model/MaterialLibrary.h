#pragma once

#include "aion/core/model/Model.h"

#include <span>

// Materialele incluse în aplicație (biblioteca din panoul Materiale). Un material din bibliotecă intră în model
// abia când e folosit.
namespace aion::model {

[[nodiscard]] std::span<const Material> builtinMaterials();

} // namespace aion::model
