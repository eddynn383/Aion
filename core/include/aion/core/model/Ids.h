#pragma once

#include "aion/core/geom/Id.h"

// Identificatorii modelului (definiții, instanțe, tag-uri), separați ca geometria (selecția) să-i poată folosi
// fără să depindă de Model.
namespace aion::model {

struct DefinitionTag;
struct InstanceTag;
struct TagTag;
struct MaterialTag;

using DefinitionId = geom::Id<DefinitionTag>;
using InstanceId = geom::Id<InstanceTag>;
using TagId = geom::Id<TagTag>;
using MaterialId = geom::Id<MaterialTag>;

} // namespace aion::model
