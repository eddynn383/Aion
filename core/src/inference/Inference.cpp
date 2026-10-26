#include "aion/core/inference/Inference.h"

#include "aion/core/geom/Mesh.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace aion::inference {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
// O direcție aproape paralelă cu raza de privire nu se poate urmări pe ecran.
constexpr double kMaxViewAlignment = 0.995;
// Avantajul axelor față de alte direcții la distanțe egale pe ecran (px).
constexpr double kAxisBonusPx = 1.5;

double screenDistance(const view::Camera& camera, const Vec3d& p, Vec2d screen)
{
    const auto s = camera.project(p);
    return s ? glm::length(*s - screen) : kInf;
}

// Punctul de pe dreapta (origin, dir) cel mai apropiat de rază; nullopt dacă sunt paralele.
std::optional<Vec3d> closestOnLine(const math::Ray& ray, const Vec3d& origin, const Vec3d& dir)
{
    const Vec3d w0 = origin - ray.origin;
    const double b = glm::dot(dir, ray.direction);
    const double denom = 1.0 - b * b;
    if (denom < 1e-9) {
        return std::nullopt;
    }
    const double d = glm::dot(dir, w0);
    const double e = glm::dot(ray.direction, w0);
    const double s = (b * e - d) / denom;
    return origin + dir * s;
}

// Intersecția a două drepte în spațiu; nullopt dacă sunt paralele sau nu se ating (în toleranța dată).
std::optional<Vec3d> intersectLines(const Vec3d& o1, const Vec3d& d1, const Vec3d& o2, const Vec3d& d2, double eps)
{
    const Vec3d w0 = o1 - o2;
    const double b = glm::dot(d1, d2);
    const double denom = 1.0 - b * b;
    if (denom < 1e-9) {
        return std::nullopt;
    }
    const double d = glm::dot(d1, w0);
    const double e = glm::dot(d2, w0);
    const double s = (b * e - d) / denom;
    const double t = (e - b * d) / denom;
    const Vec3d p1 = o1 + d1 * s;
    const Vec3d p2 = o2 + d2 * t;
    if (glm::length(p1 - p2) > eps) {
        return std::nullopt;
    }
    return (p1 + p2) * 0.5;
}

// Punctul e vizibil dacă nu există o față între el și cameră (aceeași regulă ca la picking).
bool visible(const math::Ray& ray, const view::PickResult& facePick, const Vec3d& p)
{
    if (facePick.kind != view::PickResult::Kind::Face) {
        return true;
    }
    const double faceDepth = glm::dot(facePick.point - ray.origin, ray.direction);
    const double depth = glm::dot(p - ray.origin, ray.direction);
    return depth <= faceDepth + 1.0 + faceDepth * 1e-4;
}

Vec3d edgeDirection(const geom::Mesh& mesh, geom::EdgeId id)
{
    const geom::Edge& e = mesh.edge(id);
    return glm::normalize(mesh.position(e.v1) - mesh.position(e.v0));
}

bool isAxisAligned(const Vec3d& dir)
{
    for (Axis a : { Axis::X, Axis::Y, Axis::Z }) {
        if (tol::parallel(dir, axisDirection(a), 1e-6)) {
            return true;
        }
    }
    return false;
}

Tint directionTint(Kind kind, Axis axis)
{
    if (kind == Kind::OnAxis) {
        return axisTint(axis);
    }
    if (kind == Kind::Parallel || kind == Kind::Perpendicular) {
        return Tint::Magenta;
    }
    return Tint::Neutral;
}

struct Direction {
    Vec3d dir;
    Kind kind;
    Axis axis = Axis::None;
};

struct Linear {
    Direction direction;
    Vec3d point;
    double distance = kInf;
};

} // namespace

// Punctul de pe dreaptă a cărui proiecție e cea mai apropiată de cursor (pe ecran, nu în 3D:
// în perspectivă cele două diferă, iar utilizatorul vede ecranul).
std::optional<Vec3d> pointOnLineUnderCursor(const view::Camera& camera, Vec2d screen, const Vec3d& origin, const Vec3d& dir)
{
    const math::Ray ray = camera.rayAt(screen);
    const double step = camera.worldPerPixelAtTarget() * 100.0;
    const auto pa = camera.project(origin);
    const auto pb = camera.project(origin + dir * step);
    if (pa && pb) {
        const Vec2d ab = *pb - *pa;
        const double len2 = glm::dot(ab, ab);
        if (len2 > 1e-9) {
            const Vec2d q = *pa + ab * (glm::dot(screen - *pa, ab) / len2);
            if (const auto p = closestOnLine(camera.rayAt(q), origin, dir)) {
                return p;
            }
        }
    }
    return closestOnLine(ray, origin, dir);
}

