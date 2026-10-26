#pragma once

#include "aion/core/geom/Id.h"
#include "aion/core/math/Plane.h"
#include "aion/core/math/Vec.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/Picking.h"

#include <deque>
#include <optional>
#include <string_view>
#include <vector>

namespace aion::geom {
class Mesh;
}

// Inferența (snap): din poziția cursorului și din context (punctul de pornire, geometria din jur,
// punctele „atinse” recent) alege punctul pe care îl vrea utilizatorul, ca în SketchUp.
namespace aion::inference {

using math::Vec2d;
using math::Vec3d;

enum class Kind {
    None, // nu există niciun punct (raza nu atinge nimic)
    Ground, // planul solului sau planul de lucru, fără nicio inferență
    OnFace,
    OnEdge,
    Endpoint,
    Midpoint,
    Intersection, // o direcție (axă, paralelă) care taie o muchie
    OnAxis, // pe o axă care trece prin punctul de pornire
    Parallel, // paralel cu muchia de referință
    Perpendicular, // perpendicular pe muchia de referință
    FromPoint, // aliniat pe o axă cu un punct de referință
};

enum class Axis { None, X, Y, Z };

// Culoarea unui ghidaj: a axei, magenta (paralel/perpendicular) sau neutră.
enum class Tint { Neutral, X, Y, Z, Magenta };

struct Guide {
    Vec3d a;
    Vec3d b;
    Tint tint = Tint::Neutral;
    bool dashed = false;
};

struct Inference {
    Kind kind = Kind::None;
    Vec3d point { 0.0 };
    view::PickResult pick; // ce era sub cursor (pentru fața pe care s-a dat click etc.)
    geom::VertexId vertex; // pentru Endpoint
    geom::EdgeId edge; // pentru OnEdge, Midpoint, Intersection
    geom::FaceId face; // pentru OnFace
    // Direcția liniară din punctul de pornire (unitară), dacă inferența o folosește:
    // OnAxis, Parallel, Perpendicular sau o combinație (Intersection, FromPoint pe o axă).
    std::optional<Vec3d> direction;
    Kind directionKind = Kind::None;
    Axis axis = Axis::None; // axa direcției, pentru OnAxis
    std::vector<Guide> guides;
    bool locked = false; // rezultatul a fost constrâns de un lock

    [[nodiscard]] bool valid() const { return kind != Kind::None; }
    [[nodiscard]] bool isPoint() const
    {
        return kind == Kind::Endpoint || kind == Kind::Midpoint || kind == Kind::Intersection;
    }
};

// Eticheta afișată lângă cursor: „Endpoint”, „On Red Axis”, „Parallel to Edge”; gol pentru Ground.
[[nodiscard]] std::string_view label(const Inference& inference);
[[nodiscard]] Tint axisTint(Axis axis);
// Punctul de pe dreapta (origin, dir unitar) a cărui proiecție e cea mai apropiată de cursor; pe ecran, nu în 3D:
// în perspectivă cele două diferă, iar utilizatorul vede ecranul.
[[nodiscard]] std::optional<Vec3d> pointOnLineUnderCursor(
    const view::Camera& camera, Vec2d screen, const Vec3d& origin, const Vec3d& dir);
[[nodiscard]] Vec3d axisDirection(Axis axis);

struct Options {
    double pointRadiusPx = 10.0; // capete și mijloace
    double edgeRadiusPx = 6.0;
    double lineRadiusPx = 12.0; // axe, paralele, aliniere cu puncte de referință
    // Candidatul ales anterior rămâne ales până când cursorul iese din raza lui înmulțită cu acest factor,
    // ca snap-ul să nu sară între candidați apropiați.
    double hysteresis = 1.6;
    std::size_t maxReferences = 4;
};

class Engine {
public:
    explicit Engine(Options options = {})
        : m_options(options)
    {
    }

    // `from`: punctul de pornire al operației (de exemplu începutul liniei), dacă există.
    // `plane`: planul de lucru când cursorul nu atinge nimic (implicit solul, Z = 0).
    Inference infer(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen, std::optional<Vec3d> from,
        std::optional<math::Plane> plane = std::nullopt);

    // Shift: blochează inferența curentă (direcția, sau planul feței, sau direcția spre punctul atins).
    void lockCurrent();
    // Săgețile: blochează pe o axă; aceeași axă a doua oară deblochează.
    void toggleAxisLock(Axis axis);
    // Săgeata în jos: blochează pe direcția muchiei de referință (paralel).
    void toggleReferenceLock();
    void unlock();
    [[nodiscard]] bool locked() const { return m_lock.kind != LockKind::None; }

    // Uită tot (punctele de referință, lock-ul, ultima inferență): la o operație nouă.
    void reset();
    // Uită doar lock-ul și ultima inferență, păstrând punctele de referință (de exemplu după un segment).
    void resetLock();
    // Ca resetLock(), dar operația continuă din `from`: o săgeată apăsată imediat (fără mișcare) blochează axa prin el.
    void restartFrom(const Vec3d& from);

    [[nodiscard]] const Inference& last() const { return m_last; }
    [[nodiscard]] const std::deque<Vec3d>& references() const { return m_references; }
    [[nodiscard]] std::optional<geom::EdgeId> referenceEdge() const { return m_referenceEdge; }

private:
    enum class LockKind { None, Line, Plane };
    struct Lock {
        LockKind kind = LockKind::None;
        Vec3d origin { 0.0 };
        Vec3d direction { 0.0 };
        math::Plane plane;
        Kind reason = Kind::None; // inferența care a produs lock-ul (pentru etichetă și culoare)
        Axis axis = Axis::None;
    };

    Inference inferFree(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen, std::optional<Vec3d> from,
        const std::optional<math::Plane>& plane);
    Inference inferLocked(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen, std::optional<Vec3d> from);
    void remember(const geom::Mesh& mesh, const Inference& result);

    Options m_options;
    Inference m_last;
    std::optional<Vec3d> m_lastFrom;
    Lock m_lock;
    std::deque<Vec3d> m_references;
    std::optional<geom::EdgeId> m_referenceEdge;
    std::optional<Vec3d> m_referenceDirection;
    std::optional<math::Plane> m_lastFacePlane;
};

} // namespace aion::inference
