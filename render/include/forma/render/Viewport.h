#pragma once

#include "forma/core/view/Camera.h"
#include "forma/core/view/CameraMotion.h"

#include <QColor>
#include <QElapsedTimer>
#include <QPointF>
#include <QQuickRhiItem>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <deque>

namespace forma::render {

struct ViewportFrame;

// Viewport-ul 3D integrat în scena QML. Ține camera și tratează navigarea
// (mouse, trackpad); randarea propriu-zisă se face în ViewportRenderer.
class Viewport : public QQuickRhiItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor backgroundTop MEMBER m_backgroundTop NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundBottom MEMBER m_backgroundBottom NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridMinorColor MEMBER m_gridMinor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridMajorColor MEMBER m_gridMajor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor axisXColor MEMBER m_axisX NOTIFY appearanceChanged)
    Q_PROPERTY(QColor axisYColor MEMBER m_axisY NOTIFY appearanceChanged)
    Q_PROPERTY(QColor axisZColor MEMBER m_axisZ NOTIFY appearanceChanged)
    Q_PROPERTY(QColor edgeColor MEMBER m_edge NOTIFY appearanceChanged)
    Q_PROPERTY(QColor faceFrontColor MEMBER m_faceFront NOTIFY appearanceChanged)
    Q_PROPERTY(QColor faceBackColor MEMBER m_faceBack NOTIFY appearanceChanged)
    Q_PROPERTY(bool gridVisible MEMBER m_gridVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool demoVisible MEMBER m_demoVisible NOTIFY appearanceChanged)

    Q_PROPERTY(bool orthographic READ isOrthographic WRITE setOrthographic NOTIFY cameraChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    // Unealta de navigare legată de butonul stâng: "" (niciuna), "orbit" sau "pan".
    Q_PROPERTY(QString navigationTool MEMBER m_navigationTool NOTIFY navigationToolChanged)

    // Baza camerei, pentru gizmo-ul de navigare.
    Q_PROPERTY(QVector3D cameraRight READ cameraRight NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D cameraUp READ cameraUp NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D cameraForward READ cameraForward NOTIFY cameraChanged)
    Q_PROPERTY(double cameraDistance READ cameraDistance NOTIFY cameraChanged)

    // Cadre pe secundă în ultima secundă (0 când viewport-ul nu se redesenează).
    Q_PROPERTY(int framesPerSecond READ framesPerSecond NOTIFY framesPerSecondChanged)

public:
    enum StandardView { Top, Bottom, Front, Back, Left, Right, Iso };
    Q_ENUM(StandardView)

    explicit Viewport(QQuickItem* parent = nullptr);
    ~Viewport() override;

    bool isOrthographic() const;
    void setOrthographic(bool orthographic);
    bool reducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool reduced);

    QVector3D cameraRight() const;
    QVector3D cameraUp() const;
    QVector3D cameraForward() const;
    double cameraDistance() const { return m_camera.pose().distance; }
    int framesPerSecond() const { return m_fps; }

    Q_INVOKABLE void setStandardView(forma::render::Viewport::StandardView view);
    Q_INVOKABLE void zoomExtents();
    Q_INVOKABLE void toggleProjection();
    // Rotește camera continuu timp de `seconds` secunde (pentru măsurarea fps-ului).
    Q_INVOKABLE void startTurntable(double seconds);

    // Apelat de renderer (pe firul de randare, cu firul GUI blocat).
    ViewportFrame frameForRendering() const;

signals:
    void appearanceChanged();
    void cameraChanged();
    void reducedMotionChanged();
    void navigationToolChanged();
    void framesPerSecondChanged();

protected:
    QQuickRhiItemRenderer* createRenderer() override;
    bool event(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& data) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;

private:
    enum class Drag { None, Orbit, Pan };

    void onCameraMoved();
    void requestAnimationFrame();
    void onAfterAnimating();
    void beginDrag(Drag drag, QPointF position);
    void recordOrbitSample(QPointF delta);
    view::Vec2d orbitReleaseVelocity() const;
    void sceneBounds(view::Vec3d& min, view::Vec3d& max) const;

    view::Camera m_camera;
    view::CameraMotion m_motion;
    QElapsedTimer m_frameClock;
    qint64 m_lastTickNs = 0;
    bool m_ticking = false;
    double m_turntableSecondsLeft = 0.0;

    Drag m_drag = Drag::None;
    QPointF m_lastPointer;
    struct OrbitSample {
        qint64 timeNs;
        QPointF delta;
    };
    std::deque<OrbitSample> m_orbitSamples;
    QPointF m_hoverPosition;

    QColor m_backgroundTop { 0x1A, 0x1B, 0x1F };
    QColor m_backgroundBottom { 0x0E, 0x0F, 0x11 };
    QColor m_gridMinor { 255, 255, 255, 18 };
    QColor m_gridMajor { 255, 255, 255, 40 };
    QColor m_axisX { 0xEF, 0x44, 0x44 };
    QColor m_axisY { 0x22, 0xC5, 0x5E };
    QColor m_axisZ { 0x3B, 0x82, 0xF6 };
    QColor m_edge { 0x11, 0x11, 0x13 };
    QColor m_faceFront { 0xE8, 0xE6, 0xE1 };
    QColor m_faceBack { 0x8F, 0x9B, 0xB3 };
    bool m_gridVisible = true;
    bool m_demoVisible = true;
    bool m_reducedMotion = false;
    QString m_navigationTool;

    std::atomic<int> m_framesThisSecond { 0 };
    int m_fps = 0;
};

} // namespace forma::render
