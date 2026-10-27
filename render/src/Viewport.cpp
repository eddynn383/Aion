#include "aion/render/Viewport.h"

#include "aion/render/Document.h"

#include "aion/core/model/Operations.h"
#include "aion/core/model/Texturing.h"
#include "aion/core/units/Length.h"

#include "ViewportFrame.h"
#include "ViewportRenderer.h"

#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <QBuffer>
#include <QDir>
#include <QKeyEvent>
#include <QKeySequence>
#include <QGuiApplication>
#include <QLineF>
#include <QQuickWindow>
#include <QStyleHints>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

namespace aion::render {

namespace {

// Radiani de rotație pe pixel de mișcare a mouse-ului (sau a degetelor pe trackpad).
constexpr double kOrbitRadiansPerPixel = 0.008;
// Factorul de zoom pentru o „treaptă” de rotiță (120 de unități).
constexpr double kWheelZoomStep = 0.85;
// Zoom prin tragere: factorul e exp(pixeli · k); în sus = apropiere (ca în Blender).
constexpr double kDragZoomPerPixel = 0.006;
constexpr int kTransientCursorMs = 450;
// Cât de recentă trebuie să fie mișcarea ca, la eliberare, orbita să continue prin inerție.
constexpr qint64 kInertiaWindowNs = 80'000'000;
constexpr qint64 kInertiaMaxIdleNs = 40'000'000;
// Clicurile succesive mai apropiate de atât (în pixeli) se numără împreună (dublu, triplu).
constexpr double kMultiClickDistancePx = 5.0;

constexpr float kEdgeWidth = 1.25f;
constexpr float kHighlightedEdgeWidth = 2.25f;
// Profilele: muchiile de contur ale obiectelor (silueta), mai groase.
constexpr float kProfileWidth = 2.6f;

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

// Normala într-un vârf al feței: media fețelor legate de ea prin muchii soft în jurul vârfului
// (umbrirea netedă a suprafețelor curbe); normala feței dacă vârful nu are muchii soft.
glm::dvec3 smoothNormal(const geom::Mesh& mesh, geom::FaceId face, geom::VertexId vertex)
{
    const geom::Vertex& v = mesh.vertex(vertex);
    const auto isSoft = [&](geom::EdgeId e) { return mesh.edge(e).soft; };
    if (std::none_of(v.edges.begin(), v.edges.end(), isSoft)) {
        return mesh.face(face).plane.normal;
    }
    std::vector<geom::FaceId> group { face };
    for (bool grew = true; grew;) {
        grew = false;
        for (geom::EdgeId id : v.edges) {
            const geom::Edge& e = mesh.edge(id);
            if (!e.soft || std::none_of(e.faces.begin(), e.faces.end(), [&](geom::FaceId f) {
                    return std::find(group.begin(), group.end(), f) != group.end();
                })) {
                continue;
            }
            for (geom::FaceId f : e.faces) {
                if (std::find(group.begin(), group.end(), f) == group.end()) {
                    group.push_back(f);
                    grew = true;
                }
            }
        }
    }
    glm::dvec3 sum { 0.0 };
    for (geom::FaceId f : group) {
        sum += mesh.face(f).plane.normal;
    }
    return glm::length(sum) > 1e-12 ? glm::normalize(sum) : mesh.face(face).plane.normal;
}

int styleIndex(const QString& style)
{
    if (style == QLatin1String("clay")) {
        return 1;
    }
    if (style == QLatin1String("hiddenline")) {
        return 2;
    }
    if (style == QLatin1String("wireframe")) {
        return 3;
    }
    if (style == QLatin1String("xray")) {
        return 4;
    }
    return 0;
}

// Poziția soarelui (model simplu): declinația după lună, unghiul orar după oră. Nordul e +Y, estul +X.
// Întoarce direcția spre soare și intensitatea (0 noaptea, crește lin la răsărit).
std::pair<glm::dvec3, double> sunPosition(double hour, int month, double latitudeDeg)
{
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    const double day = (month - 1) * 30.4 + 15.0;
    const double declination = 23.44 * kDeg * std::sin(2.0 * 3.14159265358979323846 * (284.0 + day) / 365.0);
    const double hourAngle = 15.0 * kDeg * (hour - 12.0);
    const double lat = latitudeDeg * kDeg;
    const double sinElevation
        = std::sin(lat) * std::sin(declination) + std::cos(lat) * std::cos(declination) * std::cos(hourAngle);
    const double elevation = std::asin(std::clamp(sinElevation, -1.0, 1.0));
    const double azimuth = std::atan2(-std::sin(hourAngle),
        std::tan(declination) * std::cos(lat) - std::sin(lat) * std::cos(hourAngle)); // de la nord, spre est
    const double e = std::max(elevation, 3.0 * kDeg); // sub orizont, lumina rămâne joasă (dar fără umbre lungi absurde)
    const glm::dvec3 dir { std::sin(azimuth) * std::cos(e), std::cos(azimuth) * std::cos(e), std::sin(e) };
    const double intensity = std::clamp((elevation / kDeg + 2.0) / 12.0, 0.0, 1.0);
    return { glm::normalize(dir), intensity };
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

    m_transientCursorTimer.setSingleShot(true);
    m_transientCursorTimer.setInterval(kTransientCursorMs);
    connect(&m_transientCursorTimer, &QTimer::timeout, this, &Viewport::updateCursor);
}

void Viewport::setNavigationTool(const QString& tool)
{
    if (tool == m_navigationTool) {
        return;
    }
    m_navigationTool = tool;
    emit navigationToolChanged();
    updateCursor();
}

void Viewport::setNavigationCursors(const QCursor& orbit, const QCursor& zoom)
{
    m_orbitCursor = orbit;
    m_zoomCursor = zoom;
    updateCursor();
}

void Viewport::setPaintCursor(const QCursor& paint)
{
    m_paintCursor = paint;
    updateCursor();
}

void Viewport::showTransientCursor(const QCursor& cursor)
{
    m_transientCursor = cursor;
    m_transientCursorTimer.start();
    updateCursor();
}

void Viewport::updateCursor()
{
    switch (m_drag) {
    case Drag::Orbit:
        setCursor(m_orbitCursor);
        return;
    case Drag::Pan:
        setCursor(Qt::ClosedHandCursor);
        return;
    case Drag::Zoom:
        setCursor(m_zoomCursor);
        return;
    case Drag::None:
        break;
    }
    if (m_transientCursorTimer.isActive()) {
        setCursor(m_transientCursor);
    } else if (m_navigationTool == QLatin1String("orbit")) {
        setCursor(m_orbitCursor);
    } else if (m_navigationTool == QLatin1String("pan")) {
        setCursor(Qt::OpenHandCursor);
    } else if (m_navigationTool == QLatin1String("zoom")) {
        setCursor(m_zoomCursor);
    } else if (m_navigationTool == QLatin1String("interact")) {
        setCursor(Qt::PointingHandCursor);
    } else if (m_navigationTool.isEmpty() && m_document && m_document->activeTool() == QLatin1String("paint")) {
        setCursor(m_paintCursor);
    } else {
        unsetCursor();
    }
}

Viewport::~Viewport() = default;

void Viewport::setDocument(Document* document)
{
    if (document == m_document) {
        return;
    }
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = document;
    if (m_document) {
        // Și schimbările care nu vin din viewport (tastele, Measurements box) mută etichetele.
        connect(m_document, &Document::changed, this, &Viewport::afterToolEvent);
        connect(m_document, &Document::changed, this, &Viewport::updateSceneBounds);
        connect(m_document, &Document::gridSnapChanged, this, [this] { update(); });
        connect(m_document, &Document::activeToolChanged, this, &Viewport::updateCursor);
        connect(m_document, &Document::animationStarted, this, &Viewport::requestAnimationFrame);
        connect(m_document, &Document::timelineTimeChanged, this, &Viewport::applyAnimationCamera);
        connect(m_document, &Document::cameraRequested, this, [this](const QVariantMap& camera) { animateToCamera(camera, 1.0); });
        connect(m_document, &Document::interactionModeChanged, this, [this] {
            if (!m_document->interactionMode()) {
                updateCursor();
            }
        });
        // Un document deschis sau nou: îl încadrăm.
        connect(m_document, &Document::documentReplaced, this, [this] {
            updateSceneBounds();
            zoomExtents();
        });
    }
    updateSceneBounds();
    emit documentChanged();
    update();
}

tools::PointerEvent Viewport::toolEvent(QPointF position, Qt::KeyboardModifiers modifiers, int clickCount) const
{
    tools::PointerEvent e;
    e.screen = toVec2(position);
    e.modifiers.shift = modifiers.testFlag(Qt::ShiftModifier);
    // Qt pe macOS: Cmd = ControlModifier, Control = MetaModifier.
    e.modifiers.command = modifiers.testFlag(Qt::ControlModifier);
    e.modifiers.alt = modifiers.testFlag(Qt::AltModifier);
    e.clickCount = clickCount;
    return e;
}

void Viewport::afterToolEvent()
{
    QRectF rect;
    bool crossing = false;
    if (m_document && m_document->tool()) {
        const tools::Overlay overlay = m_document->tool()->overlay();
        if (overlay.selectionRect) {
            const auto [a, b] = *overlay.selectionRect;
            rect = QRectF(QPointF(a.x, a.y), QPointF(b.x, b.y)).normalized();
            crossing = overlay.crossing;
        }
    }
    if (rect != m_selectionRect || crossing != m_selectionCrossing) {
        m_selectionRect = rect;
        m_selectionCrossing = crossing;
        emit selectionRectChanged();
    }
    updateAnnotations();
    update();
}

void Viewport::appendDocument(ViewportFrame& f) const
{
    const Document& doc = *m_document;
    // Imaginile și videoclipurile exportate nu arată hover-ul, selecția sau ce desenează unealta.
    const view::PickResult hover = doc.tool() && !m_cleanCapture ? doc.tool()->hover() : view::PickResult {};
    const tools::Overlay overlay = doc.tool() && !m_cleanCapture ? doc.tool()->overlay() : tools::Overlay {};
    appendDefinition(f, doc, doc.model().root(), math::Transform {}, 0, {}, hover, overlay);

    // Overlay-ul uneltei e în coordonatele contextului editat.
    const math::Transform ctx = doc.contextTransform();
    const auto w = [&](const math::Vec3d& p) { return glm::vec3(ctx.applyToPoint(p)); };

    // Grupul editat: cutia lui, punctată (ca în SketchUp).
    if (!doc.path().empty()) {
        const auto [lo, hi] = model::bounds(doc.model(), doc.activeDefinition());
        if (lo.x <= hi.x) {
            std::array<glm::vec3, 8> c {};
            for (std::size_t k = 0; k < 8; ++k) {
                c[k] = w({ (k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z });
            }
            constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 },
                { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
            for (const auto& [a, b] : kEdges) {
                f.lines.push_back({ c[std::size_t(a)], c[std::size_t(b)], withAlpha(m_preview, 0.55f), { 1.0f, 4.0f } });
            }
        }
    }

    const glm::vec4 accent = toVec4(m_accent);
    const glm::vec4 preview = toVec4(m_preview);
    for (const tools::OverlayLine& l : overlay.lines) {
        if (l.onTop) {
            f.topLines.push_back({ w(l.a), w(l.b), tintColor(l.tint), { 1.75f, l.dashed ? 4.0f : 0.0f } });
        } else {
            f.lines.push_back({ w(l.a), w(l.b), preview, { 1.5f, l.dashed ? 5.0f : 0.0f } });
        }
    }
    for (const math::Vec3d& p : overlay.points) {
        f.points.push_back({ w(p), accent, 7.0f });
    }
    // Marcajul inferenței: un disc colorat cu margine în culoarea fundalului, peste tot restul.
    if (overlay.marker) {
        const glm::vec3 p = w(overlay.marker->point);
        if (overlay.marker->kind == inference::Kind::Ground) {
            f.topPoints.push_back({ p, preview, 6.0f });
        } else {
            f.topPoints.push_back({ p, withAlpha(m_backgroundBottom, 0.9f), 13.0f });
            f.topPoints.push_back({ p, markerColor(overlay.marker->kind, overlay.marker->tint), 9.0f });
        }
    }
    if (hover.kind == view::PickResult::Kind::Vertex) {
        f.points.push_back({ w(hover.point), accent, 9.0f });
    }
}

void Viewport::appendDefinition(ViewportFrame& f, const Document& doc, model::DefinitionId definition,
    const math::Transform& toWorld, int pathIndex, DrawStyle style, const view::PickResult& hover,
    const tools::Overlay& overlay) const
{
    const model::Model& model = doc.model();
    const geom::Mesh& mesh = model.mesh(definition);
    const auto& path = doc.path();
    const bool active = pathIndex == static_cast<int>(path.size());
    const bool ancestor = pathIndex >= 0 && !active;
    // Geometria proprie a unei definiții de pe calea contextului (dar nu a contextului) e estompată.
    const bool dim = style.dim || ancestor;
    static const geom::Selection kNoSelection;
    const geom::Selection& selection = m_cleanCapture ? kNoSelection : doc.selection();

    const auto stateOf = [&](geom::FaceId faceId) {
        if (active) {
            return selection.contains(faceId) ? 2.0f
                : (hover.kind == view::PickResult::Kind::Face && hover.face == faceId) ? 1.0f
                                                                                       : 0.0f;
        }
        return dim ? 3.0f : style.selected || style.marked ? 2.0f : style.hovered ? 1.0f : 0.0f;
    };
    const int displayStyle = styleIndex(m_displayStyle);
    for (const auto& [faceId, tri] : doc.triangles(definition)) {
        if (displayStyle == 3) {
            break; // wireframe: fără fețe
        }
        const geom::Face* face = mesh.faces().find(faceId);
        if (face == nullptr || view::isHidden(*face)) {
            continue;
        }
        const float state = stateOf(faceId);
        f.hasSelectedFaces = f.hasSelectedFaces || (state > 1.5f && state < 2.5f);
        // Materialul feței, sau al grupului care o conține.
        const model::MaterialId materialId = face->material ? face->material : style.material;
        const model::Material* material = materialId ? model.materials().find(materialId) : nullptr;
        glm::vec4 color { 0.0f, 0.0f, 0.0f, -1.0f };
        glm::vec2 surface { 0.85f, 0.0f };
        if (material != nullptr) {
            color = { material->color.r / 255.0f, material->color.g / 255.0f, material->color.b / 255.0f,
                material->color.a / 255.0f };
            surface = { material->roughness, material->metallic };
        }
        if (style.colorOverride) {
            color = *style.colorOverride;
        }
        glm::vec4 backColor { 0.0f, 0.0f, 0.0f, -1.0f };
        if (const model::Material* back = face->backMaterial ? model.materials().find(face->backMaterial) : nullptr) {
            backColor = { back->color.r / 255.0f, back->color.g / 255.0f, back->color.b / 255.0f, back->color.a / 255.0f };
        }
        // Textura: stratul ei în array și mărimea reală (UV-ul se calculează în coordonatele definiției).
        float layer = -1.0f;
        if (material != nullptr && material->texture && !style.colorOverride) {
            if (const auto it = m_textureLayers.find(material->texture.value); it != m_textureLayers.end()) {
                layer = static_cast<float>(it->second);
            }
        }
        const bool transparent = displayStyle == 4 || (material != nullptr && displayStyle == 0 && color.a < 0.999f);
        std::vector<MeshVertex>& target = transparent ? f.transparent : f.triangles;
        // Punctele triangulării sunt vârfurile buclelor, în ordine (exterioara, apoi găurile).
        std::vector<geom::VertexId> ids = mesh.loopVertices(face->outer);
        for (const geom::Loop& h : face->inner) {
            const auto hv = mesh.loopVertices(h);
            ids.insert(ids.end(), hv.begin(), hv.end());
        }
        for (std::uint32_t i : tri.indices) {
            const glm::dvec3 n = i < ids.size() ? smoothNormal(mesh, faceId, ids[i]) : face->plane.normal;
            glm::vec3 uv { 0.0f, 0.0f, -1.0f };
            if (layer >= 0.0f) {
                const math::Vec2d t = model::textureCoordinates(face->plane.normal, tri.points[i], face->texture,
                    material->textureSize);
                uv = { static_cast<float>(t.x), static_cast<float>(-t.y), layer };
            }
            target.push_back({ glm::vec3(toWorld.applyToPoint(tri.points[i])),
                glm::vec3(glm::normalize(toWorld.applyToNormal(n))), state, color, surface, backColor, uv });
        }
    }

    const std::set<geom::EdgeId> marked(overlay.markedEdges.begin(), overlay.markedEdges.end());
    const glm::vec4 accent = toVec4(m_accent);
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const geom::EdgeId id = mesh.edges().keys()[i];
        const geom::Edge& e = mesh.edges().values()[i];
        if (view::isHidden(e)) {
            continue;
        }
        glm::vec4 color = e.faces.empty() ? toVec4(m_freeEdge) : toVec4(m_edge);
        float width = kEdgeWidth;
        const bool isMarked = active ? marked.contains(id) : style.marked;
        const bool isSelected = active ? selection.contains(id) : style.selected;
        const bool isHovered
            = active ? (hover.kind == view::PickResult::Kind::Edge && hover.edge == id) : style.hovered;
        if (e.soft && !(isMarked || (active && (isSelected || isHovered)))) {
            continue; // muchiile soft (laturile unui cilindru) nu se desenează
        }
        // Profil: muchia de margine a unei suprafețe sau cea dintre o față văzută din față și una din spate.
        if (m_profiles && displayStyle != 3 && !e.faces.empty()) {
            bool profile = e.faces.size() == 1;
            if (e.faces.size() == 2) {
                const math::Vec3d mid = toWorld.applyToPoint((mesh.position(e.v0) + mesh.position(e.v1)) * 0.5);
                const math::Vec3d toEye
                    = isOrthographic() ? -m_camera.forward() : glm::normalize(m_camera.eye() - mid);
                const auto facing = [&](geom::FaceId fid) {
                    return glm::dot(toWorld.applyToNormal(mesh.face(fid).plane.normal), toEye) > 0.0;
                };
                profile = facing(e.faces[0]) != facing(e.faces[1]);
            }
            if (profile) {
                width = kProfileWidth;
            }
        }
        if (isMarked) {
            color = toVec4(m_erase);
            width = kHighlightedEdgeWidth;
        } else if (isSelected) {
            color = accent;
            width = active ? kHighlightedEdgeWidth : kEdgeWidth;
        } else if (isHovered) {
            color = withAlpha(m_accent, 0.75f);
            width = active ? kHighlightedEdgeWidth : kEdgeWidth;
        } else if (dim) {
            color.a *= 0.3f;
        }
        glm::dvec3 p0 = toWorld.applyToPoint(mesh.position(e.v0));
        glm::dvec3 p1 = toWorld.applyToPoint(mesh.position(e.v1));
        if (f.section) {
            // Secțiunea taie muchia: rămâne doar partea din spatele planului.
            const glm::dvec3 n(*f.section);
            const double d0 = glm::dot(n, p0) - f.section->w;
            const double d1 = glm::dot(n, p1) - f.section->w;
            if (d0 > 0.0 && d1 > 0.0) {
                continue;
            }
            if (d0 > 0.0) {
                p0 = p0 + (p1 - p0) * (d0 / (d0 - d1));
            } else if (d1 > 0.0) {
                p1 = p1 + (p0 - p1) * (d1 / (d1 - d0));
            }
        }
        f.lines.push_back({ glm::vec3(p0), glm::vec3(p1), color, { width, 0.0f } });
    }

    // Liniile de tăietură ale secțiunii: unde planul trece prin fețe.
    if (f.section && displayStyle != 3) {
        const glm::dvec3 n(*f.section);
        const glm::vec4 cutColor = toVec4(m_edge);
        for (const auto& [faceId, tri] : doc.triangles(definition)) {
            for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
                std::array<glm::dvec3, 3> p {};
                std::array<double, 3> d {};
                for (std::size_t k = 0; k < 3; ++k) {
                    p[k] = toWorld.applyToPoint(tri.points[tri.indices[i + k]]);
                    d[k] = glm::dot(n, p[k]) - f.section->w;
                }
                std::vector<glm::dvec3> crossing;
                for (std::size_t k = 0; k < 3; ++k) {
                    const std::size_t j = (k + 1) % 3;
                    if ((d[k] > 0.0) != (d[j] > 0.0)) {
                        crossing.push_back(p[k] + (p[j] - p[k]) * (d[k] / (d[k] - d[j])));
                    }
                }
                if (crossing.size() == 2) {
                    f.lines.push_back({ glm::vec3(crossing[0]), glm::vec3(crossing[1]), cutColor, { kProfileWidth, 0.0f } });
                }
            }
        }
    }