Tint axisTint(Axis axis)
{
    switch (axis) {
    case Axis::X:
        return Tint::X;
    case Axis::Y:
        return Tint::Y;
    case Axis::Z:
        return Tint::Z;
    case Axis::None:
        break;
    }
    return Tint::Neutral;
}

Vec3d axisDirection(Axis axis)
{
    switch (axis) {
    case Axis::X:
        return { 1.0, 0.0, 0.0 };
    case Axis::Y:
        return { 0.0, 1.0, 0.0 };
    case Axis::Z:
        return { 0.0, 0.0, 1.0 };
    case Axis::None:
        break;
    }
    return { 0.0, 0.0, 0.0 };
}

std::string_view label(const Inference& inference)
{
    switch (inference.kind) {
    case Kind::None:
    case Kind::Ground:
        return {};
    case Kind::OnFace:
        return "On Face";
    case Kind::OnEdge:
        return "On Edge";
    case Kind::Endpoint:
        return "Endpoint";
    case Kind::Midpoint:
        return "Midpoint";
    case Kind::Intersection:
        return "Intersection";
    case Kind::OnAxis:
        switch (inference.axis) {
        case Axis::X:
            return "On Red Axis";
        case Axis::Y:
            return "On Green Axis";
        case Axis::Z:
            return "On Blue Axis";
        case Axis::None:
            break;
        }
        return "Locked";
    case Kind::Parallel:
        return "Parallel to Edge";
    case Kind::Perpendicular:
        return "Perpendicular to Edge";
    case Kind::FromPoint:
        return "From Point";
    }
    return {};
}

Inference Engine::infer(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen, std::optional<Vec3d> from,
    std::optional<math::Plane> plane)
{
    if (m_referenceEdge && !mesh.edges().contains(*m_referenceEdge)) {
        m_referenceEdge.reset();
        m_referenceDirection.reset();
    }
    m_lastFrom = from;
    Inference result = locked() ? inferLocked(mesh, camera, screen, from) : inferFree(mesh, camera, screen, from, plane);
    remember(mesh, result);
    m_last = result;
    return result;
}

