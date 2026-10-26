#pragma once

#include "forma/core/math/Ray.h"
#include "forma/core/math/Vec.h"

#include <optional>

// Camera de navigare: orbitează în jurul unei ținte, cu axa Z în sus (ca în SketchUp).
// Unitățile sunt mm; coordonatele de ecran sunt în pixeli logici, cu originea în colțul
// din stânga-sus și y în jos (ca evenimentele de mouse).
namespace forma::view {

using math::Mat4d;
using math::Ray;
using math::Vec2d;
using math::Vec3d;

enum class Projection { Perspective, Orthographic };

enum class StandardView { Top, Bottom, Front, Back, Left, Right, Iso };

// Poziția camerei, independentă de proiecție și de mărimea ferestrei.
// yaw: rotația în jurul axei Z (0 = privește spre +X); pitch: în sus (+) sau în jos (−), în radiani.
struct CameraPose {
    Vec3d target { 0.0, 0.0, 0.0 };
    double yaw = 0.0;
    double pitch = 0.0;
    double distance = 1.0;
};

[[nodiscard]] CameraPose standardViewPose(StandardView view, const CameraPose& current);

class Camera {
public:
    static constexpr double kDefaultFovY = 0.6108652381980153; // 35°
    static constexpr double kMinDistance = 1.0; // 1 mm
    static constexpr double kMaxDistance = 1.0e8; // 100 km

    Camera();

    void setViewportSize(double widthPx, double heightPx);
    [[nodiscard]] Vec2d viewportSize() const { return m_viewport; }

    [[nodiscard]] const CameraPose& pose() const { return m_pose; }
    void setPose(const CameraPose& pose);

    [[nodiscard]] Projection projection() const { return m_projection; }
    void setProjection(Projection projection) { m_projection = projection; }

    [[nodiscard]] double fovY() const { return m_fovY; }

    // Baza camerei în coordonate globale. `right` rămâne orizontal, chiar și privind vertical.
    [[nodiscard]] Vec3d forward() const;
    [[nodiscard]] Vec3d right() const;
    [[nodiscard]] Vec3d up() const;
    [[nodiscard]] Vec3d eye() const;

    // Matrici în convenția OpenGL (adâncime în [−1, 1]); render/ le adaptează pentru backend.
    [[nodiscard]] Mat4d viewMatrix() const;
    [[nodiscard]] Mat4d projectionMatrix() const;
    [[nodiscard]] double nearPlane() const;
    [[nodiscard]] double farPlane() const;

    // Raza care trece prin pixelul dat.
    [[nodiscard]] Ray rayAt(Vec2d screen) const;
    // Poziția pe ecran a unui punct; nullopt dacă e în spatele camerei.
    [[nodiscard]] std::optional<Vec2d> project(const Vec3d& world) const;
    // Mărimea unui pixel, în mm, la adâncimea țintei.
    [[nodiscard]] double worldPerPixelAtTarget() const;

    // Rotește în jurul țintei. Pitch-ul e limitat la ±90°.
    void orbit(double deltaYaw, double deltaPitch);
    // Mută camera ca punctele de la adâncimea țintei să urmeze cursorul.
    void pan(Vec2d deltaPx);
    // Scalează distanța față de `anchor` (factor < 1 = apropiere). Punctul `anchor`
    // rămâne pe loc pe ecran, deci zoom-ul se face „spre cursor”.
    void zoomAbout(double factor, const Vec3d& anchor);
    // Punctul folosit ca ancoră pentru zoom spre cursor: intersecția cu planul de sol,
    // dacă e destul de aproape, altfel cu planul care trece prin țintă.
    [[nodiscard]] Vec3d zoomAnchorAt(Vec2d screen) const;

    // Poziția care încadrează cutia dată, păstrând direcția de privire.
    [[nodiscard]] CameraPose framing(const Vec3d& boundsMin, const Vec3d& boundsMax) const;

private:
    CameraPose m_pose;
    Projection m_projection = Projection::Perspective;
    double m_fovY = kDefaultFovY;
    Vec2d m_viewport { 1.0, 1.0 };
};

} // namespace forma::view