    // Instanțele din definiție.
    const std::set<model::InstanceId> markedInstances(overlay.markedInstances.begin(), overlay.markedInstances.end());
    for (model::InstanceId child : model.childrenOf(definition)) {
        if (!model.isVisible(child)) {
            continue;
        }
        const model::Instance& inst = model.instance(child);
        // O ușă în mișcare (unealta Interact) se desenează cu transformarea animată.
        const math::Transform childToWorld = toWorld * doc.displayTransform(child).value_or(inst.transform);
        if (ancestor && path[static_cast<std::size_t>(pathIndex)] == child) {
            DrawStyle inside;
            inside.material = inst.material ? inst.material : style.material;
            appendDefinition(f, doc, inst.definition, childToWorld, pathIndex + 1, inside, hover, overlay);
            continue;
        }
        DrawStyle childStyle = style;
        if (active) {
            childStyle.selected = selection.contains(child);
            childStyle.hovered = hover.kind == view::PickResult::Kind::Instance && hover.instance == child;
            childStyle.marked = markedInstances.contains(child);
        }
        if (inst.material) {
            childStyle.material = inst.material;
        }
        if (const auto animated = doc.displayColor(child)) {
            childStyle.colorOverride = glm::vec4(animated->r / 255.0f, animated->g / 255.0f, animated->b / 255.0f,
                animated->a / 255.0f);
        }
        childStyle.dim = dim;
        appendDefinition(f, doc, inst.definition, childToWorld, -1, childStyle, hover, overlay);
    }
}

glm::vec4 Viewport::tintColor(inference::Tint tint) const
{
    switch (tint) {
    case inference::Tint::X:
        return toVec4(m_axisX);
    case inference::Tint::Y:
        return toVec4(m_axisY);
    case inference::Tint::Z:
        return toVec4(m_axisZ);
    case inference::Tint::Magenta:
        return toVec4(m_inferenceMagenta);
    case inference::Tint::Neutral:
        break;
    }
    return withAlpha(m_preview, 0.8f);
}

// Culorile din SketchUp: capăt verde, mijloc cyan, pe muchie roșu, pe față albastru;
// direcțiile au culoarea axei sau magenta.
glm::vec4 Viewport::markerColor(inference::Kind kind, inference::Tint tint) const
{
    switch (kind) {
    case inference::Kind::Endpoint:
        return toVec4(m_axisY);
    case inference::Kind::Midpoint:
        return toVec4(m_inferenceCyan);
    case inference::Kind::OnEdge:
        return toVec4(m_axisX);
    case inference::Kind::OnFace:
        return toVec4(m_axisZ);
    case inference::Kind::OnAxis:
    case inference::Kind::Parallel:
    case inference::Kind::Perpendicular:
    case inference::Kind::FromPoint:
    case inference::Kind::Intersection:
        return tint == inference::Tint::Neutral ? toVec4(m_preview) : tintColor(tint);
    case inference::Kind::GuidePoint:
    case inference::Kind::OnGuide:
        return toVec4(m_inferenceMagenta);
    case inference::Kind::None:
    case inference::Kind::Ground:
        break;
    }
    return toVec4(m_preview);
}

void Viewport::updateAnnotations()
{
    QVariantList list;
    if (m_document && m_document->tool()) {
        const tools::Overlay overlay = m_document->tool()->overlay();
        constexpr double kMinDimensionPx = 36.0;
        constexpr double kDimensionOffsetPx = 14.0;
        const math::Transform ctx = m_document->contextTransform();
        for (const tools::Dimension& d : overlay.dimensions) {
            const auto a = m_camera.project(ctx.applyToPoint(d.a));
            const auto b = m_camera.project(ctx.applyToPoint(d.b));
            if (!a || !b || glm::length(*b - *a) < kMinDimensionPx) {
                continue;
            }
            // Eticheta stă lângă mijlocul segmentului, deplasată perpendicular (spre partea de sus a ecranului).
            const view::Vec2d dir = glm::normalize(*b - *a);
            view::Vec2d normal { -dir.y, dir.x };
            if (normal.y > 0.0) {
                normal = -normal;
            }
            const view::Vec2d p = (*a + *b) * 0.5 + normal * kDimensionOffsetPx;
            list.append(QVariantMap {
                { QStringLiteral("kind"), QStringLiteral("dimension") },
                { QStringLiteral("x"), p.x },
                { QStringLiteral("y"), p.y },
                { QStringLiteral("text"), QString::fromStdString(d.text) },
            });
        }
        if (overlay.marker && !overlay.marker->label.empty()) {
            if (const auto p = m_camera.project(ctx.applyToPoint(overlay.marker->point))) {
                const glm::vec4 c = markerColor(overlay.marker->kind, overlay.marker->tint);
                list.append(QVariantMap {
                    { QStringLiteral("kind"), QStringLiteral("inference") },
                    { QStringLiteral("x"), p->x },
                    { QStringLiteral("y"), p->y },
                    { QStringLiteral("text"), QString::fromStdString(overlay.marker->label) },
                    { QStringLiteral("color"), QColor::fromRgbF(c.r, c.g, c.b, c.a) },
                    { QStringLiteral("locked"), overlay.marker->locked },
                });
            }
        }
    }
    // Etichetele adnotărilor din model (cote și texte), editabile din QML.
    if (m_document) {
        const model::Model& model = m_document->model();
        for (std::size_t i = 0; i < model.annotations().size(); ++i) {
            const model::Annotation& a = model.annotations().values()[i];
            const math::Transform t = definitionToWorld(*m_document, a.context);
            math::Vec3d anchor = t.applyToPoint(a.a + a.offset);
            QString text = QString::fromStdString(a.text);
            if (a.kind == model::Annotation::Kind::Dimension) {
                anchor = (anchor + t.applyToPoint(a.b + a.offset)) * 0.5;
                if (text.isEmpty()) {
                    text = QString::fromStdString(units::formatLength(glm::length(t.applyToPoint(a.b) - t.applyToPoint(a.a))));
                }
            }
            if (const auto p = m_camera.project(anchor)) {
                list.append(QVariantMap {
                    { QStringLiteral("kind"), QStringLiteral("note") },
                    { QStringLiteral("id"), static_cast<double>(model.annotations().keys()[i].value) },
                    { QStringLiteral("x"), p->x },
                    { QStringLiteral("y"), p->y },
                    { QStringLiteral("text"), text },
                    { QStringLiteral("dimension"), a.kind == model::Annotation::Kind::Dimension },
                });
            }
        }
    }
    if (list != m_annotations) {
        m_annotations = list;
        emit annotationsChanged();
    }
}

void Viewport::setPointer(QPointF position, bool inside)
{
    if (position == m_hoverPosition && inside == m_pointerInside) {
        return;
    }
    m_hoverPosition = position;
    m_pointerInside = inside;
    emit pointerPositionChanged();
}

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

math::Transform Viewport::definitionToWorld(const Document& doc, model::DefinitionId definition) const
{
    const model::Model& model = doc.model();
    // Pe calea editată, transformarea e cea a căii.
    for (std::size_t i = 0; i < doc.path().size(); ++i) {
        if (model.instance(doc.path()[i]).definition == definition) {
            return model::worldTransform(model, { doc.path().begin(), doc.path().begin() + static_cast<std::ptrdiff_t>(i + 1) });
        }
    }
    math::Transform t;
    model::DefinitionId d = definition;
    for (int guard = 0; d != model.root() && guard < 64; ++guard) {
        const auto owners = model.instancesOf(d);
        if (owners.empty()) {
            break;
        }
        const model::Instance& inst = model.instance(owners.front());
        t = inst.transform * t;
        d = inst.parent;
    }
    return t;
}

void Viewport::appendModelExtras(ViewportFrame& f, const Document& doc) const
{
    const model::Model& model = doc.model();
    const double far = std::max(m_camera.pose().distance * 40.0, 100000.0);

    // Ghidajele: linii punctate lungi și puncte.
    const glm::vec4 guide = toVec4(m_guide);
    for (const model::Guide& g : model.guides().values()) {
        const math::Transform t = definitionToWorld(doc, g.context);
        const glm::dvec3 p = t.applyToPoint(g.point);
        if (g.isLine()) {
            const glm::dvec3 dir = glm::normalize(t.applyToVector(g.direction));
            f.lines.push_back({ glm::vec3(p - dir * far), glm::vec3(p + dir * far), guide, { 1.0f, 6.0f } });
        } else {
            f.points.push_back({ glm::vec3(p), guide, 7.0f });
        }
    }

    // Adnotările: liniile cotelor și ale textelor (etichetele le desenează QML).
    const glm::vec4 noteColor = withAlpha(m_preview, 0.75f);
    for (const model::Annotation& a : model.annotations().values()) {
        const math::Transform t = definitionToWorld(doc, a.context);
        const glm::vec3 pa(t.applyToPoint(a.a));
        const glm::vec3 po(t.applyToPoint(a.a + a.offset));
        if (a.kind == model::Annotation::Kind::Dimension) {
            const glm::vec3 pb(t.applyToPoint(a.b));
            const glm::vec3 pbo(t.applyToPoint(a.b + a.offset));
            f.lines.push_back({ pa, po, noteColor, { 1.0f, 0.0f } });
            f.lines.push_back({ pb, pbo, noteColor, { 1.0f, 0.0f } });
            f.lines.push_back({ po, pbo, noteColor, { 1.25f, 0.0f } });
            f.points.push_back({ po, noteColor, 5.0f });
            f.points.push_back({ pbo, noteColor, 5.0f });
        } else {
            f.lines.push_back({ pa, po, noteColor, { 1.0f, 0.0f } });
            f.points.push_back({ pa, noteColor, 6.0f });
        }
    }

    // Planurile de secțiune: un dreptunghi în plan, plin pentru cel activ.
    const glm::dvec3 center = (f.sceneMin + f.sceneMax) * 0.5;
    const double size = std::max(glm::length(f.sceneMax - f.sceneMin) * 0.6, 1000.0);
    for (const model::SectionPlane& s : model.sections().values()) {
        const glm::dvec3 n = glm::normalize(s.normal);
        const glm::dvec3 c = center - n * glm::dot(center - s.point, n);
        const glm::dvec3 u = glm::normalize(glm::cross(std::abs(n.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0), n));
        const glm::dvec3 v = glm::cross(n, u);
        const std::array<glm::dvec3, 4> corners { c + (u + v) * size, c + (u - v) * size, c - (u + v) * size,
            c - (u - v) * size };
        const glm::vec4 color = s.active ? toVec4(m_sectionColor) : withAlpha(m_sectionColor, 0.35f);
        for (std::size_t k = 0; k < 4; ++k) {
            f.topLines.push_back({ glm::vec3(corners[k]), glm::vec3(corners[(k + 1) % 4]), color,
                { s.active ? 1.5f : 1.0f, s.active ? 0.0f : 5.0f } });
        }
    }

    // Camerele și luminile: gizmo-uri (mărime constantă pe ecran; nu și în exporturi) și luminile pentru shader.
    const glm::vec4 gizmoColor = withAlpha(m_preview, 0.9f);
    const glm::vec4 selectedColor = toVec4(m_accent);
    const auto screenScale = [&](const glm::dvec3& p) {
        // ~1 px în mm la adâncimea punctului.
        const double depth = isOrthographic() ? m_camera.pose().distance
                                              : std::max(glm::dot(p - m_camera.eye(), m_camera.forward()), 1.0);
        return m_camera.worldPerPixelAtTarget() * depth / std::max(m_camera.pose().distance, 1e-6);
    };
    const auto basis = [](const glm::dvec3& dir) {
        const glm::dvec3 f = glm::normalize(dir);
        const glm::dvec3 r = glm::normalize(glm::cross(f, std::abs(f.z) > 0.99 ? glm::dvec3(1, 0, 0) : glm::dvec3(0, 0, 1)));
        return std::array<glm::dvec3, 3> { f, r, glm::cross(r, f) };
    };
    const auto line = [&](const glm::dvec3& a, const glm::dvec3& b, const glm::vec4& color, float width = 1.25f,
                          float dash = 0.0f) { f.lines.push_back({ glm::vec3(a), glm::vec3(b), color, { width, dash } }); };
    for (std::size_t i = 0; i < model.cameras().size(); ++i) {
        const model::CameraObject& c = model.cameras().values()[i];
        const bool selected = model.cameras().keys()[i] == doc.selectedCamera();
        if ((!c.visible && !selected) || m_cleanCapture) {
            continue;
        }
        const glm::vec4 color = selected ? selectedColor : gizmoColor;
        const glm::dvec3 eye = c.position;
        const double k = screenScale(eye) * 36.0;
        const auto [fw, r, u] = basis(c.target - c.position);
        const double h = std::tan(glm::radians(c.fov) * 0.5) * k;
        const double w = h * 1.5;
        const glm::dvec3 frameCenter = eye + fw * k;
        const std::array<glm::dvec3, 4> q { frameCenter + r * w + u * h, frameCenter - r * w + u * h, frameCenter - r * w - u * h,
            frameCenter + r * w - u * h };
        for (std::size_t j = 0; j < 4; ++j) {
            line(eye, q[j], color);
            line(q[j], q[(j + 1) % 4], color);
        }
        // Triunghiul de sus arată unde e „sus”.
        line(frameCenter - r * w * 0.4 + u * h * 1.15, frameCenter + u * h * 1.6, color);
        line(frameCenter + r * w * 0.4 + u * h * 1.15, frameCenter + u * h * 1.6, color);
        line(frameCenter - r * w * 0.4 + u * h * 1.15, frameCenter + r * w * 0.4 + u * h * 1.15, color);
        if (selected) {
            line(eye, c.target, withAlpha(m_accent, 0.6f), 1.0f, 6.0f);
            f.topPoints.push_back({ glm::vec3(c.target), withAlpha(m_accent, 0.8f), 6.0f });
        }
        f.topPoints.push_back({ glm::vec3(eye), color, selected ? 9.0f : 7.0f });
    }
    for (std::size_t i = 0; i < model.lights().size(); ++i) {
        const model::Light& l = model.lights().values()[i];
        const bool selected = model.lights().keys()[i] == doc.selectedLight();
        const QColor qc(l.color.r, l.color.g, l.color.b);
        if (l.enabled && static_cast<int>(f.lights.size()) < kMaxLights) {
            LightData d;
            d.kind = static_cast<int>(l.kind);
            d.position = glm::vec3(l.position);
            d.direction = glm::vec3(glm::normalize(l.direction));
            const auto lin = [](double c) { return static_cast<float>(std::pow(c, 2.2)); };
            d.color = glm::vec3(lin(qc.redF()), lin(qc.greenF()), lin(qc.blueF())) * static_cast<float>(l.intensity);
            d.range = static_cast<float>(l.range);
            const double half = glm::radians(l.cone) * 0.5;
            d.cosOuter = static_cast<float>(std::cos(half));
            d.cosInner = static_cast<float>(std::cos(half * (1.0 - std::clamp(l.softness, 0.0, 1.0))));
            d.size = static_cast<float>((l.width + l.height) * 0.5);
            f.lights.push_back(d);
        }
        if ((!l.visible && !selected) || m_cleanCapture) {
            continue;
        }
        const glm::vec4 color = selected ? selectedColor : withAlpha(qc, l.enabled ? 0.95f : 0.4f);
        const glm::dvec3 p = l.position;
        const double k = screenScale(p);
        const auto [fw, r, u] = basis(l.direction);
        switch (l.kind) {
        case model::Light::Kind::Point:
            for (const glm::dvec3& axis : { glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0), glm::dvec3(0, 0, 1),
                     glm::normalize(glm::dvec3(1, 1, 1)), glm::normalize(glm::dvec3(1, -1, 1)) }) {
                line(p - axis * k * 14.0, p + axis * k * 14.0, color);
            }
            break;
        case model::Light::Kind::Spot: {
            const double length = k * 60.0;
            const double radius = std::tan(glm::radians(l.cone) * 0.5) * length;
            glm::dvec3 previous {};
            for (int j = 0; j <= 16; ++j) {
                const double a = j * glm::two_pi<double>() / 16.0;
                const glm::dvec3 q = p + fw * length + (r * std::cos(a) + u * std::sin(a)) * radius;
                if (j > 0) {
                    line(previous, q, color);
                }
                if (j % 4 == 0 && j < 16) {
                    line(p, q, color);
                }
                previous = q;
            }
            break;
        }
        case model::Light::Kind::Directional:
            for (int j = -1; j <= 1; ++j) {
                const glm::dvec3 a = p + r * (j * k * 14.0);
                line(a, a + fw * k * 50.0, color);
                line(a + fw * k * 50.0, a + fw * k * 40.0 + r * k * 5.0, color);
                line(a + fw * k * 50.0, a + fw * k * 40.0 - r * k * 5.0, color);
            }
            break;
        case model::Light::Kind::Area: {
            const std::array<glm::dvec3, 4> q { p + r * l.width * 0.5 + u * l.height * 0.5,
                p - r * l.width * 0.5 + u * l.height * 0.5, p - r * l.width * 0.5 - u * l.height * 0.5,
                p + r * l.width * 0.5 - u * l.height * 0.5 };
            for (std::size_t j = 0; j < 4; ++j) {
                line(q[j], q[(j + 1) % 4], color, 1.5f);
            }
            line(p, p + fw * k * 40.0, color, 1.0f, 4.0f);
            break;
        }
        }
        f.topPoints.push_back({ glm::vec3(p), color, selected ? 11.0f : 9.0f });
    }
}

