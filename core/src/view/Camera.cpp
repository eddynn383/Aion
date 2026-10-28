#include "aion/core/view/Camera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace aion::view {

namespace {

constexpr double kPi = glm::pi<double>();
constexpr double kHalfPi = glm::half_pi<double>();

// Raportul far/near ține precizia buffer-ului de adâncime (24 de biți) acceptabilă.
constexpr double kNearFactor = 0.01;
constexpr double kFarFactor = 100.0;
constexpr double kMinNear = 0.001;
// Raportul maxim far/near când planul îndepărtat cuprinde toată scena.
constexpr double kMaxDepthRatio = 1.0e6;

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

Vec3d Camera::toLocalVector(const Vec3d& v) const
{
    return m_context ? glm::normalize(m_context->toLocal.applyToVector(v)) : v;
}

CameraPose poseLookingAt(const Vec3d& eye, const Vec3d& target)
{
    const Vec3d d = target - eye;
    const double length = glm::length(d);
    CameraPose pose;
    pose.target = target;
    pose.distance = std::max(length, Camera::kMinDistance);
    if (length > 0.0) {
        pose.yaw = std::atan2(d.y, d.x);
        pose.pitch = std::asin(std::clamp(d.z / length, -1.0, 1.0));
    }
    return pose;
}

void Camera::setFovY(double radians)
{
    m_fovY = std::clamp(radians, 0.0043633231, 2.9670597284);
}

Vec3d Camera::forward() const
{
    return toLocalVector(worldForward());
}

Vec3d Camera::right() const
{
    return toLocalVector(worldRight());
}

Vec3d Camera::up() const
{
    return toLocalVector(worldUp());
}

Vec3d Camera::eye() const
{
    return m_context ? m_context->toLocal.applyToPoint(worldEye()) : worldEye();
}

double Camera::nearPlane() const
{
    return m_context ? worldNear() / m_context->scale : worldNear();
}

double Camera::farPlane() const
{
    return m_context ? worldFar() / m_context->scale : worldFar();
}

Camera Camera::inContext(const math::Transform& toWorld) const
{
    Camera c = *this;
    const math::Transform base = m_context ? m_context->toWorld : math::Transform {};
    const math::Transform combined = base * toWorld;
    const double det = std::abs(glm::determinant(glm::dmat3(combined.matrix())));
    c.m_context = Context { combined, combined.inverse(), det > 1e-300 ? std::cbrt(det) : 1.0 };
    return c;
}

Vec3d Camera::worldForward() const
{
    const double cp = std::cos(m_pose.pitch);
    return { cp * std::cos(m_pose.yaw), cp * std::sin(m_pose.yaw), std::sin(m_pose.pitch) };
}

Vec3d Camera::worldRight() const
{
    const Vec3d level { std::sin(m_pose.yaw), -std::cos(m_pose.yaw), 0.0 };
    if (m_roll == 0.0) {
        return level;
    }
    const Vec3d up = glm::cross(level, worldForward());
    return level * std::cos(m_roll) + up * std::sin(m_roll);
}

Vec3d Camera::worldUp() const
{
    return glm::cross(worldRight(), worldForward());
}

Vec3d Camera::worldEye() const
{
    return m_pose.target - worldForward() * m_pose.distance;
}

Mat4d Camera::viewMatrix() const
{
    const Vec3d f = worldForward();
    const Vec3d r = worldRight();
    const Vec3d u = worldUp();
    const Vec3d e = worldEye();

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

void Camera::setSceneBounds(const Vec3d& min, const Vec3d& max)
{
    m_sceneBounds = std::pair { min, max };
}

void Camera::setClipOverride(double nearMm, double farMm)
{
    m_clipNear = std::max(nearMm, 0.0);
    m_clipFar = std::max(farMm, 0.0);
}

double Camera::worldNear() const
{
    if (m_projection == Projection::Orthographic) {
        return -worldFar();
    }
    if (m_clipNear > 0.0) {
        return m_clipNear;
    }
    return std::max({ m_pose.distance * kNearFactor, worldFar() / kMaxDepthRatio, kMinNear });
}

double Camera::worldFar() const
{
    if (m_clipFar > 0.0) {
        return m_clipFar;
    }
    double far = m_pose.distance * kFarFactor;
    // Colțul cel mai îndepărtat al scenei trebuie să rămână vizibil (altfel, aproape de un detaliu de câțiva mm,
    // restul modelului ar dispărea).
    if (m_sceneBounds) {
        const auto& [lo, hi] = *m_sceneBounds;
        const Vec3d e = worldEye();
        const Vec3d corner { std::abs(lo.x - e.x) > std::abs(hi.x - e.x) ? lo.x : hi.x,
            std::abs(lo.y - e.y) > std::abs(hi.y - e.y) ? lo.y : hi.y, std::abs(lo.z - e.z) > std::abs(hi.z - e.z) ? lo.z : hi.z };
        far = std::max(far, glm::length(corner - e) * 1.05);
    }
    return far;
}

Mat4d Camera::projectionMatrix() const
{
    const double aspect = m_viewport.x / m_viewport.y;
    Mat4d p(1.0);
    if (m_projection == Projection::Orthographic) {
        const double halfH = m_pose.distance * std::tan(m_fovY * 0.5);
        const double halfW = halfH * aspect;
        p = glm::ortho(-halfW, halfW, -halfH, halfH, worldNear(), worldFar());
        p[3][0] -= m_shift.x;
        p[3][1] -= m_shift.y;
        return p;
    }
    p = glm::perspective(m_fovY, aspect, worldNear(), worldFar());
    // x_ndc = P00 · x / (−z) − P20: centrul proiecției se mută cu −shift (cadrul, cu +shift).
    p[2][0] = m_shift.x;
    p[2][1] = m_shift.y;
    return p;
}

Ray Camera::rayAt(Vec2d screen) const
{
    const double x = 2.0 * screen.x / m_viewport.x - 1.0 + m_shift.x;
    const double y = 1.0 - 2.0 * screen.y / m_viewport.y + m_shift.y;
    const double tanHalf = std::tan(m_fovY * 0.5);
    const double aspect = m_viewport.x / m_viewport.y;

    const Vec3d f = worldForward();
    const Vec3d r = worldRight();
    const Vec3d u = worldUp();

    Ray ray;
    if (m_projection == Projection::Orthographic) {
        const double halfH = m_pose.distance * tanHalf;
        const Vec3d onEyePlane = worldEye() + r * (x * halfH * aspect) + u * (y * halfH);
        ray = { onEyePlane + f * worldNear(), f };
    } else {
        const Vec3d dir = f + r * (x * tanHalf * aspect) + u * (y * tanHalf);
        ray = { worldEye(), glm::normalize(dir) };
    }
    if (m_context) {
        ray = { m_context->toLocal.applyToPoint(ray.origin), toLocalVector(ray.direction) };
    }
    return ray;
}

std::optional<Vec2d> Camera::project(const Vec3d& point) const
{
    const Vec3d world = m_context ? m_context->toWorld.applyToPoint(point) : point;
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
    const double world = 2.0 * m_pose.distance * std::tan(m_fovY * 0.5) / m_viewport.y;
    return m_context ? world / m_context->scale : world;
}

void Camera::orbit(double deltaYaw, double deltaPitch)
{
    if (!m_orbitPivot) {
        CameraPose p = m_pose;
        p.yaw += deltaYaw;
        p.pitch += deltaPitch;
        setPose(p);
        return;
    }
    // Rotație rigidă în jurul pivotului: ținta își păstrează coordonatele în baza camerei relativ la pivot.
    const Vec3d pivot = *m_orbitPivot;
    const Vec3d v = m_pose.target - pivot;
    const double a = glm::dot(v, worldRight());
    const double b = glm::dot(v, worldUp());
    const double c = glm::dot(v, worldForward());
    CameraPose p = m_pose;
    p.yaw += deltaYaw;
    p.pitch += deltaPitch;
    setPose(p);
    m_pose.target = pivot + worldRight() * a + worldUp() * b + worldForward() * c;
}

void Camera::retargetToDepth(const Vec3d& point)
{
    const Vec3d e = worldEye();
    const Vec3d f = worldForward();
    const double depth = std::clamp(glm::dot(point - e, f), kMinDistance, kMaxDistance);
    m_pose.target = e + f * depth;
    m_pose.distance = depth;
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

} // namespace aion::view
