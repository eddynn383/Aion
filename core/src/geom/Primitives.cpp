#include "aion/core/geom/Primitives.h"

#include "aion/core/geom/Operations.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace aion::geom {

namespace {

using math::Vec3d;

std::vector<Vec3d> ring(double radius, double z, int segments)
{
    std::vector<Vec3d> out;
    for (int k = 0; k < segments; ++k) {
        const double a = 2.0 * std::numbers::pi * k / segments;
        out.push_back({ radius * std::cos(a), radius * std::sin(a), z });
    }
    return out;
}

// Fața de jos a unui contur desenat pe sol (normala în jos), extrudată în sus.
void extrudeUp(Mesh& mesh, double height)
{
    if (mesh.faces().empty()) {
        return;
    }
    pushPull(mesh, mesh.faces().keys().front(), -height);
}

// Primitivele sunt convexe: fiecare față trebuie să privească dinspre centru spre exterior.
void orientOutward(Mesh& mesh, const Vec3d& center)
{
    const std::vector<FaceId> faces(mesh.faces().keys().begin(), mesh.faces().keys().end());
    for (FaceId f : faces) {
        if (!mesh.faces().contains(f)) {
            continue;
        }
        const Vec3d p = faceInteriorPoint(mesh, f);
        const Vec3d n = mesh.face(f).plane.normal;
        if (glm::dot(p - center, n) < 0.0) {
            orientFace(mesh, f, -n);
        }
    }
}

} // namespace

void softenEdges(Mesh& mesh, double maxAngleDegrees)
{
    const double cosLimit = std::cos(glm::radians(maxAngleDegrees));
    std::vector<EdgeId> soft;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const Edge& e = mesh.edges().values()[i];
        if (e.faces.size() != 2) {
            continue;
        }
        const Vec3d a = mesh.face(e.faces[0]).plane.normal;
        const Vec3d b = mesh.face(e.faces[1]).plane.normal;
        if (glm::dot(a, b) >= cosLimit) {
            soft.push_back(mesh.edges().keys()[i]);
        }
    }
    for (EdgeId id : soft) {
        const Edge& e = mesh.edge(id);
        mesh.setEdgeStyle(id, true, e.curve, true);
    }
}

void buildPrimitive(Mesh& mesh, Primitive kind, const math::Vec3d& size, int segments)
{
    segments = std::clamp(segments, 3, 128);
    const double w = std::max(size.x, 1e-3);
    const double d = std::max(size.y, 1e-3);
    const double h = std::max(size.z, 1e-3);
    switch (kind) {
    case Primitive::Box:
        drawPolygon(mesh, { { -w / 2, -d / 2, 0 }, { w / 2, -d / 2, 0 }, { w / 2, d / 2, 0 }, { -w / 2, d / 2, 0 } });
        extrudeUp(mesh, h);
        return;
    case Primitive::Cylinder:
        drawCurve(mesh, ring(w / 2, 0.0, segments), true, true);
        extrudeUp(mesh, h);
        return;
    case Primitive::Cone: {
        // MeshBuilder pune fețele exact cum le dăm (desenul ar umple automat contururile plane închise).
        const std::vector<Vec3d> base = ring(w / 2, 0.0, segments);
        const Vec3d apex { 0.0, 0.0, h };
        MeshBuilder builder(mesh);
        builder.face({ base.rbegin(), base.rend() }); // baza privește în jos
        for (int k = 0; k < segments; ++k) {
            builder.face({ base[static_cast<std::size_t>(k)], base[static_cast<std::size_t>((k + 1) % segments)], apex });
        }
        builder.finish(glm::radians(360.0 / segments + 1.0));
        orientOutward(mesh, { 0.0, 0.0, h / 3.0 });
        return;
    }
    case Primitive::Sphere: {
        // Sferă UV pe sol (centrul la înălțimea razei): inele de latitudine, fâșii de longitudine.
        const double r = w / 2;
        const int rings = std::max(segments / 2, 2);
        const Vec3d top { 0.0, 0.0, 2 * r };
        const Vec3d bottom { 0.0, 0.0, 0.0 };
        std::vector<std::vector<Vec3d>> lat;
        for (int i = 1; i < rings; ++i) {
            const double phi = std::numbers::pi * i / rings; // de la pol nord
            lat.push_back(ring(r * std::sin(phi), r + r * std::cos(phi), segments));
        }
        MeshBuilder builder(mesh);
        for (int k = 0; k < segments; ++k) {
            const auto a = static_cast<std::size_t>(k);
            const auto b = static_cast<std::size_t>((k + 1) % segments);
            builder.face({ top, lat.front()[a], lat.front()[b] });
            for (std::size_t j = 0; j + 1 < lat.size(); ++j) {
                builder.face({ lat[j][a], lat[j + 1][a], lat[j + 1][b], lat[j][b] });
            }
            builder.face({ lat.back()[a], bottom, lat.back()[b] });
        }
        builder.finish(glm::radians(360.0 / segments + 1.0));
        orientOutward(mesh, { 0.0, 0.0, r });
        return;
    }
    }
}

} // namespace aion::geom
