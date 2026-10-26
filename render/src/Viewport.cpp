#include "forma/render/Viewport.h"

#include "ViewportFrame.h"
#include "ViewportRenderer.h"

#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QQuickWindow>

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace forma::render {

namespace {

// Radiani de rotație pe pixel de mișcare a mouse-ului (sau a degetelor pe trackpad).
constexpr double kOrbitRadiansPerPixel = 0.008;
// Factorul de zoom pentru o „treaptă” de rotiță (120 de unități).
constexpr double kWheelZoomStep = 0.85;
// Cât de recentă trebuie să fie mișcarea ca, la eliberare, orbita să continue prin inerție.
constexpr qint64 kInertiaWindowNs = 80'000'000;
constexpr qint64 kInertiaMaxIdleNs = 40'000'000;

// Cutia demo (în mm), afișată până când modelul poate conține geometrie (Faza 3).
constexpr glm::dvec3 kDemoMin { 0.0, 0.0, 0.0 };
constexpr glm::dvec3 kDemoMax { 2000.0, 1500.0, 1000.0 };

glm::vec4 toVec4(const QColor& c)
{
    return { c.redF(), c.greenF(), c.blueF(), c.alphaF() };
}

glm::vec4 withAlpha(const QColor& c, float alpha)
{
    glm::vec4 v = toVec4(c);
    v.a *= alpha;
    return v;
}

QVector3D toQt(const glm::dvec3& v)
{
    return { static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z) };
}

view::Vec2d toVec2(QPointF p)
{
    return { p.x(), p.y() };
}

void appendBox(std::vector<MeshVertex>& out, const glm::dvec3& lo, const glm::dvec3& hi)
{
    const glm::vec3 a(lo);
    const glm::vec3 b(hi);
    const auto quad = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 n) {
        for (const glm::vec3& p : { p0, p1, p2, p0, p2, p3 }) {
            out.push_back({ p, n });
        }
    };
    quad({ a.x, a.y, a.z }, { a.x, b.y, a.z }, { b.x, b.y, a.z }, { b.x, a.y, a.z }, { 0, 0, -1 });
    quad({ a.x, a.y, b.z }, { b.x, a.y, b.z }, { b.x, b.y, b.z }, { a.x, b.y, b.z }, { 0, 0, 1 });
    quad({ a.x, a.y, a.z }, { b.x, a.y, a.z }, { b.x, a.y, b.z }, { a.x, a.y, b.z }, { 0, -1, 0 });
    quad({ a.x, b.y, a.z }, { a.x, b.y, b.z }, { b.x, b.y, b.z }, { b.x, b.y, a.z }, { 0, 1, 0 });
    quad({ a.x, a.y, a.z }, { a.x, a.y, b.z }, { a.x, b.y, b.z }, { a.x, b.y, a.z }, { -1, 0, 0 });
    quad({ b.x, a.y, a.z }, { b.x, b.y, a.z }, { b.x, b.y, b.z }, { b.x, a.y, b.z }, { 1, 0, 0 });
}

void appendBoxEdges(std::vector<LineInstance>& out, const glm::dvec3& lo, const glm::dvec3& hi,
    const glm::vec4& color, float width)
{
    std::array<glm::vec3, 8> c {};
    for (std::size_t i = 0; i < 8; ++i) {
        c[i] = { (i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z };
    }
    constexpr std::array<std::pair<int, int>, 12> edges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 },
        { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
    for (const auto& [i, j] : edges) {
        out.push_back({ c[static_cast<std::size_t>(i)], c[static_cast<std::size_t>(j)], color, { width, 0.0f } });
    }
}

} // namespace

Viewport::Viewport(QQuickItem* parent)
    : QQuickRhiItem(parent)
{
    setSampleCount(4);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setFlag(ItemIsFocusScope, false);
    m_frameClock.start();
    startTimer(1000);

    connect(this, &Viewport::appearanceChanged, this, &QQuickItem::update);
}

Viewport::~Viewport() = default;

