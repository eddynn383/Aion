#pragma once

#include "aion/core/model/Model.h"

#include <vector>

// „Întinde” (pasul 11; docs/plans/mobilier-pe-corp.md, §9): lățirea / înălțarea unui corp făcut din plăci,
// prin unul sau mai multe plane perpendiculare pe o axă. Plăcile tăiate de un plan se lungesc (parametrul
// lor), cele de partea + se mută, feroneria doar se mută. Un corp din bibliotecă își schimbă câmpul. Totul pe
// definiția corpului, deci pe toate copiile lui legate.
namespace aion::model {

enum class StretchRefusal {
    None,
    ThroughThickness, // un plan taie o placă prin grosime
    ModifiedBoard, // un plan taie o placă modificată de mână sau desprinsă de parametri
    FreeGeometry, // un plan taie geometrie liberă (fețe care nu sunt plăci) sau o piesă rotită oblic
    NotABody, // obiectul nu are piese
};

struct StretchResult {
    bool ok = false;
    StretchRefusal reason = StretchRefusal::None;
    InstanceId piece; // piesa din cauza căreia s-a refuzat (dacă e una)
};

// Întinde corpul `body` pe axa `axis` (0 = X, 1 = Y, 2 = Z, în coordonatele corpului) cu `distance` mm,
// împărțiți egal între planele de la pozițiile `planes` (coordonatele corpului). La refuz modelul rămâne
// neschimbat.
StretchResult stretch(Model& model, InstanceId body, int axis, std::vector<double> planes, double distance);

// „În N locuri egale”: întinderea corpului pe axă se împarte în N zone egale; planele sunt la mijlocul
// fiecărei zone.
[[nodiscard]] std::vector<double> equalPlanes(const Model& model, InstanceId body, int axis, int n);

} // namespace aion::model
