#pragma once

#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/tools/Tool.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>
#include <unordered_map>

namespace aion::render {

// Documentul deschis: modelul, istoricul undo/redo, selecția și unealta activă.
// Puntea dintre nucleu (fără Qt) și interfață: QML îl folosește pentru comenzi și stare,
// iar Viewport-ul îi trimite evenimentele de mouse și desenează din el.
class Document : public QObject, private geom::MeshObserver {
    Q_OBJECT
    QML_ELEMENT

    // "select", "line", "rectangle", "eraser"; alte valori = nicio unealtă de modelare activă.
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
    int faceCount() const { return static_cast<int>(m_mesh.faces().size()); }
    int edgeCount() const { return static_cast<int>(m_mesh.edges().size()); }

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    // Esc: anulează operația uneltei; dacă nu era nimic de anulat, unealta decide (Select deselectează).
    Q_INVOKABLE void escape();
    Q_INVOKABLE void enter();
    // "left", "right", "up", "down": lock-ul inferenței pe o axă. Întoarce true dacă unealta a folosit tasta.
    Q_INVOKABLE bool arrowKey(const QString& direction);
    // Valoarea din Measurements box („2500”, „2.5m”, „3000,2000”). Întoarce false dacă nu e validă.
    Q_INVOKABLE bool applyMeasurement(const QString& text);

    // --- Pentru Viewport ---
    const geom::Mesh& mesh() const { return m_mesh; }
    const geom::Selection& selection() const { return m_selection; }
    const tools::Tool* tool() const { return m_tool.get(); }
    bool hasTool() const { return m_tool != nullptr; }
    // Triangularea fiecărei fețe, refăcută numai pentru fețele schimbate.
    const std::unordered_map<geom::FaceId, geom::FaceTriangles>& triangles() const { return m_triangles; }
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
    // Orice schimbare vizibilă în viewport.
    void changed();

private:
    void meshChanged(const geom::Mesh& mesh, const geom::ChangeSummary& summary) override;
    template <class Fn>
    void withTool(const view::Camera& camera, Fn&& fn);
    void afterInteraction(const geom::Selection& before);
    // După o tastă, unealta reia ultima poziție a cursorului, ca previzualizarea să fie la zi.
    void replayPointer();
    void touch();

    geom::Mesh m_mesh;
    commands::CommandStack m_stack { m_mesh };
    geom::Selection m_selection;
    std::unique_ptr<tools::Tool> m_tool;
    QString m_toolId;
    std::unordered_map<geom::FaceId, geom::FaceTriangles> m_triangles;
    unsigned m_revision = 1;
    view::Camera m_lastCamera;
    std::optional<tools::PointerEvent> m_lastPointer;
};

} // namespace aion::render