QQuickRhiItemRenderer* Viewport::createRenderer()
{
    // Qt preia renderer-ul și îl distruge pe firul de randare.
    return new ViewportRenderer; // NOLINT(cppcoreguidelines-owning-memory)
}

bool Viewport::isOrthographic() const
{
    return m_camera.projection() == view::Projection::Orthographic;
}

void Viewport::setOrthographic(bool orthographic)
{
    if (orthographic == isOrthographic()) {
        return;
    }
    m_camera.setProjection(orthographic ? view::Projection::Orthographic : view::Projection::Perspective);
    onCameraMoved();
}

void Viewport::setReducedMotion(bool reduced)
{
    if (reduced == m_reducedMotion) {
        return;
    }
    m_reducedMotion = reduced;
    m_motion.setReducedMotion(reduced);
    emit reducedMotionChanged();
}

QVector3D Viewport::cameraRight() const
{
    return toQt(m_camera.right());
}

QVector3D Viewport::cameraUp() const
{
    return toQt(m_camera.up());
}

QVector3D Viewport::cameraForward() const
{
    return toQt(m_camera.forward());
}

void Viewport::setStandardView(StandardView v)
{
    const auto target = view::standardViewPose(static_cast<view::StandardView>(v), m_camera.pose());
    m_motion.animateTo(m_camera, target);
    requestAnimationFrame();
}

void Viewport::zoomExtents()
{
    view::Vec3d lo;
    view::Vec3d hi;
    sceneBounds(lo, hi);
    m_motion.animateTo(m_camera, m_camera.framing(lo, hi));
    requestAnimationFrame();
}

void Viewport::toggleProjection()
{
    setOrthographic(!isOrthographic());
}

void Viewport::startTurntable(double seconds)
{
    m_motion.stop();
    m_turntableSecondsLeft = seconds;
    requestAnimationFrame();
}

void Viewport::sceneBounds(view::Vec3d& min, view::Vec3d& max) const
{
    if (m_demoVisible) {
        min = kDemoMin;
        max = kDemoMax;
    } else {
        // Model gol: o zonă de 10 × 10 m în jurul originii.
        min = { -5000.0, -5000.0, 0.0 };
        max = { 5000.0, 5000.0, 0.0 };
    }
}

ViewportFrame Viewport::frameForRendering() const
{
    ViewportFrame f;
    f.view = m_camera.viewMatrix();
    f.projection = m_camera.projectionMatrix();
    f.eye = m_camera.eye();
    f.forward = m_camera.forward();
    f.targetDistance = m_camera.pose().distance;
    f.orthographic = isOrthographic();
    f.gridVisible = m_gridVisible;
    f.devicePixelRatio = window() ? static_cast<float>(window()->effectiveDevicePixelRatio()) : 1.0f;

    f.colors.backgroundTop = toVec4(m_backgroundTop);
    f.colors.backgroundBottom = toVec4(m_backgroundBottom);
    f.colors.gridMinor = toVec4(m_gridMinor);
    f.colors.gridMajor = toVec4(m_gridMajor);
    f.colors.faceFront = toVec4(m_faceFront);
    f.colors.faceBack = toVec4(m_faceBack);

    // Axele: partea pozitivă continuă, cea negativă punctată și mai discretă.
    const auto axisLength = static_cast<float>(m_camera.farPlane() + glm::length(m_camera.pose().target));
    constexpr float axisWidth = 1.5f;
    constexpr float dash = 4.0f;
    const std::array<std::pair<glm::vec3, QColor>, 3> axes { {
        { { 1, 0, 0 }, m_axisX },
        { { 0, 1, 0 }, m_axisY },
        { { 0, 0, 1 }, m_axisZ },
    } };
    for (const auto& [dir, color] : axes) {
        f.lines.push_back({ { 0, 0, 0 }, dir * axisLength, toVec4(color), { axisWidth, 0.0f } });
        f.lines.push_back({ { 0, 0, 0 }, -dir * axisLength, withAlpha(color, 0.55f), { axisWidth, dash } });
    }

    if (m_demoVisible) {
        appendBox(f.triangles, kDemoMin, kDemoMax);
        appendBoxEdges(f.lines, kDemoMin, kDemoMax, toVec4(m_edge), 1.25f);
    }
    f.meshRevision = m_demoVisible ? 1u : 2u;

    f.points.push_back({ { 0, 0, 0 }, toVec4(m_axisZ.lighter(130)), 5.0f });
    return f;
}

