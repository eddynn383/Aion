#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"

#include <set>
#include <string>

// Follow Me (ca în SketchUp): profilul (o față) se extrudează de-a lungul unui traseu de muchii legate. La fiecare
// cot, secțiunea se proiectează pe planul bisector (îmbinare „în unghi”), deci se păstrează grosimea. Un traseu
// deschis pornește de la capătul de lângă profil și primește un capac la final (profilul e capacul de început);
// un traseu închis (un inel: o margine de masă, o cornișă) dă un volum continuu, fără capace, iar fața profilului
// dispare. Benzile de pe curbe (arc, cerc) și cele ale unui profil rotund au muchiile netede. Vezi ADR 0031.
namespace aion::geom {

struct SweepResult {
    Selection selection; // fețele noi
    std::string error;   // gol = reușit

    [[nodiscard]] bool ok() const { return error.empty(); }
};

// Profilul nu are voie să aibă găuri și nici să fie paralel cu începutul traseului.
SweepResult followMe(Mesh& mesh, FaceId profile, const std::set<EdgeId>& path);

} // namespace aion::geom
