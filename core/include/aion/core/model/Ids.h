#pragma once

#include "aion/core/geom/Id.h"

// Identificatorii modelului (definiții, instanțe, tag-uri), separați ca geometria (selecția) să-i poată folosi
// fără să depindă de Model.
namespace aion::model {

struct DefinitionTag;
struct InstanceTag;
struct TagTag;
struct MaterialTag;
struct GuideTag;
struct AnnotationTag;
struct SectionTag;
struct SceneTag;
struct AnimationTag;
struct CameraTag;
struct LightTag;
struct TextureTag;

using DefinitionId = geom::Id<DefinitionTag>;
using InstanceId = geom::Id<InstanceTag>;
using TagId = geom::Id<TagTag>;
using MaterialId = geom::Id<MaterialTag>;
using GuideId = geom::Id<GuideTag>;
using AnnotationId = geom::Id<AnnotationTag>;
using SectionId = geom::Id<SectionTag>;
using SceneId = geom::Id<SceneTag>;
using AnimationId = geom::Id<AnimationTag>;
using CameraId = geom::Id<CameraTag>;
using LightId = geom::Id<LightTag>;
using TextureId = geom::Id<TextureTag>;

} // namespace aion::model
