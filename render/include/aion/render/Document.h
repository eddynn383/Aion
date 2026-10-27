#pragma once

#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/model/Model.h"
#include "aion/core/tools/Tool.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

namespace aion::render {

// Documentul deschis: modelul, istoricul undo/redo, contextul de editare, selecția și unealta activă.
// Puntea dintre nucleu (fără Qt) și interfață: QML îl folosește pentru comenzi și stare,
// iar Viewport-ul îi trimite evenimentele de mouse și desenează din el.
class Document : public QObject, private model::ModelObserver {
    Q_OBJECT
    QML_ELEMENT

    // "select", "line", "rectangle", ... (vezi tools::makeTool); alte valori = nicio unealtă de modelare activă.
    Q_PROPERTY(QString activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QString toolStatus READ toolStatus NOTIFY toolStatusChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString undoName READ undoName NOTIFY historyChanged)
    Q_PROPERTY(QString redoName READ redoName NOTIFY historyChanged)
    Q_PROPERTY(QString selectionSummary READ selectionSummary NOTIFY selectionChanged)
    // Measurements box: dacă unealta primește valori tastate, eticheta („Lungime”) și valoarea curentă.
    Q_PROPERTY(bool hasMeasurement READ hasMeasurement NOTIFY toolStatusChanged)
    Q_PROPERTY(QString measurementLabel READ measurementLabel NOTIFY toolStatusChanged)
    Q_PROPERTY(QString measurementValue READ measurementValue NOTIFY toolStatusChanged)
    Q_PROPERTY(int faceCount READ faceCount NOTIFY modelChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY modelChanged)

    // Contextul de editare: numele grupurilor în care s-a intrat (gol = modelul întreg).
    Q_PROPERTY(QStringList contextPath READ contextPath NOTIFY contextChanged)
    // Arborele modelului pentru Outliner, aplatizat: { id, name, kind ("group"|"component"), depth, hidden,
    // locked, selected, current (e contextul editat) }.
    Q_PROPERTY(QVariantList outline READ outline NOTIFY outlineChanged)
    // Tag-urile: { id, name, color, visible, count }.
    Q_PROPERTY(QVariantList tags READ tags NOTIFY outlineChanged)
    // Entity Info: { kind, title, details: [{label, value}], instanceId, name, tagId }.
    Q_PROPERTY(QVariantMap entityInfo READ entityInfo NOTIFY selectionChanged)
    // Ce comenzi de organizare se pot aplica selecției.
    Q_PROPERTY(bool canGroup READ canGroup NOTIFY selectionChanged)
    Q_PROPERTY(bool hasInstanceSelected READ hasInstanceSelected NOTIFY selectionChanged)

    // --- Fișiere (Faza 7) ---
    // Calea fișierului (gol = încă nesalvat), numele afișat și dacă are modificări nesalvate.
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY historyChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    // O sesiune nesalvată, găsită la pornire (autosalvarea unei rulări care nu s-a închis normal).
    Q_PROPERTY(bool hasRecovery READ hasRecovery NOTIFY recoveryChanged)
    Q_PROPERTY(QString recoveryInfo READ recoveryInfo NOTIFY recoveryChanged)
    // --- Materiale (Faza 8) ---
    // Biblioteca inclusă: { index, name, color, roughness, metallic }.
    Q_PROPERTY(QVariantList libraryMaterials READ libraryMaterials CONSTANT)
    // Materialele din model: { id, name, color, roughness, metallic, count }.
    Q_PROPERTY(QVariantList modelMaterials READ modelMaterials NOTIFY outlineChanged)
    // Materialul cu care vopsește Paint Bucket: { source: "default"|"model"|"library", id, index, name, color }.
    Q_PROPERTY(QVariantMap currentMaterial READ currentMaterial NOTIFY materialChanged)

