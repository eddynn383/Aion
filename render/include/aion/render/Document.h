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

    // Ultima eroare de fișier (pentru un mesaj în interfață).
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

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
    QVariantList libraryMaterials() const;
    QVariantList modelMaterials() const;
    QVariantMap currentMaterial() const;

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
    Q_INVOKABLE void makeGroup();
    Q_INVOKABLE void makeComponent(const QString& name = {});
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
    Q_INVOKABLE void selectLibraryMaterial(int index);
    Q_INVOKABLE void selectModelMaterial(double materialId);
    Q_INVOKABLE void selectDefaultMaterial();
    Q_INVOKABLE void renameMaterial(double materialId, const QString& name);
    Q_INVOKABLE void setMaterialColor(double materialId, const QColor& color);
    Q_INVOKABLE void removeMaterial(double materialId);

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
    // Un model nou a înlocuit documentul (deschidere, document nou, recuperare): viewport-ul îl încadrează.
    void documentReplaced();
    // Orice schimbare vizibilă în viewport.
    void changed();

private:
    void meshChanged(model::DefinitionId definition, const geom::Mesh& mesh, const geom::ChangeSummary& summary) override;
    void structureChanged(const model::Model& model) override;
    template <class Fn>
    void withTool(const view::Camera& camera, Fn&& fn);
    void afterInteraction(const geom::Selection& before);
    // După undo/redo sau ștergeri: calea și selecția nu trebuie să indice entități dispărute.
    void repairContext();
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
    int m_libraryMaterial = -1;
};

} // namespace aion::render
