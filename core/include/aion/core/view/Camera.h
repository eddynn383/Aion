#pragma once

#include "aion/core/math/Ray.h"
#include "aion/core/math/Transform.h"
#include "aion/core/math/Vec.h"

#include <optional>
#include <utility>

// Camera de navigare: orbitează în jurul unei ținte, cu axa Z în sus (ca în SketchUp).
// Unitățile sunt mm; coordonatele de ecran sunt în pixeli logici, cu originea în colțul
// din stânga-sus și y în jos (ca evenimentele de mouse).
namespace aion::view {

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
// Poza care privește din `eye` spre `target` (camerele-obiect din model).
[[nodiscard]] CameraPose poseLookingAt(const Vec3d& eye, const Vec3d& target);

class Camera {
public:
    static constexpr double kDefaultFovY = 0.6108652381980153; // 35°
    static constexpr double kMinDistance = 0.01; // 0,01 mm (detalii de ordinul milimetrilor)
    static constexpr double kMaxDistance = 1.0e8; // 100 km

    Camera();

    void setViewportSize(double widthPx, double heightPx);
    [[nodiscard]] Vec2d viewportSize() const { return m_viewport; }

    [[nodiscard]] const CameraPose& pose() const { return m_pose; }
    void setPose(const CameraPose& pose);

    [[nodiscard]] Projection projection() const { return m_projection; }
    void setProjection(Projection projection) { m_projection = projection; }

    [[nodiscard]] double fovY() const { return m_fovY; }
    // Unghiul de vedere vertical, în radiani (limitat la 5°–150°).
    void setFovY(double radians);

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

    // Rotește în jurul țintei sau, dacă e setat, în jurul pivotului (ca un corp rigid: punctul pivot rămâne
    // pe loc pe ecran). Pitch-ul e limitat la ±90°.
    void orbit(double deltaYaw, double deltaPitch);
    // Pivotul orbitei (selecția, punctul de sub cursor); nullopt = ținta camerei.
    void setOrbitPivot(std::optional<Vec3d> pivot) { m_orbitPivot = pivot; }
    [[nodiscard]] const std::optional<Vec3d>& orbitPivot() const { return m_orbitPivot; }

    // Mută ținta de-a lungul direcției de privire la adâncimea punctului dat, fără să miște camera.
    // Folosit înainte de zoom spre o suprafață: viteza zoom-ului și planurile de tăiere urmează adâncimea reală.
    void retargetToDepth(const Vec3d& point);

    // Cutia scenei (coordonate globale), ca planul îndepărtat să nu taie modelul când camera e foarte aproape
    // de un detaliu.
    void setSceneBounds(const Vec3d& min, const Vec3d& max);
    // Planurile de tăiere manuale (Preferințe, ca „Clip Start / End” din Blender), în mm; 0 = automat.
    void setClipOverride(double nearMm, double farMm);
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

    // O copie a camerei pentru un context de editare (grup, componentă) cu transformarea `toWorld`
    // (local → global): razele, proiecțiile, direcțiile și distanțele ei sunt în coordonatele locale,
    // așa că uneltele lucrează direct pe geometria contextului. Matricile (view, projection) rămân globale.
    [[nodiscard]] Camera inContext(const math::Transform& toWorld) const;
    [[nodiscard]] bool hasContext() const { return m_context.has_value(); }

private:
    struct Context {
        math::Transform toWorld;
        math::Transform toLocal;
        double scale = 1.0; // factorul de scară (uniform, aproximativ) local → global
    };

    [[nodiscard]] Vec3d worldForward() const;
    [[nodiscard]] Vec3d worldRight() const;
    [[nodiscard]] Vec3d worldUp() const;
    [[nodiscard]] Vec3d worldEye() const;
    [[nodiscard]] double worldNear() const;
    [[nodiscard]] double worldFar() const;
    [[nodiscard]] Vec3d toLocalVector(const Vec3d& v) const;

    std::optional<Context> m_context;
    std::optional<Vec3d> m_orbitPivot;
    std::optional<std::pair<Vec3d, Vec3d>> m_sceneBounds;
    double m_clipNear = 0.0;
    double m_clipFar = 0.0;
    CameraPose m_pose;
    Projection m_projection = Projection::Perspective;
    double m_fovY = kDefaultFovY;
    Vec2d m_viewport { 1.0, 1.0 };
};

} // namespace aion::view