    // --- Faza 9 ---
    // Scenele: { id, name, style } (poziția camerei o citește QML prin sceneCamera).
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY outlineChanged)
    // Timeline-ul: animațiile modelului, cea activă, momentul curent, previzualizarea și redarea.
    Q_PROPERTY(QVariantList animations READ animations NOTIFY outlineChanged)
    Q_PROPERTY(double activeAnimation READ activeAnimation WRITE setActiveAnimation NOTIFY timelineChanged)
    Q_PROPERTY(QVariantMap timeline READ timeline NOTIFY timelineChanged)
    Q_PROPERTY(double timelineTime READ timelineTime WRITE setTimelineTime NOTIFY timelineTimeChanged)
    Q_PROPERTY(bool previewing READ previewing NOTIFY timelineChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY timelineChanged)
    // Planurile de secțiune: { id, name, active }.
    Q_PROPERTY(QVariantList sections READ sections NOTIFY outlineChanged)
    Q_PROPERTY(int guideCount READ guideCount NOTIFY outlineChanged)
    // Adnotarea al cărei text se editează acum (după unealta Text), 0 = niciuna.
    Q_PROPERTY(double editingAnnotation READ editingAnnotation WRITE setEditingAnnotation NOTIFY editingAnnotationChanged)

    // Ultima eroare de fișier (pentru un mesaj în interfață).
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

