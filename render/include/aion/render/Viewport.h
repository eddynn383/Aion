#pragma once

#include "aion/core/tools/Tool.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/CameraMotion.h"
#include "aion/core/view/SceneAnimation.h"
#include "aion/render/Document.h"

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QCursor>
#include <QElapsedTimer>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QQuickRhiItem>
#include <QTimer>
#include <QVariantList>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

#include <glm/vec4.hpp>

#include <atomic>
#include <deque>
#include <optional>
#include <utility>

namespace aion::render {

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
    // Muchiile fără fețe stau direct pe fundal, deci au nevoie de altă culoare (în dark mode, deschisă).
    Q_PROPERTY(QColor freeEdgeColor MEMBER m_freeEdge NOTIFY appearanceChanged)
    Q_PROPERTY(QColor faceFrontColor MEMBER m_faceFront NOTIFY appearanceChanged)
    Q_PROPERTY(QColor faceBackColor MEMBER m_faceBack NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accentColor MEMBER m_accent NOTIFY appearanceChanged)
    Q_PROPERTY(QColor previewColor MEMBER m_preview NOTIFY appearanceChanged)
    Q_PROPERTY(QColor eraseColor MEMBER m_erase NOTIFY appearanceChanged)
    // Culorile inferenței care nu sunt ale axelor: mijloc (cyan), paralel/perpendicular (magenta).
    Q_PROPERTY(QColor inferenceCyanColor MEMBER m_inferenceCyan NOTIFY appearanceChanged)
    Q_PROPERTY(QColor inferenceMagentaColor MEMBER m_inferenceMagenta NOTIFY appearanceChanged)
    Q_PROPERTY(bool gridVisible MEMBER m_gridVisible NOTIFY appearanceChanged)
    Q_PROPERTY(bool demoVisible MEMBER m_demoVisible NOTIFY appearanceChanged)

    // Stilul de afișare: "shaded", "clay", "hiddenline", "wireframe", "xray".
    Q_PROPERTY(QString displayStyle MEMBER m_displayStyle NOTIFY appearanceChanged)
    // Muchiile de contur (silueta obiectelor), mai groase.
    Q_PROPERTY(bool profiles MEMBER m_profiles NOTIFY appearanceChanged)
    Q_PROPERTY(bool fog MEMBER m_fog NOTIFY appearanceChanged)
    // Umbrele soarelui, poziționat după oră, lună și latitudine (implicit România, 46° N).
    Q_PROPERTY(bool shadows MEMBER m_shadows NOTIFY appearanceChanged)
    Q_PROPERTY(double sunHour MEMBER m_sunHour NOTIFY appearanceChanged)
    Q_PROPERTY(int sunMonth MEMBER m_sunMonth NOTIFY appearanceChanged)
    Q_PROPERTY(double latitude MEMBER m_latitude NOTIFY appearanceChanged)

    // Navigare (Preferințe): centrul orbitei 0 = centrul vederii, 1 = selecția, 2 = punctul de sub cursor;
    // zoom spre suprafața de sub cursor („auto depth”); planurile de tăiere manuale, în mm (0 = automat).
    // Calitatea imaginii (Preferințe → Viewport): eșantioane MSAA (1, 2, 4, 8) și mărimea hărții de umbre.
    Q_PROPERTY(int antialiasing READ sampleCount WRITE setSampleCount NOTIFY sampleCountChanged)
    Q_PROPERTY(int shadowQuality MEMBER m_shadowMapSize NOTIFY appearanceChanged)
    Q_PROPERTY(int orbitPivot READ orbitPivot WRITE setOrbitPivot NOTIFY navigationSettingsChanged)
    Q_PROPERTY(bool zoomToSurface MEMBER m_zoomToSurface NOTIFY navigationSettingsChanged)
    Q_PROPERTY(double clipStart READ clipStart WRITE setClipStart NOTIFY navigationSettingsChanged)
    Q_PROPERTY(double clipEnd READ clipEnd WRITE setClipEnd NOTIFY navigationSettingsChanged)

    // Animația scenelor: redare în timp real și export cadru cu cadru.
    Q_PROPERTY(bool scenesPlaying READ scenesPlaying NOTIFY sceneAnimationChanged)
    Q_PROPERTY(int playingScene READ playingScene NOTIFY sceneAnimationChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY sceneAnimationChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY sceneAnimationChanged)

