#pragma once

#include "aion/core/geom/Bezier.h"
#include "aion/core/model/Model.h"

// Curbele Pen-ului în Modelare (ADR 0036): muchii obișnuite (eșantionarea curbei, cu același `curve` id) plus
// entitatea `PenCurve`, cu punctele și mânerele. Curba e editabilă cât timp muchiile ei sunt neschimbate și plane:
// după Push/Pull, un vârf mutat sau o mutare în alt grup, rămâne o curbă obișnuită (ca arcele).
namespace aion::model {

// Desenează curba în definiție (o față nouă dacă e închisă; o față-gazdă se împarte) și o ține minte.
PenCurveId addPenCurveGeometry(Model& model, DefinitionId definition, const geom::BezierPath& path, const math::Vec3d& normal);
// Redesenează curba cu forma nouă (fața-gazdă se reface, cu materialul ei); false dacă nu mai e editabilă.
bool replacePenCurve(Model& model, PenCurveId id, const geom::BezierPath& path);
[[nodiscard]] bool penCurveEditable(const Model& model, PenCurveId id);
// Curba Pen căreia îi aparține muchia (după `curve` id, în definiția dată); gol = niciuna.
[[nodiscard]] PenCurveId penCurveForEdge(const Model& model, DefinitionId definition, geom::EdgeId edge);

} // namespace aion::model
