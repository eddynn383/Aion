#pragma once

#include "aion/core/geom/Mesh.h"

#include <vector>

// Operațiile de modelare compuse din primitivele Mesh (vezi ADR 0003).
namespace aion::geom {

struct DrawLineResult {
    // Bucățile liniei, de la a la b (unele pot exista deja în model).
    std::vector<EdgeId> edges;
    std::vector<FaceId> createdFaces;
    std::vector<FaceId> removedFaces;
};

// Desenează un segment cu comportamentul „lipicios” al geometriei (PLAN.md 4.3): se unește cu
// vârfurile existente, împarte muchiile pe care le atinge sau le intersectează, nu dublează muchii
// existente, închide fețe noi în ciclurile coplanare și împarte fețele pe care le traversează.
DrawLineResult drawLine(Mesh& mesh, const Vec3d& a, const Vec3d& b);

// Desenează o polilinie închisă (de exemplu conturul unui dreptunghi).
DrawLineResult drawPolygon(Mesh& mesh, const std::vector<Vec3d>& points);

// Șterge o muchie, fețele care o folosesc și vârfurile rămase fără muchii.
void eraseEdge(Mesh& mesh, EdgeId edge);

// Șterge o față (muchiile rămân).
void eraseFace(Mesh& mesh, FaceId face);

// Întoarce fața (normala și sensul buclelor) dacă normala ei nu e în sensul `wanted`. Întoarce ID-ul feței
// (unul nou, dacă a fost întoarsă).
FaceId orientFace(Mesh& mesh, FaceId face, const Vec3d& wanted);

// Dacă muchia are exact două fețe coplanare cu aceeași orientare, le unește într-una și șterge muchia
// (și vârfurile rămase fără muchii). Întoarce fața rezultată sau un ID nul dacă nu s-a schimbat nimic.
FaceId mergeFacesAcross(Mesh& mesh, EdgeId edge);

// Dacă vârful are exact două muchii coliniare, folosite de aceleași fețe, le înlocuiește cu una singură.
bool dissolveVertex(Mesh& mesh, VertexId vertex);

struct PushPullResult {
    bool done = false;
    std::vector<FaceId> createdFaces; // fețele noi (capacul și laturile), după toate unirile
};

// Push/Pull (SketchUp): extrudă fața cu `distance` mm de-a lungul normalei ei (negativ = în sens opus).
// - O față „liberă” (nicio muchie nu are alte fețe) devine o cutie închisă; fața inițială rămâne baza.
// - O față a unui solid se mută: laturile noi se unesc cu fețele coplanare vecine.
// - Împinsă în solid, taie materialul; dacă ajunge pe o față opusă, cele două se anulează (gaură).
// `keepOriginal` (Option/Ctrl în SketchUp) păstrează fața inițială și în al doilea caz.
PushPullResult pushPull(Mesh& mesh, FaceId face, double distance, bool keepOriginal = false);

// Regiunea unei fețe (în interior, nu pe margine și nu într-o gaură).
[[nodiscard]] bool faceContainsPoint(const Mesh& mesh, FaceId face, const Vec3d& point);

} // namespace aion::geom