std::optional<std::pair<QString, double>> Viewport::objectHandleAt(QPointF position) const
{
    if (!m_document) {
        return std::nullopt;
    }
    const model::Model& model = m_document->model();
    std::optional<std::pair<QString, double>> best;
    double bestDistance = 12.0;
    const auto consider = [&](const math::Vec3d& p, const QString& kind, double id) {
        if (const auto screen = m_camera.project(p)) {
            const double d = glm::length(*screen - toVec2(position));
            if (d < bestDistance) {
                bestDistance = d;
                best = std::pair { kind, id };
            }
        }
    };
    for (std::size_t i = 0; i < model.cameras().size(); ++i) {
        if (model.cameras().values()[i].visible) {
            consider(model.cameras().values()[i].position, QStringLiteral("camera"),
                static_cast<double>(model.cameras().keys()[i].value));
        }
    }
    for (std::size_t i = 0; i < model.lights().size(); ++i) {
        if (model.lights().values()[i].visible) {
            consider(model.lights().values()[i].position, QStringLiteral("light"),
                static_cast<double>(model.lights().keys()[i].value));
        }
    }
    return best;
}

QVariantMap Viewport::cameraState() const
{
    const view::CameraPose& p = m_camera.pose();
    return QVariantMap { { QStringLiteral("tx"), p.target.x }, { QStringLiteral("ty"), p.target.y },
        { QStringLiteral("tz"), p.target.z }, { QStringLiteral("yaw"), p.yaw }, { QStringLiteral("pitch"), p.pitch },
        { QStringLiteral("distance"), p.distance }, { QStringLiteral("ortho"), isOrthographic() },
        { QStringLiteral("fov"), glm::degrees(m_camera.fovY()) } };
}

