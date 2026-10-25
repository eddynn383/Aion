#include "forma/core/view/Camera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace forma::view {

namespace {

constexpr double kPi = glm::pi<double>();
constexpr double kHalfPi = glm::half_pi<double>();

// Raportul far/near ține precizia buffer-ului de adâncime (24 de biți) acceptabilă.
constexpr double kNearFactor = 0.01;
constexpr double kFarFactor = 100.0;
constexpr double kMinNear = 0.1;

// Ancora de zoom pe sol se folosește doar dacă e la cel mult atâtea distanțe-țintă.
constexpr double kMaxGroundAnchorFactor = 20.0;

double wrapAngle(double a)
{
    a = std::fmod(a + kPi, 2.0 * kPi);
    if (a < 0.0) {
        a += 2.0 * kPi;
    }
    return a - kPi;
}

} // namespace

CameraPose standardViewPose(StandardView view, const CameraPose& current)
{
    CameraPose pose = current;
    switch (view) {
    case StandardView::Top:
        pose.yaw = kHalfPi;
        pose.pitch = -kHalfPi;
        break;
    case StandardView::Bottom:
        pose.yaw = kHalfPi;
        pose.pitch = kHalfPi;
        break;
    case StandardView::Front: // privește spre +Y
        pose.yaw = kHalfPi;
        pose.pitch = 0.0;
        break;
    case StandardView::Back:
        pose.yaw = -kHalfPi;
        pose.pitch = 0.0;
        break;
    case StandardView::Left: // privește spre +X
        pose.yaw = 0.0;
        pose.pitch = 0.0;
        break;
    case StandardView::Right:
        pose.yaw = kPi;
        pose.pitch = 0.0;
        break;
    case StandardView::Iso: // din colțul (−X, −Y, +Z)
        pose.yaw = kPi / 4.0;
        pose.pitch = -std::atan(1.0 / std::sqrt(2.0));
        break;
    }
    return pose;
}

Camera::Camera()
{
    m_pose = standardViewPose(StandardView::Iso, CameraPose { { 0.0, 0.0, 0.0 }, 0.0, 0.0, 10000.0 });
}

void Camera::setViewportSize(double widthPx, double heightPx)
{
    m_viewport = { std::max(widthPx, 1.0), std::max(heightPx, 1.0) };
}

void Camera::setPose(const CameraPose& pose)
{
    m_pose = pose;
    m_pose.yaw = wrapAngle(pose.yaw);
    m_pose.pitch = std::clamp(pose.pitch, -kHalfPi, kHalfPi);
    m_pose.distance = std::clamp(pose.distance, kMinDistance, kMaxDistance);
}

Vec3d Camera::forward() const
{
    const double cp = std::cos(m_pose.pitch);
    return { cp * std::cos(m_pose.yaw), cp * std::sin(m_pose.yaw), std::sin(m_pose.pitch) };
}

Vec3d Camera::right() const
{
    return { std::sin(m_pose.yaw), -std::cos(m_pose.yaw), 0.0 };
}

Vec3d Camera::up() const
{
    return glm::cross(right(), forward());
}

Vec3d Camera::eye() const
{
    return m_pose.target - forward() * m_pose.distance;
}

Mat4d Camera::viewMatrix() const
{
    const Vec3d f = forward();
    const Vec3d r = right();
    const Vec3d u = up();
    const Vec3d e = eye();

    Mat4d v(1.0);
    v[0][0] = r.x;
    v[1][0] = r.y;
    v[2][0] = r.z;
    v[0][1] = u.x;
    v[1][1] = u.y;
    v[2][1] = u.z;
    v[0][2] = -f.x;
    v[1][2] = -f.y;
    v[2][2] = -f.z;
    v[3][0] = -glm::dot(r, e);
    v[3][1] = -glm::dot(u, e);
    v[3][2] = glm::dot(f, e);
    return v;
}

double Camera::nearPlane() const
{
    if (m_projection == Projection::Orthographic) {
        return -m_pose.distance * kFarFactor;
    }
    return std::max(m_pose.distance * kNearFactor, kMinNear);
}

double Camera::farPlane() const
{
    return m_pose.distance * kFarFactor;
}