void Viewport::onCameraMoved()
{
    emit cameraChanged();
    update();
}

void Viewport::requestAnimationFrame()
{
    if (!m_ticking) {
        m_ticking = true;
        m_lastTickNs = m_frameClock.nsecsElapsed();
    }
    update();
}

void Viewport::onAfterAnimating()
{
    if (!m_ticking) {
        return;
    }
    const qint64 now = m_frameClock.nsecsElapsed();
    // Limităm pasul, ca după o pauză (de exemplu fereastra ascunsă) să nu sară camera.
    const double dt = std::min(static_cast<double>(now - m_lastTickNs) * 1e-9, 1.0 / 20.0);
    m_lastTickNs = now;

    if (m_motion.update(m_camera, dt)) {
        onCameraMoved();
    }
    if (m_turntableSecondsLeft > 0.0) {
        constexpr double kTurntableRadiansPerSecond = 0.8;
        m_camera.orbit(kTurntableRadiansPerSecond * dt, 0.0);
        m_turntableSecondsLeft -= dt;
        onCameraMoved();
    }
    if (m_motion.isActive() || m_turntableSecondsLeft > 0.0) {
        update();
    } else {
        m_ticking = false;
    }
}

void Viewport::itemChange(ItemChange change, const ItemChangeData& data)
{
    if (change == ItemSceneChange && data.window != nullptr) {
        connect(data.window, &QQuickWindow::afterAnimating, this, &Viewport::onAfterAnimating);
        connect(data.window, &QQuickWindow::frameSwapped, this,
            [this] { m_framesThisSecond.fetch_add(1, std::memory_order_relaxed); }, Qt::DirectConnection);
    }
    QQuickRhiItem::itemChange(change, data);
}

void Viewport::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickRhiItem::geometryChange(newGeometry, oldGeometry);
    m_camera.setViewportSize(newGeometry.width(), newGeometry.height());
    onCameraMoved();
}

void Viewport::timerEvent(QTimerEvent* /*event*/)
{
    const int fps = m_framesThisSecond.exchange(0, std::memory_order_relaxed);
    if (fps != m_fps) {
        m_fps = fps;
        emit framesPerSecondChanged();
    }
}

bool Viewport::event(QEvent* event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        const QPointF pos = gesture->position();
        switch (gesture->gestureType()) {
        case Qt::ZoomNativeGesture: {
            // value() e variația relativă a distanței dintre degete (pozitivă = depărtare = apropiere).
            const double factor = 1.0 / std::max(0.05, 1.0 + gesture->value());
            m_motion.stop();
            m_camera.zoomAbout(factor, m_camera.zoomAnchorAt(toVec2(pos)));
            onCameraMoved();
            gesture->accept();
            return true;
        }
        case Qt::SmartZoomNativeGesture:
            zoomExtents();
            gesture->accept();
            return true;
        default:
            break;
        }
    }
    return QQuickRhiItem::event(event);
}

void Viewport::beginDrag(Drag drag, QPointF position)
{
    m_drag = drag;
    m_lastPointer = position;
    m_orbitSamples.clear();
    m_motion.stop();
}

void Viewport::mousePressEvent(QMouseEvent* event)
{
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (event->button() == Qt::MiddleButton) {
        beginDrag(shift ? Drag::Pan : Drag::Orbit, event->position());
    } else if (event->button() == Qt::LeftButton && m_navigationTool == QLatin1String("orbit")) {
        beginDrag(shift ? Drag::Pan : Drag::Orbit, event->position());
    } else if (event->button() == Qt::LeftButton && m_navigationTool == QLatin1String("pan")) {
        beginDrag(Drag::Pan, event->position());
    } else {
        event->ignore();
        return;
    }
    setCursor(m_drag == Drag::Pan ? Qt::ClosedHandCursor : Qt::SizeAllCursor);
    event->accept();
}

