#pragma once

#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Transferul de obiecte între modele (biblioteca de obiecte, ADR 0026): un model întreg intră ca obiect în
// altul, iar un obiect se poate scoate într-un model separat. Geometria se copiază cu primitivele Euler (deci
// cu undo), materialele și texturile se potrivesc după nume / sursă, obiectele dinamice își păstrează
// generatorul.
namespace aion::model {

// Conținutul rădăcinii lui `source` devine o componentă nouă în `context`, cu transformarea dată.
InstanceId insertModel(Model& target, DefinitionId context, const Model& source, const std::string& name,
    const math::Transform& placement);

// Un model nou care conține obiectul (definiția lui, cu tot ce are înăuntru) ca rădăcină; transformarea
// instanței nu se păstrează (obiectul stă în originea modelului nou, la colțul lui minim).
[[nodiscard]] Model extractInstance(const Model& source, InstanceId instance);

// --- Componentele legate din bibliotecă (pasul 8; docs/plans/mobilier-pe-corp.md, §6) ---

// Definiția din model cu cheia `sourceKey` care se poate refolosi la o plasare în `context`: are cel puțin o
// instanță în arborele modelului (definițiile orfane nu se numără) și nu e `context` sau un strămoș al lui.
// La mai multe, id-ul cel mai mic; nulă dacă nu există.
[[nodiscard]] DefinitionId findLinkedDefinition(
    const Model& model, std::string_view sourceKey, DefinitionId context);

// O instanță nouă (legată) a definiției în `context`.
InstanceId placeLinked(
    Model& model, DefinitionId definition, DefinitionId context, const math::Transform& placement);

// Lista „În model”: componentele făcute de utilizator (nu grupuri, colecții, piese de mobilier sau obiecte
// dinamice) cu cel puțin o instanță în arborele modelului, cu numărul de apariții, în ordinea id-urilor.
[[nodiscard]] std::vector<std::pair<DefinitionId, int>> userComponents(const Model& model);

} // namespace aion::model
