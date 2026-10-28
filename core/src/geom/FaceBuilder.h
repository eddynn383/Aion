#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"

#include <utility>
#include <vector>

// Construcția unor fețe noi din poligoane (bevel, Follow Me): desenate cu drawPolygon, ca desenul obișnuit, cu
// sensul și stilul cerute; fețele închise de drawPolygon fără să fie cerute se scot.
namespace aion::geom {

// Atributele unei fețe, păstrate când e refăcută.
struct FaceStyle {
    model::MaterialId material;
    model::MaterialId backMaterial;
    model::TagId tag;
    TextureTransform texture;
};

[[nodiscard]] FaceStyle styleOf(const Face& f);

// O față nouă de desenat: poligonul, normala dorită (spre exterior) și stilul.
struct NewFace {
    std::vector<Vec3d> points;
    Vec3d normal { 0.0, 0.0, 1.0 };
    FaceStyle style;
    bool selected = false; // rămâne în selecția întoarsă
};

// Desenează fețele, le orientează și le dă stilul; scoate fețele noi necerute; muchiile dintre perechile de puncte
// `smoothEdges` devin netede (soft + smooth). Întoarce fețele marcate `selected`.
Selection buildFaces(Mesh& mesh, const std::vector<NewFace>& faces, const std::vector<std::pair<Vec3d, Vec3d>>& smoothEdges);

} // namespace aion::geom