namespace {

view::CameraPose poseFromState(const QVariantMap& state)
{
    view::CameraPose pose;
    pose.target = { state.value(QStringLiteral("tx")).toDouble(), state.value(QStringLiteral("ty")).toDouble(),
        state.value(QStringLiteral("tz")).toDouble() };
    pose.yaw = state.value(QStringLiteral("yaw")).toDouble();
    pose.pitch = state.value(QStringLiteral("pitch")).toDouble();
    pose.distance = state.value(QStringLiteral("distance")).toDouble();
    return pose;
}

std::vector<view::CameraPose> posesFromStates(const QVariantList& states)
{
    std::vector<view::CameraPose> poses;
    for (const QVariant& s : states) {
        poses.push_back(poseFromState(s.toMap()));
    }
    return poses;
}

} // namespace

void Viewport::animateToCamera(const QVariantMap& state, double seconds)
{
    if (state.isEmpty()) {
        return;
    }
    setOrthographic(state.value(QStringLiteral("ortho")).toBool());
    // Vederea prin cameră folosește unghiul ei; celelalte (scene, vederi standard) pe cel implicit.
    m_camera.setFovY(state.contains(QStringLiteral("fov")) ? glm::radians(state.value(QStringLiteral("fov")).toDouble())
                                                           : view::Camera::kDefaultFovY);
    if (seconds >= 0.0) {
        m_motion.animateTo(m_camera, poseFromState(state), seconds);
    } else {
        m_motion.animateTo(m_camera, poseFromState(state));
    }
    requestAnimationFrame();
}

