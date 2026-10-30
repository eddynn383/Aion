#pragma once

#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/inference/Inference.h"
#include "aion/core/inference/SnapScene.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Operations.h"
#include "aion/core/math/Vec.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/Picking.h"

#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Uneltele de modelare. Primesc evenimente abstracte (nu evenimente Qt), ca aceeași logică
// să meargă pe orice platformă (PLAN.md, regula 2 de arhitectură).
namespace aion::tools {

using math::Vec2d;
using math::Vec3d;

struct Modifiers {
    bool shift = false;
    bool command = false; // Cmd pe macOS, Ctrl în rest
    bool alt = false; // Option pe macOS
};

struct PointerEvent {
    Vec2d screen { 0.0 }; // pixeli logici, originea stânga-sus
    Modifiers modifiers;
    int clickCount = 1; // 2 = dublu-click, 3 = triplu-click
};

// Săgețile blochează inferența pe o axă: → roșie, ← verde, ↑ albastră, ↓ paralel cu muchia de referință.
enum class Key { Escape, Enter, Left, Right, Up, Down, Delete, Other };

struct KeyEvent {
    Key key = Key::Other;
    Modifiers modifiers;
};

// Ce desenează unealta peste model (previzualizări), în coordonate 3D.
struct OverlayLine {
    Vec3d a;
    Vec3d b;
    bool dashed = false;
    inference::Tint tint = inference::Tint::Neutral;
    bool onTop = false; // ghidajele de inferență se văd și prin fețe

    bool operator==(const OverlayLine&) const = default;
};

// Punctul inferat de sub cursor, cu tipul lui (culoarea marcajului) și eticheta („Endpoint”).
struct InferenceMarker {
    Vec3d point { 0.0 };
    inference::Kind kind = inference::Kind::None;
    inference::Tint tint = inference::Tint::Neutral; // culoarea direcției (axă, magenta), dacă există
    std::string label;
    bool locked = false;

    bool operator==(const InferenceMarker&) const = default;
};

// Un punct de prindere al unui obiect (Move): colț sau mijloc de muchie al cutiei lui, desenat mare și vizibil.
struct Grip {
    Vec3d point { 0.0 };
    inference::Kind kind = inference::Kind::Endpoint; // Endpoint = colț, Midpoint = mijloc de muchie
    bool hot = false; // sub cursor
    bool hidden = false; // în spatele obiectului (desenat estompat, prins doar dacă nu e altul vizibil)

    bool operator==(const Grip&) const = default;
};

// O cotă afișată pe un segment în timpul desenării („2.5 m”).
struct Dimension {
    Vec3d a;
    Vec3d b;
    std::string text;

    bool operator==(const Dimension&) const = default;
};

// Ce se lipește (unealta `place`): colțul de prindere și muchiile copiei, în coordonatele contextului.
struct PlaceSource {
    Vec3d base { 0.0 };
    std::vector<std::pair<Vec3d, Vec3d>> edges;
};

struct Overlay {
    std::vector<OverlayLine> lines;
    std::vector<Vec3d> points;
    // Dreptunghiul de selecție, în coordonate de ecran; `crossing` = tras de la dreapta la stânga.
    std::optional<std::pair<Vec2d, Vec2d>> selectionRect;
    bool crossing = false;
    // Muchiile marcate de unealtă (de exemplu cele pe care Eraser le va șterge).
    std::vector<geom::EdgeId> markedEdges;
    std::vector<model::InstanceId> markedInstances;
    std::optional<InferenceMarker> marker;
    std::vector<Dimension> dimensions;
    std::vector<Grip> grips;