    Q_PROPERTY(bool orthographic READ isOrthographic WRITE setOrthographic NOTIFY cameraChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    // Unealta de navigare legată de butonul stâng: "" (niciuna), "orbit", "pan" sau "zoom".
    Q_PROPERTY(QString navigationTool READ navigationTool WRITE setNavigationTool NOTIFY navigationToolChanged)

    // Baza camerei, pentru gizmo-ul de navigare.
    Q_PROPERTY(QVector3D cameraRight READ cameraRight NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D cameraUp READ cameraUp NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D cameraForward READ cameraForward NOTIFY cameraChanged)
    Q_PROPERTY(double cameraDistance READ cameraDistance NOTIFY cameraChanged)

    // Documentul afișat; uneltele lui primesc evenimentele butonului stâng.
    Q_PROPERTY(aion::render::Document* document READ document WRITE setDocument NOTIFY documentChanged)
    // Dreptunghiul de selecție în curs (gol dacă nu există), pentru desenul din QML.
    Q_PROPERTY(QRectF selectionRect READ selectionRect NOTIFY selectionRectChanged)
    Q_PROPERTY(bool selectionCrossing READ selectionCrossing NOTIFY selectionRectChanged)
    // Etichetele desenate de QML peste viewport: inferența („Endpoint”) și cotele live.
    // Fiecare element: { kind: "inference" | "dimension", x, y, text, color, locked }.
    Q_PROPERTY(QVariantList annotations READ annotations NOTIFY annotationsChanged)
    // Poziția cursorului în viewport (pentru Measurements box) și dacă e deasupra lui.
    Q_PROPERTY(QPointF pointerPosition READ pointerPosition NOTIFY pointerPositionChanged)
    Q_PROPERTY(bool pointerInside READ pointerInside NOTIFY pointerPositionChanged)

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

    Document* document() const { return m_document; }
    void setDocument(Document* document);
    QRectF selectionRect() const { return m_selectionRect; }
    bool selectionCrossing() const { return m_selectionCrossing; }
    QVariantList annotations() const { return m_annotations; }
    QPointF pointerPosition() const { return m_hoverPosition; }
    bool pointerInside() const { return m_pointerInside; }

    QString navigationTool() const { return m_navigationTool; }
    void setNavigationTool(const QString& tool);

    // Cursoarele pentru orbit și zoom (desenate de aplicație din iconițe).
    void setNavigationCursors(const QCursor& orbit, const QCursor& zoom);
    void setPaintCursor(const QCursor& paint);

    QVector3D cameraRight() const;
    QVector3D cameraUp() const;
    QVector3D cameraForward() const;
    double cameraDistance() const { return m_camera.pose().distance; }
    int framesPerSecond() const { return m_fps; }

    Q_INVOKABLE void setStandardView(aion::render::Viewport::StandardView view);
    Q_INVOKABLE void zoomExtents();
    // Imaginea viewport-ului, fără elementele de interfață puse peste el (toolbar, gizmo, etichete).
    Q_INVOKABLE bool saveImage(const QString& path);
    // Previzualizarea pentru fișierul .aion: PNG de cel mult 512 px lățime.
    Q_INVOKABLE QByteArray captureThumbnail();
    // Poziția camerei pentru scene: { tx, ty, tz, yaw, pitch, distance, ortho }; și tranziția animată spre una.
    Q_INVOKABLE QVariantMap cameraState() const;
    Q_INVOKABLE void animateToCamera(const QVariantMap& state, double seconds = -1.0);
    // `states`: pozițiile scenelor (ca cameraState()). Orice click sau scroll în viewport oprește redarea.
    Q_INVOKABLE void playScenes(const QVariantList& states, double transition, double delay, bool loop);
    Q_INVOKABLE void stopScenes();
    // Randează animația în PNG-uri (`directory`/frame_00000.png …); la sfârșit emite animationExported.
    Q_INVOKABLE void exportScenes(const QVariantList& states, double transition, double delay, bool loop,
        const QString& directory, int fps);
    Q_INVOKABLE void cancelExport();
    // Randează animația activă din timeline (cu camera ei, dacă are) în PNG-uri; la sfârșit emite animationExported.
    Q_INVOKABLE void exportAnimation(const QString& directory, int fps);
    bool scenesPlaying() const { return m_sceneAnimation.has_value() && !m_exporting; }
    int playingScene() const { return m_playingScene; }
    double exportProgress() const { return m_exportProgress; }
    bool exporting() const { return m_exporting; }
    // Încadrează selecția documentului (sau tot modelul, dacă nu e nimic selectat).
    Q_INVOKABLE void zoomToSelection();
    Q_INVOKABLE void toggleProjection();
    // Rotește camera continuu timp de `seconds` secunde (pentru măsurarea fps-ului).
    Q_INVOKABLE void startTurntable(double seconds);
    // Poziția pe ecran (în coordonatele item-ului) a unui punct 3D; pentru scripturile de verificare.
    Q_INVOKABLE QPointF projectToScreen(const QVector3D& world) const;

    // Apelat de renderer (pe firul de randare, cu firul GUI blocat).
    ViewportFrame frameForRendering() const;

signals:
    void appearanceChanged();
    void cameraChanged();
    void reducedMotionChanged();
    void navigationToolChanged();
    void framesPerSecondChanged();
    void documentChanged();
    void selectionRectChanged();
    // Click dreapta (fără tragere): ce era sub cursor a devenit selecția; QML deschide meniul contextual.
    void contextMenuRequested(QPointF position);
    void annotationsChanged();
    void pointerPositionChanged();
    void navigationSettingsChanged();
    void sceneAnimationChanged();
    // Export terminat: `frames` cadre în `directory` (0 = anulat sau eșuat).
    void animationExported(const QString& directory, int frames, int fps);

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
    void hoverEnterEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

public:
    int orbitPivot() const { return m_orbitPivotMode; }
    void setOrbitPivot(int mode);
    double clipStart() const { return m_clipStart; }
    void setClipStart(double mm);
    double clipEnd() const { return m_clipEnd; }
    void setClipEnd(double mm);

private:
    enum class Drag { None, Orbit, Pan, Zoom };

    // Pivotul orbitei la începutul unei orbite (după orbitPivot), în coordonate globale.
    void beginOrbitAt(QPointF position);
    // Ancora zoom-ului: suprafața de sub cursor (cu ținta mutată la adâncimea ei) sau planul obișnuit.
    view::Vec3d zoomAnchor(QPointF position);
    // Cutia scenei pentru planurile de tăiere ale camerei (după fiecare schimbare a modelului).
    void updateSceneBounds();

    void advanceSceneAnimation(double dt);
    void exportNextFrame();

    std::optional<view::SceneAnimation> m_sceneAnimation;
    double m_sceneTime = 0.0;
    bool m_loopScenes = false;
    int m_playingScene = -1;
    // Cât se captează o imagine (PNG, cadrele unui video): fără selecție, hover și marcaje.
    bool m_cleanCapture = false;
    bool m_exporting = false;
    double m_exportProgress = 0.0;
    int m_exportFrame = 0;
    int m_exportFrames = 0;
    int m_exportFps = 30;
    QString m_exportDirectory;
    QTimer* m_exportTimer = nullptr;
    bool m_exportTimeline = false; // exportul vine din timeline (nu din scene)
    void applyAnimationCamera();

    int m_shadowMapSize = 2048;
    // Modul interacțiune: apăsarea curentă (pentru click / apăsare lungă / drag).
    bool m_interactionPressed = false;
    bool m_interactionDragging = false;
    bool m_longPressFired = false;
    QPointF m_interactionPress;
    QTimer m_longPressTimer;
    void updateInteractionCursor(QPointF position);
    int m_orbitPivotMode = 0;
    bool m_trackpadOrbiting = false;
    bool m_zoomToSurface = true;
    double m_clipStart = 0.0;
    double m_clipEnd = 0.0;

    void updateCursor();
    tools::PointerEvent toolEvent(QPointF position, Qt::KeyboardModifiers modifiers, int clickCount = 1) const;
    void afterToolEvent();
    void updateAnnotations();
    void setPointer(QPointF position, bool inside);
    glm::vec4 markerColor(inference::Kind kind, inference::Tint tint) const;
    glm::vec4 tintColor(inference::Tint tint) const;
    void appendDocument(ViewportFrame& f) const;
    // Cum se desenează o definiție: estompată (în afara grupului editat), selectată, sub cursor, marcată de gumă.
    struct DrawStyle {
        bool dim = false;
        bool selected = false;
        bool hovered = false;
        bool marked = false;
        model::MaterialId material; // materialul grupului, pentru fețele fără material
        std::optional<glm::vec4> colorOverride; // culoarea animată (timeline), peste toate fețele
    };
    // Desenează definiția și tot ce conține. `pathIndex`: câte instanțe din calea contextului s-au parcurs
    // (egal cu lungimea căii = contextul editat; -1 = în afara căii).
    void appendDefinition(ViewportFrame& f, const Document& doc, model::DefinitionId definition,
        const math::Transform& toWorld, int pathIndex, DrawStyle style, const view::PickResult& hover,
        const tools::Overlay& overlay) const;
    void showTransientCursor(const QCursor& cursor);
    void onCameraMoved();
    void requestAnimationFrame();
    void onAfterAnimating();
    void beginDrag(Drag drag, QPointF position);
    void recordOrbitSample(QPointF delta);
    view::Vec2d orbitReleaseVelocity() const;
    void sceneBounds(view::Vec3d& min, view::Vec3d& max) const;
    QImage renderImage();
    // Transformarea (în model) a unei definiții: prin calea editată, dacă e pe ea, altfel prin prima ei instanță.
    math::Transform definitionToWorld(const Document& doc, model::DefinitionId definition) const;
    void appendModelExtras(ViewportFrame& f, const Document& doc) const;
    // Camera sau lumina al cărei marcaj e sub cursor: ("camera" | "light", id).
    std::optional<std::pair<QString, double>> objectHandleAt(QPointF position) const;

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
    QPointer<Document> m_document;
    bool m_toolPressed = false;
    std::optional<QPointF> m_rightPress;
    int m_clickCount = 0;
    qint64 m_lastClickNs = 0;
    QPointF m_lastClickPosition;
    QRectF m_selectionRect;
    bool m_selectionCrossing = false;
    QVariantList m_annotations;
    bool m_pointerInside = false;
    view::Vec3d m_zoomDragAnchor { 0.0, 0.0, 0.0 };

    QCursor m_orbitCursor { Qt::SizeAllCursor };
    QCursor m_paintCursor { Qt::CrossCursor };
    QCursor m_zoomCursor { Qt::SizeVerCursor };
    // La rotiță și gesturile de trackpad, cursorul acțiunii rămâne vizibil puțin după ultimul eveniment.
    QTimer m_transientCursorTimer;
    QCursor m_transientCursor;

    QColor m_backgroundTop { 0x1A, 0x1B, 0x1F };
    QColor m_backgroundBottom { 0x0E, 0x0F, 0x11 };
    QColor m_gridMinor { 255, 255, 255, 18 };
    QColor m_gridMajor { 255, 255, 255, 40 };
    QColor m_axisX { 0xEF, 0x44, 0x44 };
    QColor m_axisY { 0x22, 0xC5, 0x5E };
    QColor m_axisZ { 0x3B, 0x82, 0xF6 };
    QColor m_edge { 0x11, 0x11, 0x13 };
    QColor m_freeEdge { 0xC9, 0xCB, 0xD1 };
    QColor m_faceFront { 0xE8, 0xE6, 0xE1 };
    QColor m_faceBack { 0x8F, 0x9B, 0xB3 };
    QColor m_accent { 0x3B, 0x82, 0xF6 };
    QColor m_preview { 0xED, 0xED, 0xED };
    QColor m_erase { 0xEF, 0x44, 0x44 };
    QColor m_inferenceCyan { 0x06, 0xB6, 0xD4 };
    QColor m_inferenceMagenta { 0xD9, 0x46, 0xEF };
    QColor m_guide { 0x8B, 0x95, 0xA7, 210 };
    QColor m_sectionColor { 0xF9, 0x73, 0x16 };
    bool m_gridVisible = true;
    QString m_displayStyle = QStringLiteral("shaded");
    bool m_profiles = true;
    bool m_fog = false;
    bool m_shadows = false;
    double m_sunHour = 14.0;
    int m_sunMonth = 6;
    double m_latitude = 46.0;
    bool m_demoVisible = false;
    bool m_reducedMotion = false;
    QString m_navigationTool;

    std::atomic<int> m_framesThisSecond { 0 };
    int m_fps = 0;
};

} // namespace aion::render