Mat4d Camera::projectionMatrix() const
{
    const double aspect = m_viewport.x / m_viewport.y;
    if (m_projection == Projection::Orthographic) {
        const double halfH = m_pose.distance * std::tan(m_fovY * 0.5);
        const double halfW = halfH * aspect;
        return glm::ortho(-halfW, halfW, -halfH, halfH, nearPlane(), farPlane());
    }
    return glm::perspective(m_fovY, aspect, nearPlane(), farPlane());
}

Ray Camera::rayAt(Vec2d screen) const
{
    const double x = 2.0 * screen.x / m_viewport.x - 1.0;
    const double y = 1.0 - 2.0 * screen.y / m_viewport.y;
    const double tanHalf = std::tan(m_fovY * 0.5);
    const double aspect = m_viewport.x / m_viewport.y;

    const Vec3d f = forward();
    const Vec3d r = right();
    const Vec3d u = up();

    if (m_projection == Projection::Orthographic) {
        const double halfH = m_pose.distance * tanHalf;
        const Vec3d onEyePlane = eye() + r * (x * halfH * aspect) + u * (y * halfH);
        return { onEyePlane + f * nearPlane(), f };
    }
    const Vec3d dir = f + r * (x * tanHalf * aspect) + u * (y * tanHalf);
    return { eye(), glm::normalize(dir) };
}

std::optional<Vec2d> Camera::project(const Vec3d& world) const
{
    const glm::dvec4 clip = projectionMatrix() * viewMatrix() * glm::dvec4(world, 1.0);
    if (clip.w <= 0.0) {
        return std::nullopt;
    }
    const double x = clip.x / clip.w;
    const double y = clip.y / clip.w;
    return Vec2d { (x + 1.0) * 0.5 * m_viewport.x, (1.0 - y) * 0.5 * m_viewport.y };
}

double Camera::worldPerPixelAtTarget() const
{
    return 2.0 * m_pose.distance * std::tan(m_fovY * 0.5) / m_viewport.y;
}

void Camera::orbit(double deltaYaw, double deltaPitch)
{
    CameraPose p = m_pose;
    p.yaw += deltaYaw;
    p.pitch += deltaPitch;
    setPose(p);
}

void Camera::pan(Vec2d deltaPx)
{
    const double wpp = worldPerPixelAtTarget();
    m_pose.target += (-right() * deltaPx.x + up() * deltaPx.y) * wpp;
}

void Camera::zoomAbout(double factor, const Vec3d& anchor)
{
    const double newDistance = std::clamp(m_pose.distance * factor, kMinDistance, kMaxDistance);
    const double applied = newDistance / m_pose.distance;
    m_pose.target = anchor + (m_pose.target - anchor) * applied;
    m_pose.distance = newDistance;
}

Vec3d Camera::zoomAnchorAt(Vec2d screen) const
{
    const Ray ray = rayAt(screen);
    const Vec3d e = eye();

    if (std::abs(ray.direction.z) > 1e-9) {
        const double t = -ray.origin.z / ray.direction.z;
        if (t > 0.0) {
            const Vec3d ground = ray.origin + ray.direction * t;
            if (glm::length(ground - e) <= m_pose.distance * kMaxGroundAnchorFactor) {
                return ground;
            }
        }
    }

    const Vec3d f = forward();
    const double denom = glm::dot(ray.direction, f);
    const double t = glm::dot(m_pose.target - ray.origin, f) / denom;
    return ray.origin + ray.direction * t;
}

CameraPose Camera::framing(const Vec3d& boundsMin, const Vec3d& boundsMax) const
{
    const Vec3d center = (boundsMin + boundsMax) * 0.5;
    const double radius = std::max(glm::length(boundsMax - boundsMin) * 0.5, 1.0);
    const double aspect = m_viewport.x / m_viewport.y;
    const double halfFovY = m_fovY * 0.5;
    const double halfFovX = std::atan(std::tan(halfFovY) * aspect);
    const double halfFov = std::min(halfFovY, halfFovX);

    CameraPose p = m_pose;
    p.target = center;
    p.distance = std::clamp(radius / std::sin(halfFov), kMinDistance, kMaxDistance);
    return p;
}

} // namespace forma::view
