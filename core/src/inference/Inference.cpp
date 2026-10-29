#include "aion/core/inference/Inference.h"

#include "aion/core/geom/Mesh.h"
#include "aion/core/inference/SnapScene.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace aion::inference {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
// La aceeași prioritate, un candidat dintr-un obiect bate unul din ctx.mesh doar dacă e mai aproape cu atât (px).
constexpr double kTiePx = 0.5;

} // namespace

namespace detail {

// O geometrie pe care o vede inferența: mesh-ul contextului (`toContext` nul) sau o sursă din scenă. Căutările se fac
// în coordonatele ei (cu camera mutată acolo), iar rezultatele trec în coordonatele contextului.
struct Layer {
    const geom::Mesh* mesh = nullptr;
    view::Camera camera;
    const math::Transform* toContext = nullptr;
    model::InstanceId instance;

    [[nodiscard]] bool context() const { return toContext == nullptr; }
    [[nodiscard]] Vec3d point(const Vec3d& local) const { return toContext ? toContext->applyToPoint(local) : local; }
    // Inversa transpusă: corectă și la scalări neuniforme sau oglindiri.
    [[nodiscard]] Vec3d normal(const Vec3d& local) const { return toContext ? toContext->applyToNormal(local) : local; }
    [[nodiscard]] double tiePenalty() const { return context() ? 0.0 : kTiePx; }
    // Un rezultat dintr-un obiect spune de unde vine (ID-urile lui rămân goale).
    void stamp(Inference& out) const
    {
        if (!context()) {
            out.origin = Origin::Object;
            out.sourceInstance = instance;
        }
    }
};

} // namespace detail

namespace {
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

// Fața cea mai apropiată de sub cursor, din toate straturile, în coordonatele contextului.
struct FaceHit {
    const detail::Layer* layer = nullptr;
    geom::FaceId face; // din mesh-ul stratului
    Vec3d point { 0.0 };
    Vec3d normal { 0.0 };

    [[nodiscard]] bool hit() const { return layer != nullptr; }
};

// Adâncimea se măsoară pe raza contextului; la egalitate rămâne ctx.mesh (primul strat). `contextPick` primește
// fața atinsă în ctx.mesh (pentru `Inference::pick`).
FaceHit nearestFace(
    const std::vector<detail::Layer>& layers, const math::Ray& ray, Vec2d screen, view::PickResult& contextPick)
{
    FaceHit best;
    double bestDepth = kInf;
    for (const detail::Layer& l : layers) {
        const view::PickResult p = view::pick(*l.mesh, l.camera, screen, { -1.0, -1.0 });
        if (l.context()) {
            contextPick = p;
        }
        if (p.kind != view::PickResult::Kind::Face) {
            continue;
        }
        const Vec3d point = l.point(p.point);
        const double depth = glm::dot(point - ray.origin, ray.direction);
        if (depth < bestDepth) {
            bestDepth = depth;
            best = { &l, p.face, point, l.normal(l.mesh->face(p.face).plane.normal) };
        }
    }
    return best;
}

// Punctul e vizibil dacă nu există o față între el și cameră (aceeași regulă ca la picking).
bool visible(const math::Ray& ray, const FaceHit& faceHit, const Vec3d& p)
{
    if (!faceHit.hit()) {
        return true;
    }
    const double faceDepth = glm::dot(faceHit.point - ray.origin, ray.direction);
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
        return {};
    case Kind::Ground:
        return inference.onGrid ? "On Grid" : std::string_view {};
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
    case Kind::GuidePoint:
        return "Guide Point";
    case Kind::OnGuide:
        return "On Guide";
    }
    return {};
}

double autoGridStep(double cameraDistance)
{
    const double level = std::log10(std::max(cameraDistance, 1.0) * 0.02);
    const double base = std::pow(10.0, std::floor(level));
    // Liniile pasului mic se estompează pe măsură ce camera se depărtează (grid.frag); de la jumătate,
    // pasul vizibil e cel mare.
    return level - std::floor(level) < 0.5 ? base : base * 10.0;
}

namespace {

double roundTo(double value, double step)
{
    return std::round(value / step) * step;
}

// Rotunjește coordonatele punctului care stau în planul dat (planul trebuie să fie perpendicular pe o axă).
std::optional<Vec3d> snapInPlane(const Vec3d& point, const Vec3d& normal, double step)
{
    int axis = -1;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(std::abs(normal[i]) - 1.0) < 1e-9) {
            axis = i;
        }
    }
    if (axis < 0) {
        return std::nullopt; // față înclinată: nicio rotunjire nu rămâne în plan
    }
    Vec3d snapped = point;
    for (int i = 0; i < 3; ++i) {
        if (i != axis) {
            snapped[i] = roundTo(point[i], step);
        }
    }
    return snapped;
}

} // namespace