void Viewport::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF delta = event->position() - m_lastPointer;
    m_lastPointer = event->position();
    m_hoverPosition = event->position();

    if (m_drag == Drag::Orbit) {
        m_camera.orbit(-delta.x() * kOrbitRadiansPerPixel, -delta.y() * kOrbitRadiansPerPixel);
        recordOrbitSample(delta);
        onCameraMoved();
    } else if (m_drag == Drag::Pan) {
        m_camera.pan(toVec2(delta));
        onCameraMoved();
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drag == Drag::Orbit) {
        m_motion.setOrbitVelocity(orbitReleaseVelocity());
        requestAnimationFrame();
    }
    m_drag = Drag::None;
    unsetCursor();
    event->accept();
}

void Viewport::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Dublu-click cu butonul din mijloc centrează vederea pe punctul de sub cursor (ca în SketchUp).
    if (event->button() == Qt::MiddleButton) {
        auto pose = m_camera.pose();
        pose.target = m_camera.zoomAnchorAt(toVec2(event->position()));
        m_motion.animateTo(m_camera, pose);
        requestAnimationFrame();
        event->accept();
        return;
    }
    event->ignore();
}

void Viewport::recordOrbitSample(QPointF delta)
{
    const qint64 now = m_frameClock.nsecsElapsed();
    m_orbitSamples.push_back({ now, delta });
    while (!m_orbitSamples.empty() && now - m_orbitSamples.front().timeNs > kInertiaWindowNs) {
        m_orbitSamples.pop_front();
    }
}

view::Vec2d Viewport::orbitReleaseVelocity() const
{
    const qint64 now = m_frameClock.nsecsElapsed();
    if (m_orbitSamples.size() < 2 || now - m_orbitSamples.back().timeNs > kInertiaMaxIdleNs) {
        return { 0.0, 0.0 };
    }
    QPointF total;
    for (const auto& s : m_orbitSamples) {
        total += s.delta;
    }
    const double seconds
        = std::max(static_cast<double>(now - m_orbitSamples.front().timeNs) * 1e-9, 1.0 / 120.0);
    return { -total.x() / seconds * kOrbitRadiansPerPixel, -total.y() / seconds * kOrbitRadiansPerPixel };
}

void Viewport::wheelEvent(QWheelEvent* event)
{
    const QPointF pos = event->position();
    m_hoverPosition = pos;
    const auto mods = event->modifiers();
    // Trackpad-ul trimite evenimente cu fază (început, mișcare, inerție); rotița clasică nu.
    const bool trackpad = event->phase() != Qt::NoScrollPhase
        || event->pointingDevice()->type() == QInputDevice::DeviceType::TouchPad;

    if (trackpad && !mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::MetaModifier)) {
        m_motion.stop();
        const QPointF d = event->pixelDelta();
        if (mods.testFlag(Qt::ShiftModifier)) {
            m_camera.pan(toVec2(d));
        } else {
            m_camera.orbit(-d.x() * kOrbitRadiansPerPixel, -d.y() * kOrbitRadiansPerPixel);
        }
        onCameraMoved();
        event->accept();
        return;
    }

    double steps = event->angleDelta().y() / 120.0;
    if (std::abs(steps) < 1e-9) {
        steps = event->pixelDelta().y() / 50.0;
    }
    if (std::abs(steps) < 1e-9) {
        event->ignore();
        return;
    }
    const double factor = std::pow(kWheelZoomStep, steps);
    m_motion.zoomSmoothly(factor, m_camera.zoomAnchorAt(toVec2(pos)));
    requestAnimationFrame();
    event->accept();
}

void Viewport::hoverMoveEvent(QHoverEvent* event)
{
    m_hoverPosition = event->position();
    QQuickRhiItem::hoverMoveEvent(event);
}

} // namespace forma::render
