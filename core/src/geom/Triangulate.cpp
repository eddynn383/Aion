#include "forma/core/geom/Triangulate.h"

#include "forma/core/geom/Mesh.h"

#include <mapbox/earcut.hpp>

#include <array>

namespace forma::geom {

std::vector<std::uint32_t> triangulatePolygon(std::span<const std::vector<math::Vec2d>> rings)
{
    std::vector<std::vector<std::array<double, 2>>> polygon;
    polygon.reserve(rings.size());
    for (const auto& ring : rings) {
        auto& out = polygon.emplace_back();
        out.reserve(ring.size());
        for (const math::Vec2d& p : ring) {
            out.push_back({ p.x, p.y });
        }
    }
    return mapbox::earcut<std::uint32_t>(polygon);
}

FaceTriangles triangulateFace(const Mesh& mesh, FaceId id)
{
    const Face& face = mesh.face(id);
    const math::PlaneFrame frame(face.plane);

    FaceTriangles out;
    std::vector<std::vector<math::Vec2d>> rings;
    const auto addRing = [&](const Loop& loop) {
        auto& ring = rings.emplace_back();
        for (const math::Vec3d& p : mesh.loopPoints(loop)) {
            out.points.push_back(p);
            ring.push_back(frame.toLocal(p));
        }
    };
    addRing(face.outer);
    for (const Loop& loop : face.inner) {
        addRing(loop);
    }
    out.indices = triangulatePolygon(rings);

    // Earcut nu garantează orientarea; o aducem în sens trigonometric față de normală.
    for (std::size_t i = 0; i + 2 < out.indices.size(); i += 3) {
        const math::Vec2d a = frame.toLocal(out.points[out.indices[i]]);
        const math::Vec2d b = frame.toLocal(out.points[out.indices[i + 1]]);
        const math::Vec2d c = frame.toLocal(out.points[out.indices[i + 2]]);
        if ((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y) < 0.0) {
            std::swap(out.indices[i + 1], out.indices[i + 2]);
        }
    }
    return out;
}

} // namespace forma::geom