void Engine::applyGrid(Inference& result, const std::optional<Vec3d>& from) const
{
    if (m_gridStep <= 0.0) {
        return;
    }
    // O distanță de-a lungul unei direcții (axă, paralel, perpendicular, lock pe o linie): multiplu de pas.
    if (from && result.direction && (result.kind == Kind::OnAxis || result.kind == Kind::Parallel
                                        || result.kind == Kind::Perpendicular || result.kind == Kind::Ground)) {
        const double distance = glm::dot(result.point - *from, *result.direction);
        result.point = *from + *result.direction * roundTo(distance, m_gridStep);
        result.onGrid = true;
        return;
    }
    std::optional<Vec3d> normal;
    if (result.kind == Kind::Ground) {
        normal = result.planeNormal;
    } else if (result.kind == Kind::OnFace) {
        normal = result.normal;
    }
    if (normal) {
        if (const auto snapped = snapInPlane(result.point, *normal, m_gridStep)) {
            result.point = *snapped;
            result.onGrid = true;
        }
    }
}

Inference Engine::infer(const geom::Mesh& mesh, const view::Camera& camera, Vec2d screen, std::optional<Vec3d> from,
    std::optional<math::Plane> plane)
{
    if (m_referenceEdge && !mesh.edges().contains(*m_referenceEdge)) {
        m_referenceEdge.reset();
        m_referenceDirection.reset();
    }
    std::vector<detail::Layer> layers { { &mesh, camera, nullptr, {} } };
    if (m_scene) {
        for (const SnapSource& source : m_scene->sources) {
            if (source.mesh) {
                layers.push_back({ source.mesh, camera.inContext(source.toContext), &source.toContext, source.instance });
            }
        }
    }
    m_lastFrom = from;
    Inference result
        = locked() ? inferLocked(layers, camera, screen, from) : inferFree(layers, camera, screen, from, plane);
    applyGrid(result, from);
    remember(mesh, result);
    m_last = result;
    return result;
}