void Viewport::playScenes(const QVariantList& states, double transition, double delay, bool loop)
{
    if (states.isEmpty() || m_exporting) {
        return;
    }
    m_motion.stop();
    m_sceneAnimation.emplace(posesFromStates(states), transition, delay, loop);
    m_loopScenes = loop;
    m_sceneTime = 0.0;
    m_playingScene = 0;
    emit sceneAnimationChanged();
    requestAnimationFrame();
}

void Viewport::stopScenes()
{
    if (!m_sceneAnimation || m_exporting) {
        return;
    }
    m_sceneAnimation.reset();
    m_playingScene = -1;
    emit sceneAnimationChanged();
}

void Viewport::advanceSceneAnimation(double dt)
{
    m_sceneTime += dt;
    // Fără buclă, redarea se oprește la sfârșit; cu buclă, o luăm de la capăt.
    if (m_sceneTime >= m_sceneAnimation->duration()) {
        // (Animația cu buclă se termină pe prima scenă, de unde reîncepe.)
        m_camera.setPose(m_sceneAnimation->poseAt(m_sceneAnimation->duration()));
        onCameraMoved();
        if (m_sceneAnimation->duration() > 0.0 && m_loopScenes) {
            m_sceneTime = std::fmod(m_sceneTime, m_sceneAnimation->duration());
        } else {
            stopScenes();
            return;
        }
    }
    m_camera.setPose(m_sceneAnimation->poseAt(m_sceneTime));
    onCameraMoved();
    const int scene = static_cast<int>(m_sceneAnimation->sceneAt(m_sceneTime));
    if (scene != m_playingScene) {
        m_playingScene = scene;
        emit sceneAnimationChanged();
    }
}

