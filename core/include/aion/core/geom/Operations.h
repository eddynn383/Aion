#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/math/Transform.h"

#include <set>
#include <unordered_map>

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

// Desenează o curbă (cerc, arc, poligon) din segmente: toate muchiile primesc același identificator de curbă,
// ca să se selecteze și să se șteargă împreună. `smooth`: cerc sau arc (extrudat, are laturile soft).
DrawLineResult drawCurve(Mesh& mesh, const std::vector<Vec3d>& points, bool closed, bool smooth);
// La fel, cu un id de curbă dat (o curbă Pen redesenată își păstrează id-ul).
DrawLineResult drawCurve(Mesh& mesh, const std::vector<Vec3d>& points, bool closed, bool smooth, std::uint32_t curve);

// Închide fețe în buclele plane formate de muchiile date (de exemplu după ce fața a fost ștearsă și muchiile au rămas),
// ca la redesenarea unei muchii în SketchUp. O față nouă lângă alte fețe ia sensul lor (normala spre exterior la un
// volum). Întoarce fețele create.
DrawLineResult createFaces(Mesh& mesh, const std::vector<EdgeId>& edges);

// Muchiile curbei din care face parte muchia (doar ea, dacă nu e pe o curbă).
[[nodiscard]] std::vector<EdgeId> curveEdges(const Mesh& mesh, EdgeId edge);

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

// Un punct sigur din interiorul feței (centrul celui mai mare triunghi din triangulare).
[[nodiscard]] Vec3d faceInteriorPoint(const Mesh& mesh, FaceId face);

// Vârfurile entităților selectate (capetele muchiilor și vârfurile buclelor fețelor).
[[nodiscard]] std::set<VertexId> verticesOf(const Mesh& mesh, const Selection& selection);

// Move / Rotate / Scale: aplică transformarea pe vârfuri. Geometria conectată se deformează; fețele atinse
// își recalculează planul, iar cele care nu mai sunt plane se pliază în triunghiuri (ca în SketchUp).
// Întoarce false, fără nicio modificare, dacă transformarea ar strânge o muchie într-un punct.
bool transformVertices(Mesh& mesh, const std::set<VertexId>& vertices, const math::Transform& transform);

// Copia transformată a selecției (Option în Move și Rotate), lipită de geometria existentă ca orice desen.
// Întoarce entitățile copiei.
Selection copySelection(Mesh& mesh, const Selection& selection, const math::Transform& transform);
// La fel, dar din altă geometrie (de exemplu la Make Group sau Explode): `source` poate fi și `mesh`.
Selection copyEntities(const Mesh& source, const Selection& selection, Mesh& mesh, const math::Transform& transform);

// Conturul exterior al feței deplasat cu `distance` (pozitiv = spre interior), în planul feței.
// nullopt dacă rezultatul s-ar autointersecta, s-ar întoarce pe dos sau ar atinge o gaură.
[[nodiscard]] std::optional<std::vector<Vec3d>> offsetOutline(const Mesh& mesh, FaceId face, double distance);

// Offset (F): desenează conturul deplasat (care împarte fața sau adaugă un inel în jurul ei).
// Muchiile noi moștenesc curbele conturului. Întoarce false dacă offset-ul nu se poate face.
bool offsetFace(Mesh& mesh, FaceId face, double distance);

// Construirea directă a geometriei importate (OBJ, glTF): fără intersecții și fără refacerea planurilor, pentru
// că geometria vine deja curată și poate fi mare. Vârfurile se refolosesc după poziție (grilă de 0.01 mm).
class MeshBuilder {
public:
    explicit MeshBuilder(Mesh& mesh);

    VertexId vertex(const Vec3d& position);
    // O față din punctele date, în ordinea lor (normala după regula mâinii drepte). Poligoanele neplane se împart
    // în triunghiuri; cele degenerate sau identice cu o față existentă se sar. Întoarce fețele create.
    std::vector<FaceId> face(const std::vector<Vec3d>& points);
    // La final: fețele coplanare vecine se unesc, iar muchiile dintre fețe aproape coplanare (sub `smoothAngle`
    // radiani) devin soft, ca suprafețele curbe triangulate să arate netede.
    void finish(double smoothAngle = 0.35);

private:
    Mesh& m_mesh;
    std::unordered_map<std::uint64_t, VertexId> m_grid;
};

// Intersect Faces: desenează segmentele în care fețele `faces` taie celelalte fețe (`withAll`) sau doar pe ele
// între ele. Liniile noi împart fețele, ca orice linie desenată. Întoarce numărul de segmente.
std::size_t intersectFaces(Mesh& mesh, const std::set<FaceId>& faces, bool withAll);

// Regiunea unei fețe (în interior, nu pe margine și nu într-o gaură).
[[nodiscard]] bool faceContainsPoint(const Mesh& mesh, FaceId face, const Vec3d& point);

} // namespace aion::geom