Inference Engine::inferFree(const std::vector<detail::Layer>& layers, const view::Camera& camera, Vec2d screen,
    std::optional<Vec3d> from, const std::optional<math::Plane>& plane)
{
    const geom::Mesh& mesh = *layers.front().mesh;
    const math::Ray ray = camera.rayAt(screen);
    Inference out;
    const FaceHit faceHit = nearestFace(layers, ray, screen, out.pick);
    const double hyst = m_options.hysteresis;

    // 1. Capete (vârfuri). Vârful ales anterior rămâne ales într-o rază mai mare (în ctx.mesh după ID, într-un obiect
    // după punct).
    {
        double best = kInf;
        const detail::Layer* bestLayer = nullptr;
        geom::VertexId bestVertex;
        Vec3d bestPoint { 0.0 };
        for (const detail::Layer& l : layers) {
            view::forEachVertexNear(*l.mesh, l.camera, screen, m_options.pointRadiusPx * hyst, [&](geom::VertexId v) {
                const Vec3d p = l.point(l.mesh->position(v));
                const bool previous = m_last.kind == Kind::Endpoint
                    && (l.context() ? m_last.origin == Origin::Context && m_last.vertex == v
                                    : m_last.origin == Origin::Object && tol::samePoint(m_last.point, p));
                const double d = screenDistance(camera, p, screen);
                if (d > m_options.pointRadiusPx * (previous ? hyst : 1.0) || !visible(ray, faceHit, p)
                    || (!previous && view::isHidden(*l.mesh, v))) {
                    return;
                }
                const double score = previous ? -1.0 : d + l.tiePenalty();
                if (score <= best) {
                    best = score;
                    bestLayer = &l;
                    bestVertex = v;
                    bestPoint = p;
                }
            });
        }
        if (bestLayer) {
            out.kind = Kind::Endpoint;
            out.point = bestPoint;
            if (bestLayer->context()) {
                out.vertex = bestVertex;
            }
            bestLayer->stamp(out);
            return out;
        }
    }

    // 1b. Punctele de construcție și intersecțiile dintre două linii de construcție.
    {
        double best = m_options.pointRadiusPx;
        std::optional<Vec3d> hit;
        for (const GuideRef& g : m_guides) {
            if (g.direction == Vec3d { 0.0 }) {
                const double d = screenDistance(camera, g.point, screen);
                if (d <= best) {
                    best = d;
                    hit = g.point;
                }
            }
        }
        const double eps = std::max(camera.worldPerPixelAtTarget(), tol::kLength);
        for (std::size_t i = 0; i < m_guides.size(); ++i) {
            for (std::size_t j = i + 1; j < m_guides.size(); ++j) {
                const GuideRef& a = m_guides[i];
                const GuideRef& b = m_guides[j];
                if (a.direction == Vec3d { 0.0 } || b.direction == Vec3d { 0.0 }) {
                    continue;
                }
                if (const auto x = intersectLines(a.point, glm::normalize(a.direction), b.point, glm::normalize(b.direction), eps)) {
                    const double d = screenDistance(camera, *x, screen);
                    if (d <= best) {
                        best = d;
                        hit = *x;
                    }
                }
            }
        }
        if (hit) {
            out.kind = Kind::GuidePoint;
            out.point = *hit;
            return out;
        }
    }

    // 2. Mijloacele muchiilor.
    {
        double best = kInf;
        const detail::Layer* bestLayer = nullptr;
        geom::EdgeId bestEdge;
        Vec3d bestPoint { 0.0 };
        for (const detail::Layer& l : layers) {
            view::forEachEdgeNear(*l.mesh, l.camera, screen, m_options.pointRadiusPx * hyst, [&](geom::EdgeId id) {
                const geom::Edge& e = l.mesh->edge(id);
                if (view::isHidden(e)) {
                    return;
                }
                const Vec3d mid = l.point((l.mesh->position(e.v0) + l.mesh->position(e.v1)) * 0.5);
                const bool previous = m_last.kind == Kind::Midpoint
                    && (l.context() ? m_last.origin == Origin::Context && m_last.edge == id
                                    : m_last.origin == Origin::Object && tol::samePoint(m_last.point, mid));
                const double d = screenDistance(camera, mid, screen);
                if (d > m_options.pointRadiusPx * (previous ? hyst : 1.0) || !visible(ray, faceHit, mid)) {
                    return;
                }
                const double score = previous ? -1.0 : d + l.tiePenalty(); // mijlocul ales anterior are prioritate
                if (score < best) {
                    best = score;
                    bestLayer = &l;
                    bestEdge = id;
                    bestPoint = mid;
                }
            });
        }
        if (bestLayer) {
            out.kind = Kind::Midpoint;
            out.point = bestPoint;
            if (bestLayer->context()) {
                out.edge = bestEdge;
            }
            bestLayer->stamp(out);
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
        double best = kInf;
        const detail::Layer* bestLayer = nullptr;
        geom::EdgeId bestEdge;
        Vec3d bestPoint { 0.0 };
        for (const detail::Layer& l : layers) {
            view::forEachEdgeNear(*l.mesh, l.camera, screen, m_options.pointRadiusPx, [&](geom::EdgeId id) {
                const geom::Edge& e = l.mesh->edge(id);
                if (view::isHidden(e)) {
                    return;
                }
                const Vec3d a = l.point(l.mesh->position(e.v0));
                const Vec3d b = l.point(l.mesh->position(e.v1));
                const auto x = intersectLines(*from, dir, a, glm::normalize(b - a), eps);
                if (!x || math::projectOnSegment(*x, { a, b }).distance > eps || tol::samePoint(*x, *from)) {
                    return;
                }
                const double d = screenDistance(camera, *x, screen);
                const double score = d + l.tiePenalty();
                if (d <= m_options.pointRadiusPx && score <= best && visible(ray, faceHit, *x)) {
                    best = score;
                    bestLayer = &l;
                    bestEdge = id;
                    bestPoint = *x;
                }
            });
        }
        if (bestLayer) {
            out.kind = Kind::Intersection;
            out.point = bestPoint;
            if (bestLayer->context()) {
                out.edge = bestEdge;
            }
            bestLayer->stamp(out);
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

    // 4c. O direcție care taie o linie de construcție.
    if (linear) {
        const double eps = std::max(camera.worldPerPixelAtTarget(), tol::kLength);
        for (const GuideRef& g : m_guides) {
            if (g.direction == Vec3d { 0.0 }) {
                continue;
            }
            if (const auto x = intersectLines(*from, linear->direction.dir, g.point, glm::normalize(g.direction), eps)) {
                if (screenDistance(camera, *x, screen) <= m_options.pointRadiusPx && !tol::samePoint(*x, *from)) {
                    out.kind = Kind::Intersection;
                    out.point = *x;
                    out.direction = linear->direction.dir;
                    out.directionKind = linear->direction.kind;
                    out.axis = linear->direction.axis;
                    out.guides.push_back({ *from, *x, directionTint(linear->direction.kind, linear->direction.axis), false });
                    return out;
                }
            }
        }
    }

    // 5. Pe o muchie. Câștigă în fața unei direcții doar dacă e mai aproape de cursor.
    {
        double best = kInf;
        double bestDistance = kInf;
        const detail::Layer* bestLayer = nullptr;
        view::PickResult bestPick;
        Vec3d bestPoint { 0.0 };
        for (const detail::Layer& l : layers) {
            const view::PickResult edgePick = view::pick(*l.mesh, l.camera, screen, { -1.0, m_options.edgeRadiusPx });
            if (edgePick.kind != view::PickResult::Kind::Edge) {
                continue;
            }
            const Vec3d p = l.point(edgePick.point);
            const double d = screenDistance(camera, p, screen);
            if (d + l.tiePenalty() < best && visible(ray, faceHit, p)) {
                best = d + l.tiePenalty();
                bestDistance = d;
                bestLayer = &l;
                bestPick = edgePick;
                bestPoint = p;
            }
        }
        if (bestLayer && (!linear || bestDistance < linear->distance)) {
            out.kind = Kind::OnEdge;
            out.point = bestPoint;
            if (bestLayer->context()) {
                out.edge = bestPick.edge;
            }
            bestLayer->stamp(out);
            return out;
        }
    }

    if (linear) {
        out.kind = linear->direction.kind;
        out.point = linear->point;
        withDirection(out, *linear);
        return out;
    }

    // 5b. Pe o linie de construcție.
    {
        double best = m_options.edgeRadiusPx;
        std::optional<Vec3d> hit;
        for (const GuideRef& g : m_guides) {
            if (g.direction == Vec3d { 0.0 }) {
                continue;
            }
            if (const auto p = pointOnLineUnderCursor(camera, screen, g.point, glm::normalize(g.direction))) {
                const double d = screenDistance(camera, *p, screen);
                if (d <= best && visible(ray, faceHit, *p)) {
                    best = d;
                    hit = p;
                }
            }
        }
        if (hit) {
            out.kind = Kind::OnGuide;
            out.point = *hit;
            return out;
        }
    }

    // Punctul „liber”: pe fața de sub cursor (din orice strat) sau pe planul de lucru.
    std::optional<Vec3d> free;
    math::Plane freePlane;
    if (faceHit.hit()) {
        free = faceHit.point;
        freePlane = math::Plane::fromPointNormal(faceHit.point, faceHit.normal);
        out.normal = faceHit.normal;
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

    if (faceHit.hit()) {
        out.kind = Kind::OnFace;
        out.point = faceHit.point;
        if (faceHit.layer->context()) {
            out.face = faceHit.face;
        }
        faceHit.layer->stamp(out);
        return out;
    }
    if (free) {
        out.kind = Kind::Ground;
        out.point = *free;
        out.planeNormal = freePlane.normal;
    }
    return out;
}

Inference Engine::inferLocked(const std::vector<detail::Layer>& layers, const view::Camera& camera, Vec2d screen,
    std::optional<Vec3d> from)
{
    const math::Ray ray = camera.rayAt(screen);
    Inference out;
    const FaceHit faceHit = nearestFace(layers, ray, screen, out.pick);
    // Un capăt atins (din orice strat) se proiectează pe constrângere.
    std::optional<Vec3d> target;
    double best = kInf;
    for (const detail::Layer& l : layers) {
        const view::PickResult snap = view::pick(*l.mesh, l.camera, screen, { m_options.pointRadiusPx, -1.0 });
        if (l.context()) {
            out.pick = snap;
        }
        if (snap.kind != view::PickResult::Kind::Vertex) {
            continue;
        }
        const Vec3d p = l.point(snap.point);
        const double score = screenDistance(camera, p, screen) + l.tiePenalty();
        if (score < best && visible(ray, faceHit, p)) {
            best = score;
            target = p;
        }
    }

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
    if (result.kind == Kind::OnFace && result.normal) {
        m_lastFacePlane = math::Plane::fromPointNormal(result.point, *result.normal);
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
    // Fără punct de pornire (de exemplu imediat după primul click al lui Move), axa trece prin ultimul punct inferat.
    const std::optional<Vec3d> origin = m_lastFrom ? m_lastFrom : (m_last.valid() ? std::optional(m_last.point) : std::nullopt);
    if (!origin) {
        return;
    }
    m_lock = { LockKind::Line, *origin, axisDirection(axis), {}, Kind::OnAxis, axis };
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

void Engine::restartFrom(const Vec3d& from)
{
    resetLock();
    m_lastFrom = from;
}

void Engine::resetLock()
{
    m_lock = {};
    m_last = {};
}

} // namespace aion::inference
