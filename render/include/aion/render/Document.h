#pragma once

#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Rules.h"
#include "aion/core/tools/Tool.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVector3D>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <map>
#include <tuple>
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
    // Generatoarele de obiecte dinamice: { id, name, category, parameters: [{ key, label, unit, value, min, max, step }] }.
    Q_PROPERTY(QVariantList generators READ generators CONSTANT)
    // Camerele și luminile: { id, name, kind, enabled, visible } (pentru Outliner).
    // Obiectele cu interacțiuni: { id, name, rules: [{ trigger, summary }] } (secțiunea Interacțiuni).
    Q_PROPERTY(QVariantList interactions READ interactions NOTIFY outlineChanged)
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY outlineChanged)
    Q_PROPERTY(QVariantList lights READ lights NOTIFY outlineChanged)
    // Obiectul selectat (cameră sau lumină) cu toate proprietățile; { kind: "none" } dacă nu e niciunul.
    Q_PROPERTY(QVariantMap selectedObject READ selectedObject NOTIFY selectionChanged)
    // Scenele: { id, name, style } (poziția camerei o citește QML prin sceneCamera).
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY outlineChanged)
    // Timeline-ul: animațiile modelului, cea activă, momentul curent, previzualizarea și redarea.
    Q_PROPERTY(QVariantList animations READ animations NOTIFY outlineChanged)
    Q_PROPERTY(double activeAnimation READ activeAnimation WRITE setActiveAnimation NOTIFY timelineChanged)
    Q_PROPERTY(QVariantMap timeline READ timeline NOTIFY timelineChanged)
    Q_PROPERTY(double timelineTime READ timelineTime WRITE setTimelineTime NOTIFY timelineTimeChanged)
    Q_PROPERTY(bool previewing READ previewing NOTIFY timelineChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY timelineChanged)
    // Săgețile sunt ale uneltei active (o operație în curs); altfel, în Animare, ← → mută playhead-ul.
    Q_PROPERTY(bool toolOwnsArrows READ toolOwnsArrows NOTIFY toolStatusChanged)
    // Valoarea fiecărui canal la timpul curent („id:proprietate” → număr sau culoare), urmând playhead-ul (fără să
    // refacă tot timeline-ul la fiecare cadru).
    Q_PROPERTY(QVariantMap channelValues READ channelValues NOTIFY playheadValuesChanged)
    // Pivotul selectat (click pe marcajul lui, ținut cât rămâne aceeași selecție): Inspector-ul îi arată coordonatele
    // (ca Poziția: în coordonatele contextului), editabile — mută doar pivotul, obiectul rămâne pe loc.
    Q_PROPERTY(QVariantList pivotPosition READ pivotPosition NOTIFY selectionChanged)
    // Auto-key (Animare): pornit, Move / Rotate / Scale / Inspector pun chei; oprit, schimbă poza de bază (iar
    // proprietățile animate își mută toate cheile).
    Q_PROPERTY(bool autoKey READ autoKey WRITE setAutoKey NOTIFY autoKeyChanged)
    // Modul de lucru: 0 Modelare, 1 Animare, 2 Interacțiune. Decide ce unelte, comenzi și operații sunt permise
    // (docs/plans/moduri-de-lucru.md). Nu e un pas de undo și nu se salvează în fișier.
    Q_PROPERTY(int workMode READ workMode WRITE setWorkMode NOTIFY workModeChanged)
    // Modul interacțiune: modelul nu se mai editează; evenimentele pornesc regulile obiectelor.
    Q_PROPERTY(bool interactionMode READ interactionMode WRITE setInteractionMode NOTIFY interactionModeChanged)
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
    QVariantList cameras() const;
    QVariantList interactions() const;
    QVariantList generators() const;
    QVariantList lights() const;
    QVariantMap selectedObject() const;
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
    // Lacătul unei valori de plasare a instanței selectate (bitul din model::PlacementLock, 0–11).
    Q_INVOKABLE void setPlacementLock(int bit, bool locked);
    // Animația activă are un keyframe de transformare pentru instanță la momentul curent al timeline-ului?
    Q_INVOKABLE bool hasTransformKey(double instanceId) const;
    Q_INVOKABLE void assignMaterial(const QVariantMap& material, const QString& side);
    Q_INVOKABLE void selectLibraryMaterial(int index);
    Q_INVOKABLE void selectModelMaterial(double materialId);
    Q_INVOKABLE void selectDefaultMaterial();
    Q_INVOKABLE void renameMaterial(double materialId, const QString& name);
    Q_INVOKABLE void setMaterialColor(double materialId, const QColor& color);
    Q_INVOKABLE void removeMaterial(double materialId);
    // Un material nou (personalizat): { name, color: "#rrggbb", opacity 0–1, roughness 0–1, metallic 0–1 }.
    // Devine materialul curent (Paint Bucket). Întoarce id-ul.
    Q_INVOKABLE double createMaterial(const QVariantMap& properties);
    // Schimbă proprietățile date (aceleași chei ca la createMaterial).
    Q_INVOKABLE void updateMaterial(double materialId, const QVariantMap& properties);

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

    // --- Camere și lumini (obiecte ale modelului) ---
    // camera: ca la addScene; fov în grade. Camera nouă privește ca vederea curentă și devine selectată.
    Q_INVOKABLE void addCamera(const QVariantMap& camera, double fovDegrees);
    // kind: "point", "spot", "directional", "area"; `at` = punctul spre care privește vederea (ținta camerei).
    Q_INVOKABLE void addLight(const QString& kind, const QVector3D& at);
    // kind: "camera" sau "light"; id 0 = nimic.
    Q_INVOKABLE void selectObject(const QString& kind, double id);
    // Schimbă proprietățile obiectului selectat (aceleași chei ca în selectedObject).
    Q_INVOKABLE void setObjectProperties(const QVariantMap& changes);
    // Proprietățile unei camere anume (vederea prin cameră o editează și când nu e selectată).
    Q_INVOKABLE void setCameraProperties(double cameraId, const QVariantMap& changes);
    // Proprietățile unei camere (ca selectedObject pentru o cameră); {} dacă nu există.
    Q_INVOKABLE QVariantMap cameraInfo(double cameraId) const;
    Q_INVOKABLE void deleteSelectedObject();
    // Vederea prin cameră: { tx, ty, tz, yaw, pitch, distance, ortho, fov }.
    Q_INVOKABLE QVariantMap cameraView(double cameraId) const;
    // Presetările de format ({ name, width, height }) și de senzor ({ name, width }) ale camerelor.
    Q_INVOKABLE QVariantList cameraFormatPresets() const;
    Q_INVOKABLE QVariantList cameraSensorPresets() const;
    // Mută camera sau lumina selectată în punctul dat (camera își mută și ținta, ca direcția să rămână aceeași).
    void moveSelectedObjectTo(const math::Vec3d& position);
    [[nodiscard]] std::optional<math::Vec3d> selectedObjectPosition() const;
    // Pune camera selectată (sau lumina) unde e vederea curentă.
    Q_INVOKABLE void placeSelectedObjectAtView(const QVariantMap& camera);
    // Pentru gizmo-urile din viewport: obiectul selectat (0 = niciunul).
    [[nodiscard]] model::CameraId selectedCamera() const { return m_selectedCamera; }
    [[nodiscard]] model::LightId selectedLight() const { return m_selectedLight; }

    // --- Meniul contextual (click dreapta) ---
    // Elementele potrivite selecției: { id, name, shortcut, enabled, destructive } sau { separator: true }.
    // `id` e o comandă din Main.qml (runCommand).
    Q_INVOKABLE QVariantList contextMenuItems() const;
    Q_INVOKABLE void reverseSelectedFaces();

    // --- Modelare pe selecție (ADR 0023) ---
    // Oglindește selecția față de centrul ei, pe axa 0 (X), 1 (Y) sau 2 (Z).
    Q_INVOKABLE void mirrorSelection(int axis);
    // Forma selectată (linie, arc, cerc, poligon, dreptunghi): key = "length", "radius", "segments", "width", "height".
    Q_INVOKABLE void setShapeValue(const QString& key, double value);
    // Geometria liberă selectată (fețe, muchii): px/py/pz = colțul minim nou, dx/dy/dz = mărimea nouă (scară pe axă
    // față de colțul minim), rx/ry/rz = rotirea în grade (în jurul centrului), sx/sy/sz = scara (față de colțul minim).
    // Geometria nu are o orientare salvată: rotirea și scara sunt cele aplicate selecției curente din Inspector (se
    // pornește de la 0° / 1 când se schimbă selecția sau după undo); o valoare nouă aplică diferența.
    Q_INVOKABLE void setGeometryPlacement(const QVariantMap& changes);
    // Muchiile selectate devin soft (ascunse, umbrire netedă) sau vizibile.
    Q_INVOKABLE void setSelectedEdgesSoft(bool soft);
    // Reface fețele în buclele închise ale muchiilor selectate (după ce o față a fost ștearsă).
    Q_INVOKABLE void createFacesFromSelection();
    // Obiecte dinamice (ADR 0025): unul nou în punctul dat (pe sol), selectat; parametrul obiectului selectat.
    Q_INVOKABLE double createParametric(const QString& generator, const QVector3D& at, const QVariantMap& parameters = {});
    Q_INVOKABLE void setParameter(const QString& key, double value);

    // --- Biblioteca de obiecte (ADR 0026) ---
    // Elementele: { id ("gen:door" | "file:/cale.aion"), name, category, thumbnail (data URL, opțional), user }.
    Q_PROPERTY(QVariantList libraryItems READ libraryItems NOTIFY libraryChanged)
    QVariantList libraryItems() const;
    // Dosarul bibliotecii utilizatorului (fișiere .aion).
    Q_INVOKABLE QString libraryFolder() const;
    // Pune elementul în punctul dat (pe sol sau pe o suprafață), selectat.
    Q_INVOKABLE void placeLibraryItem(const QString& id, const QVector3D& at);
    // Salvează obiectul selectat în bibliotecă (cu miniatura dată, PNG). Întoarce false dacă nu se poate.
    Q_INVOKABLE bool saveSelectionToLibrary(const QString& name, const QByteArray& thumbnailPng);
    Q_INVOKABLE void removeLibraryItem(const QString& id);
    // Mărimea obiectului din bibliotecă (pentru previzualizarea la tragere): { dx, dy, dz } în mm, păstrată între apeluri.
    Q_INVOKABLE QVariantMap libraryItemSize(const QString& id) const;

    // --- Scripturi (ADR 0027) ---
    // Tot ce face un script între beginMacro și endMacro e un singur pas de undo (endMacro(false) îl anulează).
    void beginMacro(const std::string& name);
    void endMacro(bool keep);
    [[nodiscard]] bool inMacro() const { return m_macro; }
    // Operații pe obiecte după id (în contextul editat), folosite de API-ul scripturilor. Întorc id-ul creat (0 = eșec).
    double createPrimitive(const QString& kind, const QVector3D& corner, const QVector3D& size, const QString& name);
    bool setInstancePlacement(double instanceId, const QVariantMap& placement);
    bool transformInstance(double instanceId, const math::Transform& delta);
    bool removeInstanceById(double instanceId);
    bool setInstanceMaterialByName(double instanceId, const QString& nameOrColor);
    bool drawLineSegment(const QVector3D& a, const QVector3D& b);
    bool drawFace(const QVariantList& points);
    [[nodiscard]] QVariantList objectList() const;
    void selectInstances(const QVariantList& ids);
    // Poziționarea texturii pe fețele selectate: { ox, oy (mm), rotation (°), sx, sy } sau { reset: true }.
    Q_INVOKABLE void setFaceTexture(const QVariantMap& changes);
    // Array cu previzualizare: fiecare apel înlocuiește copiile de la apelul anterior (undo + refacere), până la
    // commitOperation() / cancelOperation(). options: { mode: "linear"|"polar", count, dx, dy, dz (pasul liniar, mm),
    // axis: 0|1|2, angle (grade între copii), rotate, aroundOrigin (altfel în jurul centrului selecției) }.
    Q_INVOKABLE void previewArray(const QVariantMap& options);
    // Soften / Smooth cu previzualizare: muchiile selecției (sau toate muchiile obiectelor selectate).
    Q_INVOKABLE void previewSoften(double maxAngleDegrees, bool includeCoplanar);
    // Bevel / chamfer cu previzualizare: muchiile selectate sau toate laturile feței selectate. segments = 1 e chamfer
    // (o teșitură plată), mai multe = o rotunjire. Un caz nepermis lasă modelul neschimbat și arată motivul.
    Q_INVOKABLE void previewBevel(double distance, int segments);
    Q_INVOKABLE void commitOperation();
    Q_INVOKABLE void cancelOperation();
    // Mijlocul selecției (pentru panoul Array): { x, y, z, dx, dy, dz } (centrul și mărimea cutiei).
    Q_INVOKABLE QVariantMap selectionBox() const;
    Q_INVOKABLE void selectConnected();

    // --- Pentru Viewport ---
    // În Modul interacțiune: copia pe care rulează interacțiunile (documentul rămâne neatins).
    const model::Model& model() const { return m_runtime ? *m_runtime : m_model; }
    model::DefinitionId activeDefinition() const;
    // Pivotul selecției (un obiect sau geometrie liberă), în coordonatele contextului activ; gol = nimic de arătat.
    [[nodiscard]] std::optional<math::Vec3d> selectionPivot() const;
    // Marcajul pivotului în coordonatele lumii ([x, y, z]; gol fără selecție), pentru scripturile de verificare.
    Q_INVOKABLE QVariantList pivotMarker() const;
    QVariantList pivotPosition() const;
    // { x, y, z } (oricare): coordonatele noi ale pivotului, în coordonatele contextului (pentru scripturi).
    Q_INVOKABLE void setPivotPosition(const QVariantMap& coordinates);
    // Tragerea marcajului pivotului (cu Select): până la eliberare lucrează unealta „pivot” (se prinde ca Move), apoi
    // revine Select. Punctul previzualizat e unde se desenează marcajul.
    void beginPivotDrag();
    [[nodiscard]] bool pivotDragging() const { return m_suspendedTool != nullptr && !curveEditing(); }
    // Editarea unei curbe Pen (dublu click cu Select): unealta `penedit` până la Esc / click în afară.
    void beginCurveEdit(model::PenCurveId id);
    [[nodiscard]] bool curveEditing() const { return static_cast<bool>(m_editCurve); }
    [[nodiscard]] std::optional<math::Vec3d> pivotDragPoint() const { return m_pivotDragPoint; }
    // Mută pivotul selecției în punctul dat (coordonatele contextului). Obiectul nu se mișcă.
    void setSelectionPivot(const math::Vec3d& point);
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
    // --- Modul interacțiune ---
    enum WorkMode { Modeling = 0, Animation = 1, Interaction = 2 };
    int workMode() const { return m_workMode; }
    void setWorkMode(int mode);
    // Operațiile modului dat sunt permise acum? (Modul curent e acela, sau rulează un script: scripturile sunt
    // cerute explicit de utilizator și ocolesc regula.)
    Q_INVOKABLE bool allows(int mode) const { return m_workMode == mode || m_macro; }
    // Comanda (id din palette: „edit.bevel”, „tool.line”, „mode.animate”…) e disponibilă în modul curent?
    // Sursa unică a regulii: palette, scurtăturile, meniurile și gărzile o folosesc.
    Q_INVOKABLE bool commandAllowed(const QString& id) const;
    // Modurile în care e disponibilă comanda.
    static QList<int> commandModes(const QString& id);
    // Gărzile operațiilor: false (și un mesaj) dacă modul dat nu e cel curent.
    bool require(int mode);
    bool interactionMode() const { return m_runtime.has_value(); }
    void setInteractionMode(bool on);
    // Cursorul pentru obiectul de sub mouse: "drag", "click", "context", "hover" sau gol (nimic interactiv).
    QString interactionHover(const view::Camera& camera, math::Vec2d screen);
    // event: "click", "rightClick", "doubleClick", "longPress". Întoarce true dacă a pornit ceva.
    bool interactionEvent(const view::Camera& camera, math::Vec2d screen, const QString& event);
    // Sub cursor e un obiect, dar nici el, nici cele din care face parte nu au interacțiuni (pentru un mesaj).
    bool objectWithoutInteractionsAt(const view::Camera& camera, math::Vec2d screen) const;
    bool interactionDragStart(const view::Camera& camera, math::Vec2d screen);
    void interactionDragMove(const view::Camera& camera, math::Vec2d screen);
    void interactionDragEnd();
    Q_INVOKABLE void interactionKey(const QString& key);
    // Editorul de blocuri: regulile unei instanțe ca liste de hărți ({ trigger, key, actions: [{ kind, target, x, y,
    // z, duration, material, animation, scene, first, second }] }); trigger și kind sunt nume („click”, „moveBy”…).
    Q_INVOKABLE QVariantList rulesOf(double instanceId) const;
    Q_INVOKABLE void setRules(double instanceId, const QVariantList& rules);

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
    // Keyframe-uri la momentul curent: toate canalele de transformare / culoarea obiectelor selectate, camera dată
    // (cameraState()).
    Q_INVOKABLE void keySelection();
    Q_INVOKABLE void keyInstance(double instance);
    Q_INVOKABLE void keySelectionColor();
    Q_INVOKABLE void keyCamera(const QVariantMap& camera);
    // Cheile de pe timeline, ca liste de referințe { instance, property, time }: instance 0 = camera; property ""
    // = rândul sumar al obiectului (toate canalele lui de la acel moment), altfel „position.x” … „color”.
    Q_INVOKABLE void removeKeys(const QVariantList& keys);
    // Mută cheile cu `delta` secunde; întoarce referințele noi (pentru selecție).
    Q_INVOKABLE QVariantList moveKeys(const QVariantList& keys, double delta);
    // easing: { name, x1, y1, x2, y2, step } sau numele unei presetări („easeInOut”).
    Q_INVOKABLE void setKeysEasing(const QVariantList& keys, const QVariant& easing);
    // Easing-ul cheilor noi din animația activă.
    Q_INVOKABLE void setDefaultEasing(const QVariant& easing);
    // Valoarea unei chei de pe un canal numeric; valoarea curentă a unui canal (scrie o cheie la timpul curent).
    Q_INVOKABLE void setKeyValue(const QVariantMap& key, double value);
    Q_INVOKABLE void setChannelValue(double instance, const QString& property, double value);
    // Presetările de easing (ale aplicației), ca listă de { name, x1, y1, x2, y2, step }.
    Q_INVOKABLE QVariantList easingPresets() const;
    // Copiere / lipire de chei: lipirea se face la timpul curent, pe aceleași canale (pe obiectul selectat, dacă
    // cheile copiate sunt ale unui singur alt obiect). Întoarce referințele cheilor lipite.
    Q_INVOKABLE void copyKeys(const QVariantList& keys);
    Q_INVOKABLE QVariantList pasteKeys();
    Q_INVOKABLE bool hasCopiedKeys() const { return !m_keyClipboard.empty(); }
    bool autoKey() const { return m_autoKey; }
    void setAutoKey(bool on);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlay();
    // Tastatura timeline-ului: `frames` cadre înainte / înapoi (oprește redarea); cheia următoare (+1) sau anterioară
    // (−1) a selecției (sau a tuturor, fără selecție); începutul / sfârșitul.
    Q_INVOKABLE void stepFrames(int frames);
    Q_INVOKABLE bool jumpToKey(int direction);
    Q_INVOKABLE void jumpToEdge(bool end);
    bool toolOwnsArrows() const;
    QVariantMap channelValues() const;
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
    void interactionModeChanged();
    void workModeChanged();
    void libraryChanged();
    // O regulă cere o vedere (mergi la scenă): viewport-ul mută camera.
    void cameraRequested(const QVariantMap& camera);
    void timelineTimeChanged();
    void playheadValuesChanged();
    void autoKeyChanged();
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
    // Operația cu previzualizare în curs (array, soften): selecția de dinainte și dacă e un pas de anulat.
    bool m_previewApplied = false;
    bool m_macro = false;
    bool m_keepPreview = false; // editAnimation: keyframe-urile nu ies din previzualizare
    // Rotirea și scara aplicate din Inspector geometriei libere selectate (cheia = muchiile și fețele selecției).
    std::vector<std::uint64_t> m_geometryKey;
    math::Vec3d m_geometryRotation { 0.0 };
    // Pivotul geometriei libere selectate (în lume), ales din Inspector; gol = centrul cutiei selecției.
    std::optional<math::Vec3d> m_geometryPivot;
    math::Vec3d m_geometryScale { 1.0 };
    [[nodiscard]] std::vector<std::uint64_t> geometrySelectionKey() const;
    geom::Selection m_previewSelection;
    model::CameraId m_selectedCamera;
    model::LightId m_selectedLight;
    std::optional<model::Model> m_runtime;
    int m_workMode = Modeling;
    // Modul Animare: o copie a modelului cu obiectele animate în poza de la timpul curent. Uneltele de transformare
    // și Inspector-ul lucrează pe ea; rezultatul devine keyframe-uri (modelul de bază nu se schimbă). Se reface doar
    // când se schimbă modelul (m_modelRevision: crește la orice schimbare a istoricului — m_stack.setOnChanged —,
    // în runOnModel și la înlocuirea modelului), timpul sau animația.
    mutable std::optional<model::Model> m_pose;
    mutable std::tuple<std::uint64_t, double, std::uint64_t> m_poseKey { ~std::uint64_t { 0 }, 0.0, 0 };
    std::uint64_t m_modelRevision = 0;
    model::Model& pose() const;
    // Transformările noi ale obiectelor devin keyframe-uri la timpul curent (un pas de undo; creează animația).
    // Auto-key: transformările noi ale obiectelor (la timpul curent) devin chei, doar pe canalele schimbate.
    void keyTransforms(const std::map<model::InstanceId, math::Transform>& transforms, const char* name);
    // Valorile de transformare ale instanței la timpul curent (canalele animației peste modelul de bază).
    [[nodiscard]] model::TransformValues currentValues(model::InstanceId instance) const;
    // Transformarea instanței așa cum se vede: în Animare, poza de la timpul curent (fără copia modelului, deci
    // merge și în timpul unei tranzacții); altfel cea din model.
    [[nodiscard]] math::Transform shownTransform(model::InstanceId instance) const;
    [[nodiscard]] QString channelValueText(model::Property property, double value) const;
    model::RuleRuntime m_rules;
    model::InstanceId m_hoverInstance;
    struct DragState {
        model::InstanceId instance;
        math::Vec3d last;
        double planeZ = 0.0;
    };
    std::optional<DragState> m_drag;
    model::InstanceId ruleTargetAt(const view::Camera& camera, math::Vec2d screen) const;
    void handleRuleRequests();
    model::AnimationId m_activeAnimation;
    double m_timelineTime = 0.0;
    bool m_previewing = false;
    bool m_playing = false;
    bool m_autoKey = true;
    std::vector<model::ClipboardKey> m_keyClipboard;
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
    // Undo / redo: pasul din istoric și tot ce depinde de model (animația activă, timeline-ul, panourile).
    template <class Fn>
    void stepHistory(Fn&& step);
    // După undo/redo sau ștergeri: calea și selecția nu trebuie să indice entități dispărute.
    void repairContext();
    // Varianta sigură în mijlocul unei notificări de la model (fără să anuleze unealta).
    void dropStaleReferences();
    void updateHiddenTags();
    QVariantMap materialInfo(model::MaterialId id) const;
    // Miniatura texturii (data URL), păstrată între apeluri.
    QString textureThumbnail(model::TextureId id) const;
    mutable std::map<std::uint64_t, std::pair<std::size_t, QString>> m_thumbnails;
    mutable std::map<QString, QVariantMap> m_librarySizes;
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
    // Cât se trage pivotul: unealta aleasă (Select), pusă deoparte, și punctul previzualizat (în context).
    std::unique_ptr<tools::Tool> m_suspendedTool;
    std::optional<math::Vec3d> m_pivotDragPoint;
    void finishPivotDrag(const std::optional<math::Vec3d>& point);
    model::PenCurveId m_editCurve; // curba Pen editată (cu `penedit`), dacă există
    void finishCurveEdit();
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
