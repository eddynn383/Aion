#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"

#include <set>
#include <string>
#include <variant>

// Bevel (rotunjire) și chamfer (teșitură) pe muchii sau pe toate muchiile unei fețe. `distance` = cât intră teșitura
// pe fiecare față (retragerea de la muchie); `segments` = 1 pentru chamfer, mai multe pentru o rotunjire tangentă la
// ambele fețe. Fețele atinse se refac cu drawPolygon (aceeași cale ca desenul), cu materialele lor; benzile unei
// rotunjiri au muchiile netede. Vezi ADR 0029.
//
// Limitele: muchii convexe între exact două fețe plane fără găuri; vârfurile de la capete au exact trei muchii (ca la
// cutii și la formele extrudate). Altfel operația nu schimbă nimic și întoarce motivul.
namespace aion::geom {

struct BevelResult {
    Selection selection; // fețele noi (benzile și fața micșorată)
    std::string error;   // gol = reușit

    [[nodiscard]] bool ok() const { return error.empty(); }
};

// Muchiile date (care nu au vârfuri comune), fiecare tratată separat.
BevelResult bevelEdges(Mesh& mesh, const std::set<EdgeId>& edges, double distance, int segments);

// Toate muchiile feței, cu colțurile îmbinate „în unghi” (ca la o ramă).
BevelResult bevelFace(Mesh& mesh, FaceId face, double distance, int segments);

} // namespace aion::geom