Inference Engine::inferFree(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen,
    std::optional<Vec3d> from, const std::optional<math::Plane>& plane)
{
    const math::Ray ray = camera.rayAt(screen);
    const view::PickResult facePick = view::pick(mesh, camera, screen, { -1.0, -1.0 });
    const double hyst = m_options.hysteresis;

    Inference out;
    out.pick = facePick;

    // 1. Capete (vârfuri). Vârful ales anterior rămâne ales într-o rază mai mare.
    {
        double best = m_options.pointRadiusPx;
        geom::VertexId bestVertex;
        if (m_last.kind == Kind::Endpoint && mesh.vertices().contains(m_last.vertex)) {
            const Vec3d& p = mesh.position(m_last.vertex);
            const double d = screenDistance(camera, p, screen);
            if (d <= best * hyst && visible(ray, facePick, p)) {
                bestVertex = m_last.vertex;
                best = -1.0;
            }
        }
        for (std::size_t i = 0; best >= 0.0 && i < mesh.vertices().size(); ++i) {
            const Vec3d& p = mesh.vertices().values()[i].position;
            const double d = screenDistance(camera, p, screen);
            if (d <= best && visible(ray, facePick, p)) {
                best = d;
                bestVertex = mesh.vertices().keys()[i];
            }
        }
        if (bestVertex) {
            out.kind = Kind::Endpoint;
            out.vertex = bestVertex;
            out.point = mesh.position(bestVertex);
            return out;
        }
    }

    // 2. Mijloacele muchiilor.
    {
        double best = kInf;
        geom::EdgeId bestEdge;
        for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
            const geom::EdgeId id = mesh.edges().keys()[i];
            const geom::Edge& e = mesh.edges().values()[i];
            const Vec3d mid = (mesh.position(e.v0) + mesh.position(e.v1)) * 0.5;
            const bool previous = m_last.kind == Kind::Midpoint && m_last.edge == id;
            const double d = screenDistance(camera, mid, screen);
            if (d > m_options.pointRadiusPx * (previous ? hyst : 1.0) || !visible(ray, facePick, mid)) {
                continue;
            }
            const double score = previous ? -1.0 : d; // mijlocul ales anterior are prioritate
            if (score < best) {
                best = score;
                bestEdge = id;
            }
        }
        if (bestEdge) {
            const geom::Edge& e = mesh.edge(bestEdge);
            out.kind = Kind::Midpoint;
            out.edge = bestEdge;
            out.point = (mesh.position(e.v0) + mesh.position(e.v1)) * 0.5;
            return out;
        }
    }

    // 3. Direcții din punctul de pornire: axele, paralel și perpendicular pe muchia de referință.
    std::optional<Linear> linear;
    if (from) {
        std::vector<Direction> directions {
            { axisDirection(Axis::X), Kind::OnAxis, Axis::X },
            { axisDirection(Axis::Y), Kind::OnAxis, Axis::Y },
            { axisDirection(Axis::Z), Kind::OnAxis, Axis::Z },
        };
        if (m_referenceEdge && m_referenceDirection && !isAxisAligned(*m_referenceDirection)) {
            const Vec3d d = *m_referenceDirection;
            directions.push_back({ d, Kind::Parallel });
            const geom::Edge& e = mesh.edge(*m_referenceEdge);
            Vec3d normal { 0.0, 0.0, 1.0 };
            if (!e.faces.empty()) {
                normal = mesh.face(e.faces.front()).plane.normal;
            }
            const Vec3d perp = glm::cross(normal, d);
            if (glm::length(perp) > 1e-6 && !isAxisAligned(glm::normalize(perp))) {
                directions.push_back({ glm::normalize(perp), Kind::Perpendicular });
            }
        }
        for (const Direction& dir : directions) {
            if (std::abs(glm::dot(dir.dir, ray.direction)) > kMaxViewAlignment) {
                continue;
            }
            const auto p = pointOnLineUnderCursor(camera, screen, *from, dir.dir);
            if (!p) {
                continue;
            }
            const bool previous = m_last.direction && m_last.directionKind == dir.kind && m_last.axis == dir.axis
                && tol::parallel(*m_last.direction, dir.dir, 1e-6);
            double d = screenDistance(camera, *p, screen);
            if (d > m_options.lineRadiusPx * (previous ? hyst : 1.0)) {
                continue;
            }
            if (previous) {
                d -= m_options.lineRadiusPx; // păstrăm direcția aleasă cât timp e în raza mărită
            }
            if (dir.kind == Kind::OnAxis) {
                d -= kAxisBonusPx;
            }
            if (!linear || d < linear->distance) {
                linear = Linear { dir, *p, d };
            }
        }
    }

    const auto withDirection = [&](Inference& inf, const Linear& l) {
        inf.direction = l.direction.dir;
        inf.directionKind = l.direction.kind;
        inf.axis = l.direction.axis;
        inf.guides.push_back({ *from, inf.point, directionTint(l.direction.kind, l.direction.axis), false });
        if (l.direction.kind != Kind::OnAxis && m_referenceEdge) {
            const geom::Edge& e = mesh.edge(*m_referenceEdge);
            inf.guides.push_back({ mesh.position(e.v0), mesh.position(e.v1), Tint::Magenta, false });
        }
    };

    if (linear) {
        const Vec3d dir = linear->direction.dir;
        const double eps = std::max(camera.worldPerPixelAtTarget(), tol::kLength);

        // 4a. Direcția taie o muchie lângă cursor.
        double bestPx = m_options.pointRadiusPx;
        std::optional<std::pair<Vec3d, geom::EdgeId>> crossing;
        for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
            const geom::Edge& e = mesh.edges().values()[i];
            const Vec3d a = mesh.position(e.v0);
            const Vec3d b = mesh.position(e.v1);
            const auto x = intersectLines(*from, dir, a, glm::normalize(b - a), eps);
            if (!x || math::projectOnSegment(*x, { a, b }).distance > eps || tol::samePoint(*x, *from)) {
                continue;
            }
            const double d = screenDistance(camera, *x, screen);
            if (d <= bestPx && visible(ray, facePick, *x)) {
                bestPx = d;
                crossing = std::pair { *x, mesh.edges().keys()[i] };
            }
        }
        if (crossing) {
            out.kind = Kind::Intersection;
            out.point = crossing->first;
            out.edge = crossing->second;
            withDirection(out, *linear);
            return out;
        }

        // 4b. Direcția întâlnește linia de aliniere a unui punct de referință.
        double bestTrack = m_options.lineRadiusPx;
        std::optional<std::pair<Vec3d, Guide>> track;
        for (const Vec3d& r : m_references) {
            if (tol::samePoint(r, *from)) {
                continue;
            }
            for (Axis a : { Axis::X, Axis::Y, Axis::Z }) {
                const Vec3d ad = axisDirection(a);
                if (tol::parallel(ad, dir, 1e-6)) {
                    continue;
                }
                const auto x = intersectLines(*from, dir, r, ad, eps);
                if (!x || tol::samePoint(*x, r)) {
                    continue;
                }
                const double d = screenDistance(camera, *x, screen);
                if (d <= bestTrack) {
                    bestTrack = d;
                    track = std::pair { *x, Guide { r, *x, axisTint(a), true } };
                }
            }
        }
        if (track) {
            out.kind = Kind::FromPoint;
            out.point = track->first;
            withDirection(out, *linear);
            out.guides.push_back(track->second);
            return out;
        }
    }

    // 5. Pe o muchie. Câștigă în fața unei direcții doar dacă e mai aproape de cursor.
    const view::PickResult edgePick = view::pick(mesh, camera, screen, { -1.0, m_options.edgeRadiusPx });
    if (edgePick.kind == view::PickResult::Kind::Edge) {
        const double d = screenDistance(camera, edgePick.point, screen);
        if (!linear || d < linear->distance) {
            out.kind = Kind::OnEdge;
            out.edge = edgePick.edge;
            out.point = edgePick.point;
            return out;
        }
    }

    if (linear) {
        out.kind = linear->direction.kind;
        out.point = linear->point;
        withDirection(out, *linear);
        return out;
    }

    // Punctul „liber”: pe fața de sub cursor sau pe planul de lucru.
    std::optional<Vec3d> free;
    math::Plane freePlane;
    if (facePick.kind == view::PickResult::Kind::Face) {
        free = facePick.point;
        freePlane = math::Plane::fromPointNormal(facePick.point, mesh.face(facePick.face).plane.normal);
    } else {
        freePlane = plane.value_or(
            from ? math::Plane::fromPointNormal(*from, { 0.0, 0.0, 1.0 }) : math::Plane::fromPointNormal({}, { 0, 0, 1 }));
        if (const auto t = math::intersectRayPlane(ray, freePlane)) {
            free = ray.at(*t);
        } else if (from) {
            // Planul e în spatele camerei sau paralel cu raza: folosim planul prin `from` orientat spre cameră.
            freePlane = math::Plane::fromPointNormal(*from, -ray.direction);
            if (const auto t2 = math::intersectRayPlane(ray, freePlane)) {
                free = ray.at(*t2);
            }
        }
    }

    // 6. Aliniat cu un punct de referință pe o axă din planul punctului liber.
    if (free) {
        double best = m_options.lineRadiusPx;
        std::optional<std::pair<Vec3d, Guide>> track;
        for (const Vec3d& r : m_references) {
            for (Axis a : { Axis::X, Axis::Y, Axis::Z }) {
                const Vec3d ad = axisDirection(a);
                if (!tol::perpendicular(ad, freePlane.normal, 1e-6) || !tol::isZero(freePlane.signedDistance(r), 0.01)) {
                    continue;
                }
                if (std::abs(glm::dot(ad, ray.direction)) > kMaxViewAlignment) {
                    continue;
                }
                const auto p = pointOnLineUnderCursor(camera, screen, r, ad);
                if (!p || tol::samePoint(*p, r)) {
                    continue;
                }
                const double d = screenDistance(camera, *p, screen);
                if (d <= best) {
                    best = d;
                    track = std::pair { *p, Guide { r, *p, axisTint(a), true } };
                }
            }
        }
        if (track) {
            out.kind = Kind::FromPoint;
            out.point = track->first;
            out.guides.push_back(track->second);
            return out;
        }
    }

    if (facePick.kind == view::PickResult::Kind::Face) {
        out.kind = Kind::OnFace;
        out.face = facePick.face;
        out.point = facePick.point;
        return out;
    }
    if (free) {
        out.kind = Kind::Ground;
        out.point = *free;
    }
    return out;
}