    bool operator==(const Overlay&) const = default;
};

// Ghidajele și marcajul inferenței, adăugate în overlay (și de viewport, pentru obiectul tras din bibliotecă).
void appendInference(Overlay& o, const inference::Inference& inf);

// Ce arată Measurements box: eticheta („Lungime”) și valoarea curentă („2.5 m”, gol dacă nu e nimic de măsurat).
struct Measurement {
    std::string label;
    std::string value;
};

// Tot ce poate folosi o unealtă; aplicația îl construiește și îl ține în viață.
// Ce cere o unealtă documentului după un eveniment: intrarea într-un grup (dublu-click) sau ieșirea (Esc).
struct ToolRequests {
    model::InstanceId enter;
    bool exit = false;
    // Text: adnotarea nou creată, al cărei text îl scrie utilizatorul.
    model::AnnotationId editAnnotation;
    // Paint Bucket: materialul luat cu pipeta (Option) sau materialul nou intrat în model la vopsire.
    std::optional<model::MaterialId> sampledMaterial;
    model::MaterialId addedMaterial;
    // Tragerea pivotului: punctul nou (în coordonatele contextului), pentru previzualizare; `pivotDone` = tragerea
    // s-a terminat — cu `pivot` = punctul ales, fără = anulată.
    std::optional<math::Vec3d> pivot;
    bool pivotDone = false;
    // Lipirea (unealta internă `place`): punctul ales pentru colțul copiei (click) sau renunțarea (Esc).
    std::optional<Vec3d> placeAt;
    bool placeCancel = false;
    // Pen în Animare: curba terminată (în coordonatele contextului) și normala planului ei; Document face din ea o cale.
    std::optional<geom::BezierPath> penPath;
    math::Vec3d penNormal { 0.0, 0.0, 1.0 };
    // Editarea unei curbe (penedit): forma nouă, după o tragere / inserare / ștergere (un pas de undo), și ieșirea.
    std::optional<geom::BezierPath> curveEdit;
    bool curveEditDone = false;
    // Select: dublu click pe o curbă Pen editabilă → Document intră în editarea ei.
    model::PenCurveId editPenCurve;
};

struct ToolContext {
    ToolContext(geom::Mesh& mesh, commands::CommandStack& stack, geom::Selection& selection, const view::Camera& camera)
        : mesh(mesh)
        , stack(stack)
        , selection(selection)
        , camera(camera)
    {
    }

    geom::Mesh& mesh; // geometria contextului de editare
    commands::CommandStack& stack;
    geom::Selection& selection;
    const view::Camera& camera; // în coordonatele contextului (vezi Camera::inContext)
    // Modelul și definiția editată; nul în testele care lucrează pe o singură geometrie.
    model::Model* model = nullptr;
    model::DefinitionId context;
    ToolRequests* requests = nullptr;
    // Paint Bucket: materialul curent (din model), sau unul din bibliotecă care intră în model la prima folosire.
    model::MaterialId paintMaterial;
    const model::Material* libraryMaterial = nullptr;
    // Ghidajele contextului, pentru inferență.
    std::span<const inference::GuideRef> guides;
    // Snap la grid: pasul în mm (0 = oprit).
    double gridStep = 0.0;
    // Scena de prindere: geometria obiectelor (grupuri, componente, primitive) văzută de inferență. Nul = doar mesh-ul.
    const inference::SnapScene* snap = nullptr;
    // Transformarea contextului editat (local → model), pentru ce se păstrează în coordonatele modelului.
    math::Transform toWorld;
    // Pivotul selecției (marcajul din viewport), în coordonatele contextului: centrul implicit pentru Rotate și
    // punctul fix pentru Scale cu Option. Gol dacă nu e nimic selectat.
    std::optional<Vec3d> pivot;
    // Camera sau lumina selectată (fără altă selecție): ținta lui Move / Rotate / Scale.
    std::optional<model::SceneObjectRef> object;
    // Animare: Pen-ul desenează o cale de parcurgere (o cere prin ToolRequests::penPath), nu geometrie.
    bool drawsPaths = false;
    // Curba editată (penedit), în coordonatele contextului, și normala planului ei; Document le pune la fiecare apel.
    std::optional<geom::BezierPath> editCurve;
    math::Vec3d editNormal { 0.0, 0.0, 1.0 };
    // Lipirea: colțul de prindere al copiei și muchiile ei (previzualizarea), în coordonatele contextului.
    std::optional<PlaceSource> place;
};

class Tool {
public:
    virtual ~Tool() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;

