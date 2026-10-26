#pragma once

#include "aion/core/math/Plane.h"
#include "aion/core/math/Ray.h"
#include "aion/core/math/Segment.h"
#include "aion/core/math/Vec.h"

#include <optional>
#include <span>

// Teste de intersecție și calcule pe poligoane. Toleranțele vin din Tolerance.h.
namespace aion::math {

// Cele mai apropiate puncte dintre două segmente: s ∈ [0, 1] pe primul, t ∈ [0, 1] pe al doilea.
struct ClosestPoints {
    double s = 0.0;
    double t = 0.0;
    Vec3d onFirst;
    Vec3d onSecond;
    double distance = 0.0;
};
[[nodiscard]] ClosestPoints closestPoints(const Segment& first, const Segment& second);

// Parametrul și distanța de la un punct la segment (proiecția limitată la [0, 1]).
struct PointOnSegment {
    double t = 0.0;
    double distance = 0.0;
};
[[nodiscard]] PointOnSegment projectOnSegment(const Vec3d& p, const Segment& segment);

// Distanța t de-a lungul razei până la plan; nullopt dacă raza e paralelă sau planul e în spate.
[[nodiscard]] std::optional<double> intersectRayPlane(const Ray& ray, const Plane& plane);

// Möller–Trumbore, ambele fețe ale triunghiului. Întoarce t de-a lungul razei.
[[nodiscard]] std::optional<double> intersectRayTriangle(
    const Ray& ray, const Vec3d& a, const Vec3d& b, const Vec3d& c);

// Aria cu semn a unui poligon 2D (pozitivă în sens trigonometric).
[[nodiscard]] double signedArea(std::span<const Vec2d> polygon);

enum class PointLocation { Outside, OnBoundary, Inside };
// Poziția unui punct față de un poligon 2D simplu (orice orientare); granița are toleranța `eps`.
[[nodiscard]] PointLocation locatePoint(const Vec2d& p, std::span<const Vec2d> polygon, double eps);

} // namespace aion::math
