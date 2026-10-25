#include "forma/core/view/CameraMotion.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

namespace forma::view {

namespace {

// Cât de repede se stinge inerția orbitei (1/s). La 6, viteza scade la jumătate în ~115 ms.
constexpr double kOrbitDamping = 6.0;
constexpr double kMinOrbitSpeed = 0.01; // rad/s

// Cât de repede se consumă zoom-ul în așteptare (1/s).
constexpr double kZoomRate = 18.0;
constexpr double kMinZoomLog = 1e-4;

double easeInOutCubic(double t)
{
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

// Diferența unghiulară cea mai scurtă, în (−π, π].
double shortestAngle(double from, double to)
{
    constexpr double twoPi = glm::two_pi<double>();
    double d = std::fmod(to - from, twoPi);
    if (d > glm::pi<double>()) {
        d -= twoPi;
    } else if (d <= -glm::pi<double>()) {
        d += twoPi;
    }
    return d;
}

CameraPose interpolate(const CameraPose& a, const CameraPose& b, double t)
{
    CameraPose p;
    p.target = a.target + (b.target - a.target) * t;
    p.yaw = a.yaw + shortestAngle(a.yaw, b.yaw) * t;
    p.pitch = a.pitch + (b.pitch - a.pitch) * t;
    // Distanța se interpolează geometric, ca zoom-ul să pară uniform.
    p.distance = std::exp(std::log(a.distance) + (std::log(b.distance) - std::log(a.distance)) * t);
    return p;
}

} // namespace

void CameraMotion::animateTo(const Camera& camera, const CameraPose& target, double durationSeconds)
{
    stop();
    m_transition = Transition { camera.pose(), target, 0.0, m_reducedMotion ? 0.0 : durationSeconds };
}

void CameraMotion::setOrbitVelocity(Vec2d radiansPerSecond)
{
    m_orbitVelocity = m_reducedMotion ? Vec2d { 0.0, 0.0 } : radiansPerSecond;
}

void CameraMotion::zoomSmoothly(double factor, const Vec3d& anchor)
{
    m_transition.reset();
    m_pendingZoomLog += std::log(factor);
    m_zoomAnchor = anchor;
}

void CameraMotion::stop()
{
    m_transition.reset();
    m_orbitVelocity = { 0.0, 0.0 };
    m_pendingZoomLog = 0.0;
}

bool CameraMotion::isActive() const
{
    return m_transition.has_value() || glm::length(m_orbitVelocity) > 0.0 || m_pendingZoomLog != 0.0;
}

bool CameraMotion::update(Camera& camera, double dtSeconds)
{
    bool changed = false;

    if (m_transition) {
        m_transition->elapsed += dtSeconds;
        const double t = m_transition->duration <= 0.0
            ? 1.0
            : std::min(m_transition->elapsed / m_transition->duration, 1.0);
        camera.setPose(interpolate(m_transition->from, m_transition->to, easeInOutCubic(t)));
        if (t >= 1.0) {
            m_transition.reset();
        }
        changed = true;
    }

    if (glm::length(m_orbitVelocity) > 0.0) {
        camera.orbit(m_orbitVelocity.x * dtSeconds, m_orbitVelocity.y * dtSeconds);
        m_orbitVelocity *= std::exp(-kOrbitDamping * dtSeconds);
        if (glm::length(m_orbitVelocity) < kMinOrbitSpeed) {
            m_orbitVelocity = { 0.0, 0.0 };
        }
        changed = true;
    }

    if (m_pendingZoomLog != 0.0) {
        double step = m_reducedMotion ? m_pendingZoomLog
                                      : m_pendingZoomLog * (1.0 - std::exp(-kZoomRate * dtSeconds));
        if (std::abs(m_pendingZoomLog - step) < kMinZoomLog) {
            step = m_pendingZoomLog;
        }
        camera.zoomAbout(std::exp(step), m_zoomAnchor);
        m_pendingZoomLog -= step;
        if (std::abs(m_pendingZoomLog) < kMinZoomLog) {
            m_pendingZoomLog = 0.0;
        }
        changed = true;
    }

    return changed;
}

} // namespace forma::view