    // Preferințe de afișare a lungimilor (păstrate în QSettings): 0 Auto, 1 mm, 2 cm, 3 m, 4 inci, 5 picioare;
    // zecimale: -1 = implicit.
    // Snap la grid: activ sau nu; pasul în mm (0 = automat, după grid-ul vizibil).
    Q_PROPERTY(bool snapToGrid MEMBER m_snapToGrid NOTIFY gridSnapChanged)
    Q_PROPERTY(double gridStep MEMBER m_gridStep NOTIFY gridSnapChanged)
    // Autosalvarea (Preferințe → Salvare), păstrată în QSettings.
    Q_PROPERTY(bool autosaveEnabled READ autosaveEnabled WRITE setAutosaveEnabled NOTIFY autosaveSettingsChanged)
    Q_PROPERTY(int autosaveMinutes READ autosaveMinutes WRITE setAutosaveMinutes NOTIFY autosaveSettingsChanged)
    Q_PROPERTY(int displayUnit READ displayUnit WRITE setDisplayUnit NOTIFY unitsChanged)
    Q_PROPERTY(int displayDecimals READ displayDecimals WRITE setDisplayDecimals NOTIFY unitsChanged)

public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    QString activeTool() const { return m_toolId; }
    void setActiveTool(const QString& id);
    QString toolStatus() const;
    bool canUndo() const { return m_stack.canUndo(); }
    bool canRedo() const { return m_stack.canRedo(); }
    QString undoName() const;
    QString redoName() const;
    QString selectionSummary() const;
    bool hasMeasurement() const;
    QString measurementLabel() const;
    QString measurementValue() const;
    int faceCount() const;
    int edgeCount() const;
    QStringList contextPath() const;
    QVariantList outline() const;
    QVariantList tags() const;
    QVariantMap entityInfo() const;
    bool canGroup() const { return !m_selection.empty(); }
    bool hasInstanceSelected() const { return !m_selection.instances.empty(); }
    QString filePath() const { return m_filePath; }
    QString fileName() const;
    bool modified() const { return !m_stack.isClean(); }
    QStringList recentFiles() const;
    bool hasRecovery() const { return m_hasRecovery; }
    QString recoveryInfo() const;
    QString lastError() const { return m_lastError; }
    bool autosaveEnabled() const { return m_autosaveTimer.isActive(); }
    void setAutosaveEnabled(bool enabled);
    int autosaveMinutes() const { return m_autosaveTimer.interval() / 60'000; }
    void setAutosaveMinutes(int minutes);
    int displayUnit() const;
    void setDisplayUnit(int unit);
    int displayDecimals() const;
    void setDisplayDecimals(int decimals);
    // Raportul de cantități (general) și lista de debitare (mobilier), ca date pentru QML. Lungimi în mm,
    // arii în m², volume în m³. `options`: sheetLength, sheetWidth (mm), waste (0..1), maxThickness (mm).
    Q_INVOKABLE QVariantMap quantityReport() const;
    Q_INVOKABLE QVariantMap cutList(const QVariantMap& options) const;
    // Scrie raportul („general” sau „cutlist”) ca CSV (separator „;”, virgulă zecimală, pentru Excel în română).
    Q_INVOKABLE bool exportReport(const QString& path, const QString& kind, const QVariantMap& options);
    // Exemplu de lungime formatată cu setările curente (pentru dialogul de preferințe).
    Q_INVOKABLE QString formatLength(double mm) const;
    QVariantList libraryMaterials() const;
    QVariantList modelMaterials() const;
    QVariantMap currentMaterial() const;
    QVariantList scenes() const;
    QVariantList sections() const;
    int guideCount() const { return static_cast<int>(m_model.guides().size()); }
    double editingAnnotation() const { return static_cast<double>(m_editingAnnotation.value); }
    void setEditingAnnotation(double id);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    // Esc: anulează operația uneltei; dacă nu era nimic de anulat, unealta decide (Select deselectează sau iese din grup).
    Q_INVOKABLE void escape();
    Q_INVOKABLE void enter();
    // "left", "right", "up", "down": lock-ul inferenței pe o axă. Întoarce true dacă unealta a folosit tasta.
    Q_INVOKABLE bool arrowKey(const QString& direction);
    // Valoarea din Measurements box („2500”, „2.5m”, „3000,2000”). Întoarce false dacă nu e validă.
    Q_INVOKABLE bool applyMeasurement(const QString& text);

    // --- Organizare (Faza 6). ID-urile vin din QML ca numere. ---
    // Organizarea (ca în Blender): obiect = geometrie încapsulată; grup = colecție de obiecte (folder în Outliner).
    Q_INVOKABLE void makeGroup(); // ⌘G: Creează obiect din formele selectate (cu doar obiecte selectate: Grupează)
    Q_INVOKABLE void makeComponent(const QString& name = {});
    Q_INVOKABLE void groupSelection(); // ⌥⌘G: Grupează obiectele selectate
    Q_INVOKABLE void duplicateLinked(); // ⌥D: copie legată (aceeași geometrie, editată împreună)
    // Drag & drop în Outliner: mută obiectul în grupul `targetInstance` (0 = în model), cu poziția păstrată.
    Q_INVOKABLE void moveInstanceTo(double instanceId, double targetInstance);
    // Selectează ce are tag-ul în contextul editat (obiecte, fețe, muchii).
    Q_INVOKABLE void selectByTag(double tagId);
    Q_INVOKABLE void explodeSelection();
    Q_INVOKABLE void makeUniqueSelection();
    Q_INVOKABLE void hideSelection();
    Q_INVOKABLE void unhideAll();
    Q_INVOKABLE void setSelectionLocked(bool locked);
    // Intră în instanța dată pentru editare (0 = cea selectată); iese un nivel.
    Q_INVOKABLE void enterInstance(double instanceId = 0);
    Q_INVOKABLE void exitContext();
    // Outliner: selectează, redenumește, ascunde sau arată o instanță.
    Q_INVOKABLE void selectInstance(double instanceId);
    Q_INVOKABLE void renameInstance(double instanceId, const QString& name);
    Q_INVOKABLE void setInstanceHidden(double instanceId, bool hidden);
    Q_INVOKABLE void setInstanceLocked(double instanceId, bool locked);
    // Tag-uri.
    Q_INVOKABLE void addTag(const QString& name);
    Q_INVOKABLE void renameTag(double tagId, const QString& name);
    Q_INVOKABLE void setTagVisible(double tagId, bool visible);
    Q_INVOKABLE void removeTag(double tagId);
    // Tag-ul instanțelor selectate (0 = fără tag).
    Q_INVOKABLE void assignTag(double tagId);

    // --- Fișiere ---
    Q_INVOKABLE void newDocument();
    Q_INVOKABLE bool openFile(const QString& path);
    // Salvează (`path` gol = fișierul curent); `thumbnailPng` e imaginea de previzualizare din arhivă.
    Q_INVOKABLE bool saveFile(const QString& path, const QByteArray& thumbnailPng = {});
    // Import după extensie (.obj, .gltf, .glb), ca grup în contextul editat.
    Q_INVOKABLE bool importFile(const QString& path);
    // Export după extensie: .obj (+ .mtl alături), .gltf, .glb.
    Q_INVOKABLE bool exportFile(const QString& path);
    Q_INVOKABLE void recoverAutosave();
    Q_INVOKABLE void discardAutosave();
    Q_INVOKABLE void clearRecentFiles();
    // Calea locală dintr-un URL de fișier (dialogurile QML dau URL-uri).
    Q_INVOKABLE QString localPath(const QUrl& url) const;

    // --- Materiale ---
    // Inspector: plasarea instanței selectate (px, py, pz, rotation, sx, sy, sz, dx, dy, dz; dimensiunile au
    // prioritate față de scară) și materialul selecției. `material`: { id } din model sau { library } din bibliotecă,
    // gol = fără material; `side`: "front" sau "back" (pentru fețe).
    Q_INVOKABLE void setPlacement(const QVariantMap& placement);
    Q_INVOKABLE void assignMaterial(const QVariantMap& material, const QString& side);
    Q_INVOKABLE void selectLibraryMaterial(int index);
    Q_INVOKABLE void selectModelMaterial(double materialId);
    Q_INVOKABLE void selectDefaultMaterial();
    Q_INVOKABLE void renameMaterial(double materialId, const QString& name);
    Q_INVOKABLE void setMaterialColor(double materialId, const QColor& color);
    Q_INVOKABLE void removeMaterial(double materialId);

    // --- Faza 9: intersecții, booleene, ghidaje, adnotări, secțiuni, scene ---
    Q_INVOKABLE void intersectSelection(bool withModel);
    // "union", "subtract", "intersect" pe cele două obiecte selectate.
    Q_INVOKABLE void booleanSelection(const QString& kind);
    Q_INVOKABLE void deleteGuides();
    Q_INVOKABLE void setAnnotationText(double annotationId, const QString& text);
    Q_INVOKABLE void removeAnnotation(double annotationId);
    Q_INVOKABLE void setSectionActive(double sectionId, bool active);
    Q_INVOKABLE void moveSection(double sectionId, double distance);
    Q_INVOKABLE void removeSection(double sectionId);
    // camera: { tx, ty, tz, yaw, pitch, distance, ortho } (Viewport.cameraState()).
    Q_INVOKABLE void addScene(const QVariantMap& camera, const QString& style);
    Q_INVOKABLE void updateScene(double sceneId, const QVariantMap& camera, const QString& style);
    Q_INVOKABLE void renameScene(double sceneId, const QString& name);
    Q_INVOKABLE void removeScene(double sceneId);
    Q_INVOKABLE QVariantMap sceneCamera(double sceneId) const;

    // --- Meniul contextual (click dreapta) ---
    // Elementele potrivite selecției: { id, name, shortcut, enabled, destructive } sau { separator: true }.
    // `id` e o comandă din Main.qml (runCommand).
    Q_INVOKABLE QVariantList contextMenuItems() const;
    Q_INVOKABLE void reverseSelectedFaces();
    Q_INVOKABLE void selectConnected();

    // --- Pentru Viewport ---
    const model::Model& model() const { return m_model; }
    model::DefinitionId activeDefinition() const;
    const std::vector<model::InstanceId>& path() const { return m_path; }
    // Transformarea contextului editat (local → global).
    math::Transform contextTransform() const;
    const geom::Mesh& mesh() const { return m_model.mesh(activeDefinition()); }
    const geom::Selection& selection() const { return m_selection; }
    const tools::Tool* tool() const { return m_tool.get(); }
    bool hasTool() const { return m_tool != nullptr; }
    // Triangularea fiecărei fețe a unei definiții (coordonate locale), refăcută numai pentru fețele schimbate.
    const std::unordered_map<geom::FaceId, geom::FaceTriangles>& triangles(model::DefinitionId definition) const;
    // Crește la orice schimbare care trebuie redesenată (model, selecție, hover, overlay).
    unsigned revision() const { return m_revision; }

    // Click dreapta: ce e sub cursor devine selecția, dacă nu era deja selectat (ca în Figma).
    void selectForContextMenu(const view::Camera& camera, math::Vec2d screen);
    // Cutia selecției în coordonate globale (pentru „Zoom la selecție”); nullopt dacă selecția e goală.
    std::optional<std::pair<math::Vec3d, math::Vec3d>> selectionBounds() const;
    // --- Interacțiuni (unealta Interact) ---
    // Click: comută instanța interactivă de sub cursor, cu animație. Întoarce false dacă nu e nimic interactiv.
    bool interactAt(const view::Camera& camera, math::Vec2d screen);
    // Avansează animațiile; întoarce true cât timp mai există una (viewport-ul cere cadre).
    bool advanceInteractions(double dt);
    // Transformarea de afișat pentru o instanță în mișcare (altfel nullopt: cea din model).
    std::optional<math::Transform> displayTransform(model::InstanceId instance) const;
    unsigned interactionRevision() const { return m_interactionRevision; }
    // Comportamentul instanței selectate: kind ("none", "rotate", "slide"), anchor, amount, duration.
    Q_INVOKABLE void setBehavior(const QVariantMap& behavior);
    // --- Timeline ---
    QVariantList animations() const;
    double activeAnimation() const { return static_cast<double>(m_activeAnimation.value); }
    void setActiveAnimation(double id);
    // Pistele animației active: { name, duration, fps, loop, tracks: [{ instance, name, transform: [t…], color: [t…] }],
    // camera: [t…] }.
    QVariantMap timeline() const;
    double timelineTime() const { return m_timelineTime; }
    void setTimelineTime(double seconds); // mută playhead-ul și pornește previzualizarea
    bool previewing() const { return m_previewing; }
    bool playing() const { return m_playing; }
    Q_INVOKABLE void createAnimation();
    Q_INVOKABLE void renameAnimation(double id, const QString& name);
    Q_INVOKABLE void removeAnimation(double id);
    Q_INVOKABLE void setAnimationSettings(double duration, int fps, bool loop);
    // Keyframe-uri la momentul curent: transformarea / culoarea obiectelor selectate, camera dată (cameraState()).
    Q_INVOKABLE void keySelection();
    Q_INVOKABLE void keySelectionColor();
    Q_INVOKABLE void keyCamera(const QVariantMap& camera);
    // kind: "transform", "color", "camera"; instance 0 pentru cameră.
    Q_INVOKABLE void removeKey(const QString& kind, double instance, double time);
    Q_INVOKABLE void moveKey(const QString& kind, double instance, double time, double newTime);
    Q_INVOKABLE void setKeyEasing(const QString& kind, double instance, double time, int easing);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void endPreview(); // înapoi la model (editare)
    // Camera animației la momentul curent ({} dacă nu are pistă de cameră), ca cameraState().
    Q_INVOKABLE QVariantMap animationCamera() const;
    // Culoarea animată a unei instanțe (pentru randare), dacă există.
    std::optional<model::Color> displayColor(model::InstanceId instance) const;
    const model::Animation* currentAnimation() const;
    double animationDuration() const;

    // Aplică starea unei scene (pozițiile obiectelor, ascunderile, tag-urile), cu o tranziție de `seconds`.
    Q_INVOKABLE void applyScene(double sceneId, double seconds);
    bool animating() const { return m_sceneTransition.has_value() || !m_interactions.empty(); }

    // Punctul de pe model (fețe, muchii, grupuri și obiecte vizibile) de sub cursor, în coordonate globale.
    // `camera` e camera globală a viewport-ului. Pentru zoom spre suprafață și orbita în jurul cursorului.
    std::optional<math::Vec3d> surfacePoint(const view::Camera& camera, math::Vec2d screen) const;

    void pointerDown(const view::Camera& camera, const tools::PointerEvent& e);
    void pointerMove(const view::Camera& camera, const tools::PointerEvent& e);
    void pointerUp(const view::Camera& camera, const tools::PointerEvent& e);

signals:
    void activeToolChanged();
    void toolStatusChanged();
    void historyChanged();
    void selectionChanged();
    void modelChanged();
    void outlineChanged();
    void contextChanged();
    void fileChanged();
    void recentFilesChanged();
    void recoveryChanged();
    void errorChanged();
    void materialChanged();
    void editingAnnotationChanged();
    void unitsChanged();
    void gridSnapChanged();
    void autosaveSettingsChanged();
    // A pornit o animație a modelului (tranziția unei scene): viewport-ul cere cadre.
    void animationStarted();
    void timelineChanged();
    void timelineTimeChanged();
    // Un model nou a înlocuit documentul (deschidere, document nou, recuperare): viewport-ul îl încadrează.
    void documentReplaced();
    // Orice schimbare vizibilă în viewport.
    void changed();

private:
    struct InteractionAnimation {
        model::InstanceId instance;
        double elapsed = 0.0;
        double duration = 0.6;
        bool opening = true;
    };
    std::vector<InteractionAnimation> m_interactions;
    struct SceneTransition {
        model::SceneId scene;
        std::map<model::InstanceId, std::pair<math::Transform, math::Transform>> moves;
        double elapsed = 0.0;
        double duration = 1.0;
    };
    std::optional<SceneTransition> m_sceneTransition;
    model::AnimationId m_activeAnimation;
    double m_timelineTime = 0.0;
    bool m_previewing = false;
    bool m_playing = false;
    template <class Fn>
    void editAnimation(const char* name, Fn&& fn);
    void finishSceneTransition();
    unsigned m_interactionRevision = 0;

    bool m_snapToGrid = false;
    double m_gridStep = 0.0;

    void meshChanged(model::DefinitionId definition, const geom::Mesh& mesh, const geom::ChangeSummary& summary) override;
    void structureChanged(const model::Model& model) override;
    template <class Fn>
    void withTool(const view::Camera& camera, Fn&& fn);
    void afterInteraction(const geom::Selection& before);
    // După undo/redo sau ștergeri: calea și selecția nu trebuie să indice entități dispărute.
    void repairContext();
    // Varianta sigură în mijlocul unei notificări de la model (fără să anuleze unealta).
    void dropStaleReferences();
    void updateHiddenTags();
    QVariantMap materialInfo(model::MaterialId id) const;
    std::size_t validPathLength() const;
    bool m_contextLost = false;
    void setPath(std::vector<model::InstanceId> path);
    void retriangulate(model::DefinitionId definition);
    // Rulează o comandă pe model și aduce la zi selecția și contextul.
    template <class Fn>
    void runOnModel(const std::string& name, Fn&& fn);
    void replayPointer();
    void touch();
    void replaceModel(const model::Model& model);
    void setFilePath(const QString& path);
    void addRecentFile(const QString& path);
    void setError(const QString& message);
    void autosave();
    void removeAutosave();
    static QString autosavePath();

    model::Model m_model;
    commands::CommandStack m_stack { m_model };
    std::vector<model::InstanceId> m_path;
    geom::Selection m_selection;
    std::unique_ptr<tools::Tool> m_tool;
    QString m_toolId;
    std::map<model::DefinitionId, std::unordered_map<geom::FaceId, geom::FaceTriangles>> m_triangles;
    unsigned m_revision = 1;
    view::Camera m_lastCamera;
    std::optional<tools::PointerEvent> m_lastPointer;
    QString m_filePath;
    QString m_lastError;
    bool m_hasRecovery = false;
    QTimer m_autosaveTimer;
    model::MaterialId m_currentMaterial;
    model::AnnotationId m_editingAnnotation;
    std::vector<inference::GuideRef> m_contextGuides;
    int m_libraryMaterial = -1;
};

} // namespace aion::render
