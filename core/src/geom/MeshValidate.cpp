#include "forma/core/geom/Mesh.h"
#include "forma/core/math/Intersect.h"
#include "forma/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <set>
#include <sstream>

namespace forma::geom {

namespace {

class Reporter {
public:
    explicit Reporter(ValidationReport& report)
        : m_report(report)
    {
    }

    template <class... Args>
    void fail(Args&&... args)
    {
        std::ostringstream s;
        (s << ... << args);
        m_report.errors.push_back(s.str());
    }

private:
    ValidationReport& m_report;
};

template <class T>
bool hasDuplicates(std::vector<T> v)
{
    std::sort(v.begin(), v.end());
    return std::adjacent_find(v.begin(), v.end()) != v.end();
}

// Toleranța pentru planaritate: puțin peste kLength, ca erorile de rotunjire ale
// calculelor cu planuri să nu fie raportate ca fețe strâmbe.
constexpr double kPlanarTolerance = tol::kLength * 4.0;

} // namespace

ValidationReport Mesh::validate(ValidationLevel level) const
{
    ValidationReport report;
    Reporter r(report);

    // --- Vârfuri ---
    for (std::size_t i = 0; i < m_vertices.size(); ++i) {
        const VertexId id = m_vertices.keys()[i];
        const Vertex& v = m_vertices.values()[i];
        if (v.edges.empty()) {
            r.fail("vârful ", id.value, " nu are muchii (vârf izolat)");
        }
        if (hasDuplicates(v.edges)) {
            r.fail("vârful ", id.value, " are o muchie listată de două ori");
        }
        for (EdgeId e : v.edges) {
            const Edge* edge = m_edges.find(e);
            if (edge == nullptr) {
                r.fail("vârful ", id.value, " referă muchia inexistentă ", e.value);
            } else if (!edge->hasVertex(id)) {
                r.fail("vârful ", id.value, " listează muchia ", e.value, " care nu îl conține");
            }
        }
    }

    // --- Muchii ---
    std::set<std::pair<std::uint64_t, std::uint64_t>> vertexPairs;
    for (std::size_t i = 0; i < m_edges.size(); ++i) {
        const EdgeId id = m_edges.keys()[i];
        const Edge& e = m_edges.values()[i];
        const Vertex* a = m_vertices.find(e.v0);
        const Vertex* b = m_vertices.find(e.v1);
        if (a == nullptr || b == nullptr) {
            r.fail("muchia ", id.value, " referă un vârf inexistent");
            continue;
        }
        if (e.v0 == e.v1) {
            r.fail("muchia ", id.value, " are capetele identice");
        }
        if (glm::length(a->position - b->position) <= tol::kLength) {
            r.fail("muchia ", id.value, " are lungimea sub toleranță");
        }
        if (std::find(a->edges.begin(), a->edges.end(), id) == a->edges.end()
            || std::find(b->edges.begin(), b->edges.end(), id) == b->edges.end()) {
            r.fail("muchia ", id.value, " lipsește din lista unui capăt");
        }
        const auto key = std::minmax(e.v0.value, e.v1.value);
        if (!vertexPairs.insert(key).second) {
            r.fail("muchie duplicată între vârfurile ", key.first, " și ", key.second);
        }
        if (hasDuplicates(e.faces)) {
            r.fail("muchia ", id.value, " are o față listată de două ori");
        }
        for (FaceId f : e.faces) {
            const Face* face = m_faces.find(f);
            if (face == nullptr) {
                r.fail("muchia ", id.value, " referă fața inexistentă ", f.value);
                continue;
            }
            bool used = false;
            const auto scan = [&](const Loop& loop) {
                for (const EdgeUse& u : loop.uses) {
                    used = used || u.edge == id;
                }
            };
            scan(face->outer);
            std::for_each(face->inner.begin(), face->inner.end(), scan);
            if (!used) {
                r.fail("muchia ", id.value, " listează fața ", f.value, " care nu o folosește");
            }
        }
    }

    // --- Fețe ---
    std::set<std::vector<EdgeId>> faceEdgeSets;
    for (std::size_t i = 0; i < m_faces.size(); ++i) {
        const FaceId id = m_faces.keys()[i];
        const Face& f = m_faces.values()[i];
        if (!tol::equal(glm::length(f.plane.normal), 1.0, 1e-9)) {
            r.fail("fața ", id.value, " are normala nenormalizată");
        }
        if (f.outer.uses.size() < 3) {
            r.fail("fața ", id.value, " are bucla exterioară cu mai puțin de 3 muchii");
            continue;
        }

        const math::PlaneFrame frame(f.plane);
        std::vector<EdgeId> allEdges;
        std::vector<math::Vec2d> outer2d;
        bool loopsValid = true;

        const auto checkLoop = [&](const Loop& loop, bool isOuter) {
            if (loop.uses.size() < 3) {
                r.fail("fața ", id.value, " are o buclă cu mai puțin de 3 muchii");
                loopsValid = false;
                return;
            }
            for (std::size_t k = 0; k < loop.uses.size(); ++k) {
                const EdgeUse& use = loop.uses[k];
                const Edge* edge = m_edges.find(use.edge);
                if (edge == nullptr) {
                    r.fail("fața ", id.value, " folosește muchia inexistentă ", use.edge.value);
                    loopsValid = false;
                    return;
                }
                if (std::find(edge->faces.begin(), edge->faces.end(), id) == edge->faces.end()) {
                    r.fail("fața ", id.value, " lipsește din lista feței a muchiei ", use.edge.value);
                }
                const EdgeUse& next = loop.uses[(k + 1) % loop.uses.size()];
                if (m_edges.find(next.edge) != nullptr && useEnd(use) != useStart(next)) {
                    r.fail("fața ", id.value, ": bucla nu e închisă la muchia ", use.edge.value);
                    loopsValid = false;
                }
                allEdges.push_back(use.edge);
            }
            if (!loopsValid) {
                return;
            }
            std::vector<math::Vec2d> pts;
            for (const Vec3d& p : loopPoints(loop)) {
                if (std::abs(f.plane.signedDistance(p)) > kPlanarTolerance) {
                    r.fail("fața ", id.value, " nu e plană (un vârf e la ", f.plane.signedDistance(p), " mm)");
                }
                pts.push_back(frame.toLocal(p));
            }
            const double area = math::signedArea(pts);
            if (isOuter && area <= 0.0) {
                r.fail("fața ", id.value, ": bucla exterioară nu e în sens trigonometric față de normală");
            }
            if (!isOuter && area >= 0.0) {
                r.fail("fața ", id.value, ": o gaură nu e în sens orar față de normală");
            }
            if (isOuter) {
                outer2d = std::move(pts);
            } else if (!outer2d.empty()) {
                for (const math::Vec2d& p : pts) {
                    if (math::locatePoint(p, outer2d, tol::kLength) == math::PointLocation::Outside) {
                        r.fail("fața ", id.value, ": o gaură iese din bucla exterioară");
                        break;
                    }
                }
            }
        };

        checkLoop(f.outer, true);
        for (const Loop& loop : f.inner) {
            checkLoop(loop, false);
        }
        if (hasDuplicates(allEdges)) {
            r.fail("fața ", id.value, " folosește aceeași muchie de două ori");
        }
        std::sort(allEdges.begin(), allEdges.end());
        if (!faceEdgeSets.insert(allEdges).second) {
            r.fail("fața ", id.value, " dublează o altă față");
        }
    }

    if (level != ValidationLevel::Full) {
        return report;
    }

    // --- Vârfuri distincte în toleranță ---
    const auto& vs = m_vertices.values();
    for (std::size_t i = 0; i < vs.size(); ++i) {
        for (std::size_t j = i + 1; j < vs.size(); ++j) {
            if (tol::samePoint(vs[i].position, vs[j].position)) {
                r.fail("vârfurile ", m_vertices.keys()[i].value, " și ", m_vertices.keys()[j].value,
                    " coincid");
            }
        }
    }

    // --- Muchiile nu se intersectează decât în vârfurile comune ---
    const auto& es = m_edges.values();
    for (std::size_t i = 0; i < es.size(); ++i) {
        const Edge& a = es[i];
        if (!m_vertices.contains(a.v0) || !m_vertices.contains(a.v1)) {
            continue;
        }
        const math::Segment sa { position(a.v0), position(a.v1) };
        for (std::size_t j = i + 1; j < es.size(); ++j) {
            const Edge& b = es[j];
            if (!m_vertices.contains(b.v0) || !m_vertices.contains(b.v1)) {
                continue;
            }
            const math::Segment sb { position(b.v0), position(b.v1) };
            const bool shareVertex = a.hasVertex(b.v0) || a.hasVertex(b.v1);
            if (shareVertex) {
                // Două muchii dintr-un vârf comun se suprapun doar dacă sunt coliniare și în același sens.
                const VertexId common = a.hasVertex(b.v0) ? b.v0 : b.v1;
                const Vec3d o = position(common);
                const Vec3d da = glm::normalize(position(a.other(common)) - o);
                const Vec3d db = glm::normalize(position(b.other(common)) - o);
                if (glm::dot(da, db) > 1.0 - 1e-12) {
                    r.fail("muchiile ", m_edges.keys()[i].value, " și ", m_edges.keys()[j].value,
                        " se suprapun");
                }
                continue;
            }
            if (math::closestPoints(sa, sb).distance <= tol::kLength) {
                r.fail("muchiile ", m_edges.keys()[i].value, " și ", m_edges.keys()[j].value,
                    " se intersectează în afara vârfurilor");
            }
        }
    }
    return report;
}

} // namespace forma::geom