Inference Engine::inferLocked(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen,
    std::optional<Vec3d> from)
{
    const math::Ray ray = camera.rayAt(screen);
    // Un punct atins (capăt, mijloc) se proiectează pe constrângere.
    const view::PickResult snap = view::pick(mesh, camera, screen, { m_options.pointRadiusPx, -1.0 });
    std::optional<Vec3d> target;
    if (snap.kind == view::PickResult::Kind::Vertex) {
        target = snap.point;
    }

    Inference out;
    out.pick = snap;
    out.locked = true;
    out.kind = m_lock.reason;
    if (m_lock.kind == LockKind::Line) {
        const Vec3d origin = from.value_or(m_lock.origin);
        const Vec3d dir = m_lock.direction;
        std::optional<Vec3d> p;
        if (target) {
            p = origin + dir * glm::dot(*target - origin, dir);
        } else {
            p = pointOnLineUnderCursor(camera, screen, origin, dir);
        }
        if (!p) {
            Inference keep = m_last;
            keep.locked = true;
            return keep;
        }
        out.point = *p;
        out.direction = dir;
        out.directionKind = m_lock.reason;
        out.axis = m_lock.axis;
        out.guides.push_back({ origin, *p, directionTint(m_lock.reason, m_lock.axis), false });
        if (target && !tol::samePoint(*target, *p)) {
            out.guides.push_back({ *target, *p, directionTint(m_lock.reason, m_lock.axis), true });
        }
        return out;
    }

    // Lock pe plan.
    const math::Plane& plane = m_lock.plane;
    if (target) {
        out.point = plane.project(*target);
        if (!tol::samePoint(*target, out.point)) {
            out.guides.push_back({ *target, out.point, Tint::Neutral, true });
        }
        return out;
    }
    if (const auto t = math::intersectRayPlane(ray, plane)) {
        out.point = ray.at(*t);
        return out;
    }
    if (const auto t = math::intersectRayPlane(ray, plane.flipped())) {
        out.point = ray.at(*t);
        return out;
    }
    Inference keep = m_last;
    keep.locked = true;
    return keep;
}