    virtual void pointerDown(ToolContext& ctx, const PointerEvent& e) = 0;
    // Mișcarea cursorului, cu sau fără butonul apăsat.
    virtual void pointerMove(ToolContext& ctx, const PointerEvent& e) = 0;
    virtual void pointerUp(ToolContext& ctx, const PointerEvent& e) = 0;
    // Întoarce true dacă unealta a folosit tasta.
    virtual bool key(ToolContext& ctx, const KeyEvent& e);
    // Renunță la operația în curs (Esc sau schimbarea uneltei).
    virtual void cancel(ToolContext& ctx);

    [[nodiscard]] virtual Overlay overlay() const { return {}; }
    // Ce e sub cursor (pentru evidențierea la hover).
    [[nodiscard]] virtual view::PickResult hover() const { return {}; }
    // Indicația din status bar („Alege punctul de început”).
    [[nodiscard]] virtual std::string status() const = 0;
    // Săgețile sunt acum ale uneltei (o operație în curs, sau aleg ceva, ca axa la Rotate)? Altfel, în Animare,
    // merg la timeline (cadre, chei).
    [[nodiscard]] virtual bool ownsArrowKeys() const { return false; }

    // Measurements box: nullopt dacă unealta nu primește valori tastate.
    [[nodiscard]] virtual std::optional<Measurement> measurement() const { return std::nullopt; }
    // Aplică o valoare tastată („2500”, „2.5m”, „3000,2000”). Întoarce false dacă textul nu e valid acum.
    virtual bool applyMeasurement(ToolContext& /*ctx*/, std::string_view /*text*/) { return false; }
    // Undo (⌘Z) cât unealta e în mijlocul unei operații: un pas înapoi în ea (Pen: ultimul punct). false = undo-ul
    // obișnuit, din istoric.
    virtual bool undoStep(ToolContext& /*ctx*/) { return false; }
    // Obiectele pe care operația în curs le transformă (Move / Rotate / Scale, după ce baza sau centrul e ales): nu
    // sunt surse de inferență, ca să nu se prindă de ele însele. Înainte, vârfurile lor se prind (colțul apucat).
    [[nodiscard]] virtual std::set<model::InstanceId> snapExclude() const { return {}; }
};

// "select", "line", "arc", "rectangle", "circle", "polygon", "pushpull", "offset", "move", "rotate", "scale", "eraser";
// nullptr pentru o unealtă necunoscută.
[[nodiscard]] std::unique_ptr<Tool> makeTool(std::string_view id);

// Punctul 3D de sub cursor: pe geometrie dacă există, altfel pe planul solului (Z = 0).
struct SurfacePoint {
    Vec3d point { 0.0 };
    view::PickResult pick;
};
[[nodiscard]] std::optional<SurfacePoint> surfacePoint(const ToolContext& ctx, Vec2d screen);

// Ce e sub cursor, inclusiv grupurile și obiectele din context (cea mai apropiată dintre geometrie și instanțe).
[[nodiscard]] view::PickResult pickWithInstances(const ToolContext& ctx, Vec2d screen, view::PickOptions options = {});

// Ce selectează un click (Select): o muchie (cu toată curba ei), o față sau o instanță.
[[nodiscard]] geom::Selection selectionAt(const ToolContext& ctx, Vec2d screen);

// Șterge selecția: fețele selectate, apoi muchiile selectate (cu fețele lor). O singură comandă.
// Șterge selecția (un pas de undo). keepFaces (⌫, guma): fața pe care sunt desenate liniile rămâne; altfel (Delete)
// liniile pleacă cu fețele lor.
bool deleteSelection(ToolContext& ctx, bool keepFaces = false);

} // namespace aion::tools
