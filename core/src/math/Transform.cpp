#include "forma/core/math/Transform.h"

#include "forma/core/math/Tolerance.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

namespace forma::math {

Transform Transform::translation(const Vec3d& offset)
{
    return Transform(glm::translate(Mat4d(1.0), offset));
}

Transform Transform::rotation(const Vec3d& pivot, const Vec3d& axis, double angle)
{
    Mat4d m = glm::translate(Mat4d(1.0), pivot);
    m = glm::rotate(m, angle, glm::normalize(axis));
    m = glm::translate(m, -pivot);
    return Transform(m);
}

Transform Transform::scaling(const Vec3d& pivot, const Vec3d& factors)
{
    Mat4d m = glm::translate(Mat4d(1.0), pivot);
    m = glm::scale(m, factors);
    m = glm::translate(m, -pivot);
    return Transform(m);
}

Vec3d Transform::applyToPoint(const Vec3d& p) const
{
    const glm::dvec4 r = m_matrix * glm::dvec4(p, 1.0);
    return Vec3d(r) / r.w;
}

Vec3d Transform::applyToVector(const Vec3d& v) const
{
    return Vec3d(m_matrix * glm::dvec4(v, 0.0));
}

Vec3d Transform::applyToNormal(const Vec3d& n) const
{
    const glm::dmat3 normalMatrix = glm::transpose(glm::inverse(glm::dmat3(m_matrix)));
    return glm::normalize(normalMatrix * n);
}

Transform Transform::inverse() const
{
    return Transform(glm::inverse(m_matrix));
}

Transform Transform::operator*(const Transform& other) const
{
    return Transform(m_matrix * other.m_matrix);
}

bool Transform::isIdentity() const
{
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            if (!tol::equal(m_matrix[c][r], c == r ? 1.0 : 0.0, 1e-12)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace forma::math