void Viewport::exportScenes(const QVariantList& states, double transition, double delay, bool loop,
    const QString& directory, int fps)
{
    if (states.isEmpty() || m_exporting) {
        return;
    }
    QDir().mkpath(directory);
    m_motion.stop();
    m_sceneAnimation.emplace(posesFromStates(states), transition, delay, loop);
    m_exporting = true;
    m_exportFps = std::clamp(fps, 1, 120);
    m_exportFrame = 0;
    m_exportFrames = std::max(1, static_cast<int>(std::ceil(m_sceneAnimation->duration() * m_exportFps)) + 1);
    m_exportDirectory = directory;
    m_exportProgress = 0.0;
    m_playingScene = 0;
    emit sceneAnimationChanged();
    if (m_exportTimer == nullptr) {
        m_exportTimer = new QTimer(this);
        m_exportTimer->setSingleShot(true);
        m_exportTimer->setInterval(0);
        connect(m_exportTimer, &QTimer::timeout, this, &Viewport::exportNextFrame);
    }
    m_exportTimer->start();
}

void Viewport::applyAnimationCamera()
{
    if (!m_document || !m_document->previewing()) {
        return;
    }
    const QVariantMap camera = m_document->animationCamera();
    if (!camera.isEmpty()) {
        m_motion.stop();
        m_camera.setPose(poseFromState(camera));
        onCameraMoved();
    }
}

void Viewport::exportAnimation(const QString& directory, int fps)
{
    if (m_exporting || !m_document || m_document->currentAnimation() == nullptr) {
        return;
    }
    QDir().mkpath(directory);
    m_motion.stop();
    m_document->pause();
    m_exporting = true;
    m_exportTimeline = true;
    m_exportFps = std::clamp(fps, 1, 120);
    m_exportFrame = 0;
    m_exportFrames = std::max(1, static_cast<int>(std::ceil(m_document->animationDuration() * m_exportFps)) + 1);
    m_exportDirectory = directory;
    m_exportProgress = 0.0;
    emit sceneAnimationChanged();
    if (m_exportTimer == nullptr) {
        m_exportTimer = new QTimer(this);
        m_exportTimer->setSingleShot(true);
        m_exportTimer->setInterval(0);
        connect(m_exportTimer, &QTimer::timeout, this, &Viewport::exportNextFrame);
    }
    m_exportTimer->start();
}

void Viewport::exportNextFrame()
{
    if (!m_exporting || (!m_sceneAnimation && !m_exportTimeline)) {
        return;
    }
    const double t = static_cast<double>(m_exportFrame) / m_exportFps;
    if (m_exportTimeline) {
        m_document->setTimelineTime(t); // pozele obiectelor și camera animației
        applyAnimationCamera();
    } else {
        m_camera.setPose(m_sceneAnimation->poseAt(t));
        onCameraMoved();
        const int scene = static_cast<int>(m_sceneAnimation->sceneAt(t));
        if (scene != m_playingScene) {
            m_playingScene = scene; // QML schimbă stilul scenei înainte de randare
            emit sceneAnimationChanged();
        }
    }
    const QString file = QStringLiteral("%1/frame_%2.png").arg(m_exportDirectory).arg(m_exportFrame, 5, 10, QLatin1Char('0'));
    if (!saveImage(file)) {
        cancelExport();
        return;
    }
    ++m_exportFrame;
    m_exportProgress = static_cast<double>(m_exportFrame) / m_exportFrames;
    emit sceneAnimationChanged();
    if (m_exportFrame < m_exportFrames) {
        m_exportTimer->start();
        return;
    }
    const int frames = m_exportFrame;
    m_exporting = false;
    m_exportTimeline = false;
    m_sceneAnimation.reset();
    m_playingScene = -1;
    emit sceneAnimationChanged();
    emit animationExported(m_exportDirectory, frames, m_exportFps);
}

void Viewport::cancelExport()
{
    if (!m_exporting) {
        return;
    }
    m_exporting = false;
    m_exportTimeline = false;
    m_sceneAnimation.reset();
    m_playingScene = -1;
    emit sceneAnimationChanged();
    emit animationExported(m_exportDirectory, 0, m_exportFps);
}

QImage Viewport::renderImage()
{
    if (window() == nullptr) {
        return {};
    }
    // Elementele QML puse peste viewport (toolbar, gizmo, etichete) nu intră în imagine.
    QList<QQuickItem*> hidden;
    for (QQuickItem* child : childItems()) {
        if (child->isVisible()) {
            child->setVisible(false);
            hidden.append(child);
        }
    }
    // grabWindow sincronizează doar elementele marcate pentru actualizare: cerem un cadru nou în ambele stări.
    m_cleanCapture = true;
    update();
    const QImage frame = window()->grabWindow();
    m_cleanCapture = false;
    update();
    for (QQuickItem* child : hidden) {
        child->setVisible(true);
    }
    const qreal dpr = frame.devicePixelRatio() > 0 ? frame.devicePixelRatio() : window()->effectiveDevicePixelRatio();
    const QRectF scene = mapRectToScene(boundingRect());
    const QRect pixels(qRound(scene.x() * dpr), qRound(scene.y() * dpr), qRound(scene.width() * dpr),
        qRound(scene.height() * dpr));
    return frame.copy(pixels);
}

bool Viewport::saveImage(const QString& path)
{
    const QImage image = renderImage();
    return !image.isNull() && image.save(path, "PNG");
}

