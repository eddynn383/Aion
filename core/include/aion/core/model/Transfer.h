#pragma once

#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <string>

// Transferul de obiecte între modele (biblioteca de obiecte, ADR 0026): un model întreg intră ca obiect în altul,
// iar un obiect se poate scoate într-un model separat. Geometria se copiază cu primitivele Euler (deci cu undo),
// materialele și texturile se potrivesc după nume / sursă, obiectele dinamice își păstrează generatorul.
namespace aion::model {

// Conținutul rădăcinii lui `source` devine o componentă nouă în `context`, cu transformarea dată.
InstanceId insertModel(Model& target, DefinitionId context, const Model& source, const std::string& name,
    const math::Transform& placement);

// Un model nou care conține obiectul (definiția lui, cu tot ce are înăuntru) ca rădăcină; transformarea instanței
// nu se păstrează (obiectul stă în originea modelului nou, la colțul lui minim).
[[nodiscard]] Model extractInstance(const Model& source, InstanceId instance);

} // namespace aion::model
