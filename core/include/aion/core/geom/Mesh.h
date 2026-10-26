#pragma once

#include "aion/core/geom/ChangeSet.h"
#include "aion/core/geom/SlotMap.h"
#include "aion/core/geom/Topology.h"

#include <optional>
#include <string>
#include <vector>

namespace aion::geom {

struct ValidationReport {
    std::vector<std::string> errors;

    [[nodiscard]] bool ok() const { return errors.empty(); }
    // Toate erorile pe câte un rând (pentru mesajele din teste).
    [[nodiscard]] std::string text() const;
};

enum class ValidationLevel {
    Topology, // referințe, bucle închise, planaritate, orientare: O(n)
    Full, // plus vârfuri distincte și muchii care nu se intersectează: O(n²), pentru teste
};

// Geometria unui context de modelare: vârfuri, muchii, fețe.
//
// Modificările trec numai prin primitivele de mai jos (operațiile Euler). Când înregistrarea e
// activă, fiecare primitivă salvează starea entităților atinse, iar endRecording() întoarce
// ChangeSet-ul folosit pentru undo/redo. Primitivele nu verifică regulile geometrice de nivel
// înalt (de exemplu că o față e plană); pentru asta există validate().
class Mesh {
public:
    // --- Citire ---
    [[nodiscard]] const SlotMap<VertexId, Vertex>& vertices() const { return m_vertices; }
    [[nodiscard]] const SlotMap<EdgeId, Edge>& edges() const { return m_edges; }
    [[nodiscard]] const SlotMap<FaceId, Face>& faces() const { return m_faces; }

    [[nodiscard]] const Vertex& vertex(VertexId id) const { return m_vertices.at(id); }
    [[nodiscard]] const Edge& edge(EdgeId id) const { return m_edges.at(id); }
    [[nodiscard]] const Face& face(FaceId id) const { return m_faces.at(id); }
    [[nodiscard]] const Vec3d& position(VertexId id) const { return m_vertices.at(id).position; }

    [[nodiscard]] EdgeId findEdge(VertexId a, VertexId b) const;
    // Vârful aflat la cel mult `tolerance` de punct (cel mai apropiat), dacă există.
    [[nodiscard]] VertexId findVertexNear(const Vec3d& p, double tolerance) const;

    [[nodiscard]] VertexId useStart(const EdgeUse& use) const;
    [[nodiscard]] VertexId useEnd(const EdgeUse& use) const;
    [[nodiscard]] std::vector<VertexId> loopVertices(const Loop& loop) const;
    [[nodiscard]] std::vector<Vec3d> loopPoints(const Loop& loop) const;

    // --- Primitive (operații Euler) ---
    VertexId addVertex(const Vec3d& position);
    // Vârful trebuie să nu mai aibă muchii.
    void removeVertex(VertexId id);
    // Între a și b nu trebuie să existe deja o muchie.
    EdgeId addEdge(VertexId a, VertexId b);
    // Muchia trebuie să nu mai fie folosită de nicio față.
    void removeEdge(EdgeId id);
    FaceId addFace(const math::Plane& plane, Loop outer, std::vector<Loop> inner = {}, model::MaterialId material = {});
    void setFaceMaterial(FaceId id, model::MaterialId material);
    void removeFace(FaceId id);
    // Mută un vârf (fără verificări: fețele atinse trebuie apoi aduse la zi, vezi transformVertices).
    void setPosition(VertexId id, const Vec3d& position);
    // Stilul unei muchii (vezi Edge): soft, curba din care face parte.
    void setEdgeStyle(EdgeId id, bool soft, std::uint32_t curve, bool smoothCurve);
    // Un identificator de curbă nefolosit.
    [[nodiscard]] std::uint32_t newCurveId() const;
    // Schimbă planul unei fețe (de exemplu după ce i s-au mutat vârfurile).
    void setFacePlane(FaceId id, const math::Plane& plane);
    // Înlocuiește muchia cu două muchii prin vârful `at` și actualizează buclele fețelor.
    // Întoarce (muchia dinspre v0, muchia dinspre v1).
    std::pair<EdgeId, EdgeId> splitEdge(EdgeId id, VertexId at);

    // --- Înregistrarea schimbărilor ---
    void beginRecording();
    [[nodiscard]] bool isRecording() const { return m_recording.has_value(); }
    ChangeSet endRecording();
    // Aplică o înregistrare înainte (redo) sau înapoi (undo).
    void apply(const ChangeSet& changes, ChangeDirection direction);

    void addObserver(MeshObserver* observer);
    void removeObserver(MeshObserver* observer);

    // --- Verificări ---
    [[nodiscard]] ValidationReport validate(ValidationLevel level = ValidationLevel::Full) const;
    // Aceleași entități, cu aceleași ID-uri și aceleași date (pentru testele de undo).
    [[nodiscard]] bool sameAs(const Mesh& other) const;

private:
    void touch(VertexId id);
    void touch(EdgeId id);
    void touch(FaceId id);
    void attachFace(FaceId id, const Face& face);
    void detachFace(FaceId id, const Face& face);
    void notify(const ChangeSummary& summary);

    SlotMap<VertexId, Vertex> m_vertices;
    SlotMap<EdgeId, Edge> m_edges;
    SlotMap<FaceId, Face> m_faces;
    std::optional<ChangeSet> m_recording;
    std::vector<MeshObserver*> m_observers;
};

} // namespace aion::geom