void Engine::remember(const geom::Mesh& mesh, const Inference& result)
{
    std::optional<Vec3d> reference;
    if (result.kind == Kind::Endpoint || result.kind == Kind::Midpoint) {
        reference = result.point;
    }
    if (reference) {
        const auto same = [&](const Vec3d& r) { return tol::samePoint(r, *reference); };
        m_references.erase(std::remove_if(m_references.begin(), m_references.end(), same), m_references.end());
        m_references.push_front(*reference);
        while (m_references.size() > m_options.maxReferences) {
            m_references.pop_back();
        }
    }
    m_lastFacePlane.reset();
    if (result.kind == Kind::OnFace && result.face) {
        m_lastFacePlane = math::Plane::fromPointNormal(result.point, mesh.face(result.face).plane.normal);
    }
    if ((result.kind == Kind::OnEdge || result.kind == Kind::Midpoint) && result.edge) {
        m_referenceEdge = result.edge;
        m_referenceDirection = edgeDirection(mesh, result.edge);
    }
}

void Engine::lockCurrent()
{
    if (locked()) {
        return;
    }
    if (m_last.direction && m_lastFrom) {
        m_lock = { LockKind::Line, *m_lastFrom, *m_last.direction, {}, m_last.directionKind, m_last.axis };
        return;
    }
    if (m_lastFrom && m_last.valid() && m_last.kind != Kind::Ground && m_last.kind != Kind::OnFace
        && !tol::samePoint(m_last.point, *m_lastFrom)) {
        const Vec3d dir = glm::normalize(m_last.point - *m_lastFrom);
        m_lock = { LockKind::Line, *m_lastFrom, dir, {}, Kind::OnAxis, Axis::None };
        return;
    }
    if (m_last.kind == Kind::OnFace && m_lastFacePlane) {
        m_lock.kind = LockKind::Plane;
        m_lock.plane = *m_lastFacePlane;
        m_lock.reason = Kind::OnFace;
    }
}

void Engine::toggleAxisLock(Axis axis)
{
    if (m_lock.kind == LockKind::Line && m_lock.reason == Kind::OnAxis && m_lock.axis == axis) {
        unlock();
        return;
    }
    if (!m_lastFrom) {
        return;
    }
    m_lock = { LockKind::Line, *m_lastFrom, axisDirection(axis), {}, Kind::OnAxis, axis };
}

void Engine::toggleReferenceLock()
{
    if (m_lock.kind == LockKind::Line && m_lock.reason == Kind::Parallel) {
        unlock();
        return;
    }
    if (!m_lastFrom || !m_referenceDirection) {
        return;
    }
    m_lock = { LockKind::Line, *m_lastFrom, *m_referenceDirection, {}, Kind::Parallel, Axis::None };
}

void Engine::unlock()
{
    m_lock = {};
}

void Engine::reset()
{
    resetLock();
    m_references.clear();
    m_referenceEdge.reset();
    m_referenceDirection.reset();
}

void Engine::resetLock()
{
    m_lock = {};
    m_last = {};
}

} // namespace aion::inference