QByteArray Viewport::captureThumbnail()
{
    QImage image = renderImage();
    if (image.isNull()) {
        return {};
    }
    if (image.width() > 512) {
        image = image.scaledToWidth(512, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void Viewport::setOrbitPivot(int mode)
{
    mode = std::clamp(mode, 0, 2);
    if (mode != m_orbitPivotMode) {
        m_orbitPivotMode = mode;
        emit navigationSettingsChanged();
    }
}

void Viewport::setClipStart(double mm)
{
    m_clipStart = std::max(mm, 0.0);
    m_camera.setClipOverride(m_clipStart, m_clipEnd);
    emit navigationSettingsChanged();
    update();
}

void Viewport::setClipEnd(double mm)
{
    m_clipEnd = std::max(mm, 0.0);
    m_camera.setClipOverride(m_clipStart, m_clipEnd);
    emit navigationSettingsChanged();
    update();
}

void Viewport::beginOrbitAt(QPointF position)
{
    std::optional<view::Vec3d> pivot;
    if (m_document && m_orbitPivotMode == 1) {
        if (const auto bounds = m_document->selectionBounds()) {
            pivot = (bounds->first + bounds->second) * 0.5;
        }
    } else if (m_document && m_orbitPivotMode == 2) {
        pivot = m_document->surfacePoint(m_camera, toVec2(position));
    }
    m_camera.setOrbitPivot(pivot);
}

QVector3D Viewport::dropPointAt(const QPointF& position) const
{
    view::Vec3d p = m_camera.pose().target;
    if (m_document) {
        if (const auto surface = m_document->surfacePoint(m_camera, toVec2(position))) {
            p = *surface;
            return QVector3D(float(p.x), float(p.y), float(p.z));
        }
    }
    const math::Ray ray = m_camera.rayAt(toVec2(position));
    if (std::abs(ray.direction.z) > 1e-9) {
        const double t = -ray.origin.z / ray.direction.z;
        if (t > 0.0) {
            p = ray.origin + ray.direction * t;
        }
    }
    return QVector3D(float(p.x), float(p.y), float(p.z));
}

view::Vec3d Viewport::zoomAnchor(QPointF position)
{
    if (m_zoomToSurface && m_document) {
        if (const auto surface = m_document->surfacePoint(m_camera, toVec2(position))) {
            // Ținta la adâncimea suprafeței: pasul zoom-ului (proporțional cu distanța) devine mic lângă
            // detaliile mici, iar zoom-ul nu mai trece prin ele.
            m_camera.retargetToDepth(*surface);
            return *surface;
        }
    }
    return m_camera.zoomAnchorAt(toVec2(position));
}

void Viewport::updateSceneBounds()
{
    view::Vec3d lo;
    view::Vec3d hi;
    sceneBounds(lo, hi);
    // Marcajele camerelor și ale luminilor nu trebuie tăiate de planul îndepărtat (doar pentru tăiere; umbrele
    // folosesc cutia geometriei).
    if (m_document) {
        for (const model::CameraObject& c : m_document->model().cameras().values()) {
            lo = glm::min(lo, c.position);
            hi = glm::max(hi, c.position);
        }
        for (const model::Light& l : m_document->model().lights().values()) {
            lo = glm::min(lo, l.position);
            hi = glm::max(hi, l.position);
        }
    }
    m_camera.setSceneBounds(lo, hi);
}

void Viewport::zoomToSelection()
{
    const auto bounds = m_document ? m_document->selectionBounds() : std::nullopt;
    if (!bounds) {
        zoomExtents();
        return;
    }
    m_motion.animateTo(m_camera, m_camera.framing(bounds->first, bounds->second));
    requestAnimationFrame();
}

void Viewport::toggleProjection()
{
    setOrthographic(!isOrthographic());
}

QPointF Viewport::projectToScreen(const QVector3D& world) const
{
    const auto s = m_camera.project({ world.x(), world.y(), world.z() });
    return s ? QPointF(s->x, s->y) : QPointF(-1, -1);
}

void Viewport::startTurntable(double seconds)
{
    m_motion.stop();
    m_turntableSecondsLeft = seconds;
    requestAnimationFrame();
}

void Viewport::sceneBounds(view::Vec3d& min, view::Vec3d& max) const
{
    const auto bounds = m_document ? model::bounds(m_document->model(), m_document->model().root())
                                   : std::pair { view::Vec3d { 1.0 }, view::Vec3d { -1.0 } };
    if (bounds.first.x <= bounds.second.x) {
        min = bounds.first;
        max = bounds.second;
        if (m_demoVisible) {
            min = glm::min(min, kDemoMin);
            max = glm::max(max, kDemoMax);
        }
    } else if (m_demoVisible) {
        min = kDemoMin;
        max = kDemoMax;
    } else {
        // Model gol: o zonă de 10 × 10 m în jurul originii.
        min = { -5000.0, -5000.0, 0.0 };
        max = { 5000.0, 5000.0, 0.0 };
    }
}

void Viewport::collectTextures(ViewportFrame& f, const Document& doc) const
{
    // Imaginile decodate se păstrează între cadre; se decodează din nou doar când octeții se schimbă.
    const model::Model& model = doc.model();
    m_textureLayers.clear();
    std::size_t signature = model.textures().size();
    for (std::size_t i = 0; i < model.textures().size(); ++i) {
        const std::uint64_t id = model.textures().keys()[i].value;
        const model::Texture& t = model.textures().values()[i];
        const std::size_t key = std::hash<std::string_view> {}(
            std::string_view(reinterpret_cast<const char*>(t.data.data()), std::min<std::size_t>(t.data.size(), 4096)))
            ^ t.data.size();
        auto& cached = m_textureCache[id];
        if (cached.first != key || cached.second.isNull()) {
            cached = { key, QImage::fromData(t.data.data(), static_cast<int>(t.data.size())) };
        }
        m_textureLayers[id] = static_cast<int>(f.textures.size());
        f.textures.push_back(cached.second);
        signature = signature * 1000003u ^ (id * 31u + key);
    }
    f.textureSignature = signature;
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
    f.shadowMapSize = std::clamp(m_shadowMapSize, 512, 8192);
    if (m_document && m_document->property("snapToGrid").toBool()) {
        f.gridStep = m_document->property("gridStep").toDouble();
    }
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

    f.colors.accent = toVec4(m_accent);
    f.style = styleIndex(m_displayStyle);
    f.fog = m_fog;
    const auto [sunDir, sunIntensity] = sunPosition(m_sunHour, m_sunMonth, m_latitude);
    f.sunDirection = m_shadows ? sunDir : glm::normalize(glm::dvec3(-0.4, -0.6, 0.7));
    f.sunIntensity = m_shadows ? sunIntensity : 1.0;
    f.shadows = m_shadows && sunIntensity > 0.0 && f.style != 3;
    sceneBounds(f.sceneMin, f.sceneMax);
    if (m_document) {
        for (const model::SectionPlane& s : m_document->model().sections().values()) {
            if (s.active) {
                const glm::dvec3 n = glm::normalize(s.normal);
                f.section = glm::dvec4(n, glm::dot(n, s.point));
            }
        }
        collectTextures(f, *m_document);
        appendDocument(f);
        appendModelExtras(f, *m_document);
    }
    if (m_demoVisible) {
        appendBox(f.triangles, kDemoMin, kDemoMax);
        appendBoxEdges(f.lines, kDemoMin, kDemoMax, toVec4(m_edge), kEdgeWidth);
    }
    f.meshRevision = ((m_document ? m_document->revision() + m_document->interactionRevision() * 7919u : 0u)) * 2u
        + (m_demoVisible ? 1u : 0u) + (m_cleanCapture ? 0x40000000u : 0u)
        + static_cast<unsigned>(f.textureSignature & 0xFFFFu) * 2654435761u;

    f.points.push_back({ { 0, 0, 0 }, toVec4(m_axisZ.lighter(130)), 5.0f });
    // Pivotul orbitei, cât timp se orbitează în jurul lui.
    if (m_drag == Drag::Orbit && m_camera.orbitPivot()) {
        f.points.push_back({ glm::vec3(*m_camera.orbitPivot()), toVec4(m_accent), 8.0f });
    }
    return f;
}

void Viewport::onCameraMoved()
{
    updateAnnotations();
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
    if (m_sceneAnimation && !m_exporting) {
        advanceSceneAnimation(dt);
    }
    const bool interacting = m_document && m_document->advanceInteractions(dt);
    if (interacting && m_document->playing()) {
        applyAnimationCamera();
    }
    if (m_turntableSecondsLeft > 0.0) {
        constexpr double kTurntableRadiansPerSecond = 0.8;
        m_camera.orbit(kTurntableRadiansPerSecond * dt, 0.0);
        m_turntableSecondsLeft -= dt;
        onCameraMoved();
    }
    if (m_motion.isActive() || m_turntableSecondsLeft > 0.0 || (m_sceneAnimation && !m_exporting) || interacting) {
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
            m_camera.zoomAbout(factor, zoomAnchor(pos));
            onCameraMoved();
            showTransientCursor(m_zoomCursor);
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

void Viewport::updateInteractionCursor(QPointF position)
{
    const QString kind = m_document->interactionHover(m_camera, toVec2(position));
    setCursor(kind == QLatin1String("drag") ? Qt::OpenHandCursor
            : kind == QLatin1String("click") ? Qt::PointingHandCursor
            : kind == QLatin1String("context") ? Qt::WhatsThisCursor
                                               : Qt::ArrowCursor);
}

void Viewport::keyPressEvent(QKeyEvent* event)
{
    if (m_document && m_document->interactionMode() && !event->isAutoRepeat()) {
        m_document->interactionKey(QKeySequence(event->key()).toString());
        event->accept();
        return;
    }
    QQuickRhiItem::keyPressEvent(event);
}

void Viewport::mousePressEvent(QMouseEvent* event)
{
    // Modul interacțiune: nicio unealtă de editare; evenimentele pornesc regulile obiectelor.
    if (m_document && m_document->interactionMode() && event->button() != Qt::MiddleButton
        && m_navigationTool.isEmpty()) {
        forceActiveFocus(Qt::MouseFocusReason);
        if (event->button() == Qt::RightButton) {
            if (m_document->interactionEvent(m_camera, toVec2(event->position()), QStringLiteral("rightClick"))) {
                requestAnimationFrame();
            }
        } else if (event->button() == Qt::LeftButton) {
            m_interactionPressed = true;
            m_longPressFired = false;
            m_interactionPress = event->position();
            m_interactionDragging = m_document->interactionDragStart(m_camera, toVec2(event->position()));
            if (m_interactionDragging) {
                setCursor(Qt::ClosedHandCursor);
            } else {
                m_longPressTimer.setSingleShot(true);
                m_longPressTimer.setInterval(600);
                disconnect(&m_longPressTimer, nullptr, this, nullptr);
                connect(&m_longPressTimer, &QTimer::timeout, this, [this] {
                    if (m_interactionPressed && m_document
                        && m_document->interactionEvent(m_camera, toVec2(m_interactionPress), QStringLiteral("longPress"))) {
                        m_longPressFired = true;
                        requestAnimationFrame();
                    }
                });
                m_longPressTimer.start();
            }
        }
        event->accept();
        return;
    }
    stopScenes(); // ca în SketchUp: orice click oprește animația
    // Tastele (Measurements box, săgețile) ajung la viewport după un click în el.
    forceActiveFocus(Qt::MouseFocusReason);
    const auto mods = event->modifiers();
    const bool shift = mods.testFlag(Qt::ShiftModifier);
    // Pe macOS, Qt raportează tasta Control ca MetaModifier (Cmd devine ControlModifier).
    const bool ctrl = mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier);

    if (event->button() == Qt::RightButton) {
        m_rightPress = event->position();
        event->accept();
        return;
    }

    Drag drag = Drag::None;
    if (event->button() == Qt::MiddleButton) {
        drag = ctrl ? Drag::Zoom : shift ? Drag::Pan : Drag::Orbit;
    } else if (event->button() == Qt::LeftButton) {
        if (m_navigationTool == QLatin1String("orbit")) {
            drag = shift ? Drag::Pan : Drag::Orbit;
        } else if (m_navigationTool == QLatin1String("pan")) {
            drag = Drag::Pan;
        } else if (m_navigationTool == QLatin1String("zoom")) {
            drag = Drag::Zoom;
        } else if (m_navigationTool == QLatin1String("interact")) {
            if (m_document && m_document->interactAt(m_camera, toVec2(event->position()))) {
                requestAnimationFrame();
            }
            event->accept();
            return;
        }
    }
    // Click pe o cameră sau o lumină (cu unealta Select): o selectează.
    if (drag == Drag::None && event->button() == Qt::LeftButton && m_document
        && m_document->activeTool() == QLatin1String("select")) {
        if (const auto handle = objectHandleAt(event->position())) {
            m_document->selectObject(handle->first, handle->second);
            event->accept();
            return;
        }
    }
    if (drag == Drag::None && event->button() == Qt::LeftButton && m_document && m_document->hasTool()) {
        // Un click de editare iese din previzualizarea animației: uneltele lucrează pe model.
        m_document->endPreview();
        // Numărăm noi clicurile (dublu, triplu), ca să funcționeze la fel pe toate platformele.
        const qint64 now = m_frameClock.nsecsElapsed();
        const qint64 interval = qint64(QGuiApplication::styleHints()->mouseDoubleClickInterval()) * 1'000'000;
        const bool continues = now - m_lastClickNs <= interval
            && QLineF(event->position(), m_lastClickPosition).length() <= kMultiClickDistancePx;
        m_clickCount = continues ? std::min(m_clickCount + 1, 3) : 1;
        m_lastClickNs = now;
        m_lastClickPosition = event->position();

        m_toolPressed = true;
        m_document->pointerDown(m_camera, toolEvent(event->position(), event->modifiers(), m_clickCount));
        afterToolEvent();
        event->accept();
        return;
    }
    if (drag == Drag::None) {
        event->ignore();
        return;
    }
    beginDrag(drag, event->position());
    if (drag == Drag::Zoom) {
        m_zoomDragAnchor = zoomAnchor(event->position());
    } else if (drag == Drag::Orbit) {
        beginOrbitAt(event->position());
    }
    updateCursor();
    event->accept();
}

void Viewport::mouseMoveEvent(QMouseEvent* event)
{
    if (m_interactionPressed && m_document && m_document->interactionMode()) {
        if (m_interactionDragging) {
            m_document->interactionDragMove(m_camera, toVec2(event->position()));
        } else if (QLineF(event->position(), m_interactionPress).length() > kMultiClickDistancePx) {
            m_longPressTimer.stop(); // s-a mișcat: nu mai e o apăsare lungă
        }
        event->accept();
        return;
    }
    const QPointF delta = event->position() - m_lastPointer;
    m_lastPointer = event->position();
    setPointer(event->position(), contains(event->position()));

    if (m_drag == Drag::Orbit) {
        m_camera.orbit(-delta.x() * kOrbitRadiansPerPixel, -delta.y() * kOrbitRadiansPerPixel);
        recordOrbitSample(delta);
        onCameraMoved();
    } else if (m_drag == Drag::Pan) {
        m_camera.pan(toVec2(delta));
        onCameraMoved();
    } else if (m_drag == Drag::Zoom) {
        m_camera.zoomAbout(std::exp(delta.y() * kDragZoomPerPixel), m_zoomDragAnchor);
        onCameraMoved();
    } else if (m_toolPressed && m_document) {
        m_document->pointerMove(m_camera, toolEvent(event->position(), event->modifiers(), m_clickCount));
        afterToolEvent();
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_interactionPressed && m_document && m_document->interactionMode() && event->button() == Qt::LeftButton) {
        m_interactionPressed = false;
        m_longPressTimer.stop();
        if (m_interactionDragging) {
            m_document->interactionDragEnd();
            m_interactionDragging = false;
            requestAnimationFrame();
        } else if (!m_longPressFired
            && QLineF(event->position(), m_interactionPress).length() <= kMultiClickDistancePx) {
            if (m_document->interactionEvent(m_camera, toVec2(event->position()), QStringLiteral("click"))) {
                requestAnimationFrame();
            }
        }
        updateInteractionCursor(event->position());
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        if (m_rightPress && QLineF(*m_rightPress, event->position()).length() <= kMultiClickDistancePx && m_document) {
            m_document->selectForContextMenu(m_camera, toVec2(event->position()));
            afterToolEvent();
            emit contextMenuRequested(event->position());
        }
        m_rightPress.reset();
        event->accept();
        return;
    }
    if (m_toolPressed && event->button() == Qt::LeftButton) {
        m_toolPressed = false;
        if (m_document) {
            m_document->pointerUp(m_camera, toolEvent(event->position(), event->modifiers(), m_clickCount));
            afterToolEvent();
        }
        event->accept();
        return;
    }
    if (m_drag == Drag::Orbit) {
        m_motion.setOrbitVelocity(orbitReleaseVelocity());
        requestAnimationFrame();
    }
    m_drag = Drag::None;
    updateCursor();
    event->accept();
}

void Viewport::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_document && m_document->interactionMode() && event->button() == Qt::LeftButton) {
        if (m_document->interactionEvent(m_camera, toVec2(event->position()), QStringLiteral("doubleClick"))) {
            requestAnimationFrame();
        }
        event->accept();
        return;
    }
    // Dublu-click cu butonul din mijloc centrează vederea pe punctul de sub cursor (ca în SketchUp).
    if (event->button() == Qt::MiddleButton) {
        auto pose = m_camera.pose();
        const auto surface = m_document ? m_document->surfacePoint(m_camera, toVec2(event->position())) : std::nullopt;
        pose.target = surface ? *surface : m_camera.zoomAnchorAt(toVec2(event->position()));
        m_motion.animateTo(m_camera, pose);
        requestAnimationFrame();
        event->accept();
        return;
    }
    // Butonul stâng: clicurile multiple le numărăm în mousePressEvent.
    event->setAccepted(event->button() == Qt::LeftButton && m_document && m_document->hasTool());
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
    stopScenes();
    const QPointF pos = event->position();
    setPointer(pos, true);
    const auto mods = event->modifiers();
    // Trackpad-ul trimite evenimente cu fază (început, mișcare, inerție); rotița clasică nu.
    const bool trackpad = event->phase() != Qt::NoScrollPhase
        || event->pointingDevice()->type() == QInputDevice::DeviceType::TouchPad;

    if (trackpad && !mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::MetaModifier)) {
        m_motion.stop();
        const QPointF d = event->pixelDelta();
        if (mods.testFlag(Qt::ShiftModifier)) {
            m_camera.pan(toVec2(d));
            showTransientCursor(Qt::ClosedHandCursor);
        } else {
            // Pivotul se alege la începutul gestului (sau la fiecare eveniment, dacă dispozitivul nu are faze).
            if (event->phase() == Qt::ScrollBegin || event->phase() == Qt::NoScrollPhase || !m_trackpadOrbiting) {
                beginOrbitAt(pos);
                m_trackpadOrbiting = true;
            }
            m_camera.orbit(-d.x() * kOrbitRadiansPerPixel, -d.y() * kOrbitRadiansPerPixel);
            showTransientCursor(m_orbitCursor);
        }
        if (event->phase() == Qt::ScrollEnd) {
            m_trackpadOrbiting = false;
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
    m_motion.zoomSmoothly(factor, zoomAnchor(pos));
    showTransientCursor(m_zoomCursor);
    requestAnimationFrame();
    event->accept();
}

void Viewport::hoverEnterEvent(QHoverEvent* event)
{
    setPointer(event->position(), true);
    QQuickRhiItem::hoverEnterEvent(event);
}

void Viewport::hoverMoveEvent(QHoverEvent* event)
{
    setPointer(event->position(), true);
    if (m_document && m_document->interactionMode()) {
        if (m_drag == Drag::None && m_navigationTool.isEmpty()) {
            updateInteractionCursor(event->position());
        }
        QQuickRhiItem::hoverMoveEvent(event);
        return;
    }
    // În timpul navigării cu butonul din mijloc nu actualizăm unealta (evenimentele vin prin mouseMove).
    if (m_document && m_drag == Drag::None && !m_toolPressed) {
        m_document->pointerMove(m_camera, toolEvent(event->position(), event->modifiers()));
        afterToolEvent();
    }
    QQuickRhiItem::hoverMoveEvent(event);
}

void Viewport::hoverLeaveEvent(QHoverEvent* event)
{
    QQuickRhiItem::hoverLeaveEvent(event);
    setPointer(m_hoverPosition, false);
    update();
}

} // namespace aion::render
