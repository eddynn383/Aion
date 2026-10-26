#pragma once

#include "forma/core/math/Vec.h"

namespace forma::math {

// Transformare afină (mutare, rotație, scalare) pentru grupuri, componente și unelte.
class Transform {
public:
    Transform() = default;
    explicit Transform(const Mat4d& matrix)
        : m_matrix(matrix)
    {
    }

    [[nodiscard]] static Transform translation(const Vec3d& offset);
    // Rotație cu `angle` radiani în jurul axei care trece prin `pivot`.
    [[nodiscard]] static Transform rotation(const Vec3d& pivot, const Vec3d& axis, double angle);
    [[nodiscard]] static Transform scaling(const Vec3d& pivot, const Vec3d& factors);

    [[nodiscard]] Vec3d applyToPoint(const Vec3d& p) const;
    [[nodiscard]] Vec3d applyToVector(const Vec3d& v) const;
    // Normalele se transformă cu inversa transpusă (corect și la scalări neuniforme).
    [[nodiscard]] Vec3d applyToNormal(const Vec3d& n) const;

    [[nodiscard]] Transform inverse() const;
    // (a * b) aplică întâi b, apoi a.
    [[nodiscard]] Transform operator*(const Transform& other) const;

    [[nodiscard]] const Mat4d& matrix() const { return m_matrix; }
    [[nodiscard]] bool isIdentity() const;

private:
    Mat4d m_matrix { 1.0 };
};

} // namespace forma::math
