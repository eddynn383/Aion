#pragma once

#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/model/Model.h"
#include "aion/core/tools/Tool.h"

#include <QObject>
#include <QString>
#include <QStringList>
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
};

} // namespace aion::render
