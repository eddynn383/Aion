#pragma once

#include "forma/core/math/Vec.h"

#include <optional>
#include <span>

namespace forma::math {

// Planul n · x = d, cu n normalizat.
struct Plane {
    Vec3d normal { 0.0, 0.0, 1.0 };
    double d = 0.0;

    [[nodiscard]] static Plane fromPointNormal(const Vec3d& point, const Vec3d& normal);
    // Planul celor trei puncte (în sens trigonometric = normala spre privitor); nullopt dacă sunt coliniare.
    [[nodiscard]] static std::optional<Plane> fromPoints(const Vec3d& a, const Vec3d& b, const Vec3d& c);
    // Planul unui poligon (metoda lui Newell, robustă și pentru poligoane concave); nullopt dacă e degenerat.
    [[nodiscard]] static std::optional<Plane> fromPolygon(std::span<const Vec3d> points);

    [[nodiscard]] double signedDistance(const Vec3d& p) const;
    [[nodiscard]] Vec3d project(const Vec3d& p) const;
    [[nodiscard]] Vec3d origin() const { return normal * d; }
    [[nodiscard]] Plane flipped() const { return { -normal, -d }; }

    // Egalitate exactă (pentru compararea stărilor la undo), nu geometrică; vezi coincident().
    bool operator==(const Plane&) const = default;

    // Aceeași mulțime de puncte (normala poate avea sens opus).
    [[nodiscard]] bool coincident(const Plane& other) const;
};

// Sistem de coordonate 2D pe un plan, pentru algoritmii pe poligoane.
// Axele (u, v, normal) formează un reper drept, deci sensul trigonometric în 2D
// corespunde sensului trigonometric privit dinspre normală.
class PlaneFrame {
public:
    explicit PlaneFrame(const Plane& plane);

    [[nodiscard]] Vec2d toLocal(const Vec3d& p) const;
    [[nodiscard]] Vec3d toWorld(const Vec2d& p) const;
    [[nodiscard]] const Plane& plane() const { return m_plane; }

private:
    Plane m_plane;
    Vec3d m_origin;
    Vec3d m_u;
    Vec3d m_v;
};

} // namespace forma::math
