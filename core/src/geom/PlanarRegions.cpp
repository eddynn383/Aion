#include "aion/core/geom/PlanarRegions.h"

#include "aion/core/geom/Mesh.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

namespace aion::geom {

namespace {

struct HalfEdge {
    std::size_t from; // indice local de vârf
    std::size_t to;
    std::size_t edge; // indice local de muchie
};

struct Graph {
    std::vector<VertexId> vertexIds;
    std::vector<math::Vec2d> points;
    std::vector<EdgeId> edgeIds;
    std::vector<std::pair<std::size_t, std::size_t>> ends; // capetele fiecărei muchii (v0, v1)
    std::vector<std::vector<std::size_t>> incident; // muchiile fiecărui vârf
};

Graph buildGraph(const Mesh& mesh, const math::PlaneFrame& frame, std::span<const EdgeId> edges)
{
    Graph g;
    std::unordered_map<VertexId, std::size_t> local;
    const auto localVertex = [&](VertexId v) {
        const auto [it, inserted] = local.emplace(v, g.vertexIds.size());
        if (inserted) {
            g.vertexIds.push_back(v);
            g.points.push_back(frame.toLocal(mesh.position(v)));
            g.incident.emplace_back();
        }
        return it->second;
    };
    for (EdgeId id : edges) {
        const Edge& e = mesh.edge(id);
        const std::size_t a = localVertex(e.v0);
        const std::size_t b = localVertex(e.v1);
        const std::size_t index = g.edgeIds.size();
        g.edgeIds.push_back(id);
        g.ends.emplace_back(a, b);
        g.incident[a].push_back(index);
        g.incident[b].push_back(index);
    }
    return g;
}

// Punțile grafului (Tarjan, iterativ ca să nu depășim stiva la modele mari).
std::vector<bool> findBridges(const Graph& g)
{
    const std::size_t n = g.vertexIds.size();
    constexpr std::size_t kUnset = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> order(n, kUnset);
    std::vector<std::size_t> low(n, 0);
    std::vector<bool> bridge(g.edgeIds.size(), false);
    std::size_t counter = 0;

    struct Frame {
        std::size_t vertex;
        std::size_t parentEdge;
        std::size_t next; // următorul indice din incident[vertex]
    };
    for (std::size_t root = 0; root < n; ++root) {
        if (order[root] != kUnset) {
            continue;
        }
        std::vector<Frame> stack { { root, kUnset, 0 } };
        order[root] = low[root] = counter++;
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.next < g.incident[f.vertex].size()) {
                const std::size_t e = g.incident[f.vertex][f.next++];
                if (e == f.parentEdge) {
                    continue;
                }
                const auto [a, b] = g.ends[e];
                const std::size_t w = a == f.vertex ? b : a;
                if (order[w] == kUnset) {
                    order[w] = low[w] = counter++;
                    stack.push_back({ w, e, 0 });
                } else {
                    low[f.vertex] = std::min(low[f.vertex], order[w]);
                }
            } else {
                const Frame done = f;
                stack.pop_back();
                if (!stack.empty()) {
                    const std::size_t parent = stack.back().vertex;
                    low[parent] = std::min(low[parent], low[done.vertex]);
                    if (low[done.vertex] > order[parent]) {
                        bridge[done.parentEdge] = true;
                    }
                }
            }
        }
    }
    return bridge;
}

std::size_t findRoot(std::vector<std::size_t>& parent, std::size_t x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

struct Cycle {
    Loop loop;
    std::vector<math::Vec2d> polygon;
    double area = 0.0;
    std::size_t component = 0;
};

} // namespace

Loop reversed(const Loop& loop)
{
    Loop out;
    out.uses.reserve(loop.uses.size());
    for (auto it = loop.uses.rbegin(); it != loop.uses.rend(); ++it) {
        out.uses.push_back({ it->edge, !it->reversed });
    }
    return out;
}

std::vector<PlanarRegion> findPlanarRegions(const Mesh& mesh, const math::Plane& plane, std::span<const EdgeId> edges)
{
    const math::PlaneFrame frame(plane);
    const Graph g = buildGraph(mesh, frame, edges);
    const std::vector<bool> bridge = findBridges(g);

    // Componentele conexe după eliminarea punților.
    std::vector<std::size_t> parent(g.vertexIds.size());
    std::iota(parent.begin(), parent.end(), 0);
    for (std::size_t e = 0; e < g.edgeIds.size(); ++e) {
        if (!bridge[e]) {
            parent[findRoot(parent, g.ends[e].first)] = findRoot(parent, g.ends[e].second);
        }
    }

    // Muchiile rămase la fiecare vârf, sortate după unghi (sens trigonometric).
    const auto angleOf = [&](std::size_t from, std::size_t edge) {
        const auto [a, b] = g.ends[edge];
        const math::Vec2d d = g.points[a == from ? b : a] - g.points[from];
        return std::atan2(d.y, d.x);
    };
    std::vector<std::vector<std::size_t>> around(g.vertexIds.size());
    for (std::size_t v = 0; v < g.vertexIds.size(); ++v) {
        for (std::size_t e : g.incident[v]) {
            if (!bridge[e]) {
                around[v].push_back(e);
            }
        }
        std::sort(around[v].begin(), around[v].end(),
            [&](std::size_t x, std::size_t y) { return angleOf(v, x) < angleOf(v, y); });
    }

    // Parcurgem fiecare semi-muchie o singură dată. La fiecare vârf continuăm pe muchia aflată
    // imediat în sens orar față de cea pe care am venit: regiunile mărginite ies în sens
    // trigonometric (arie pozitivă), contururile exterioare ale componentelor în sens orar.
    std::vector<std::array<bool, 2>> visited(g.edgeIds.size(), { false, false });
    std::vector<Cycle> cycles;
    for (std::size_t startEdge = 0; startEdge < g.edgeIds.size(); ++startEdge) {
        if (bridge[startEdge]) {
            continue;
        }
        for (int dir = 0; dir < 2; ++dir) {
            if (visited[startEdge][static_cast<std::size_t>(dir)]) {
                continue;
            }
            Cycle cycle;
            std::size_t edge = startEdge;
            std::size_t from = dir == 0 ? g.ends[edge].first : g.ends[edge].second;
            bool ok = true;
            for (std::size_t guard = 0; guard <= 2 * g.edgeIds.size() + 1; ++guard) {
                const bool forward = from == g.ends[edge].first;
                auto& seen = visited[edge][forward ? 0 : 1];
                if (seen) {
                    break;
                }
                seen = true;
                const EdgeId id = g.edgeIds[edge];
                cycle.loop.uses.push_back({ id, !forward });
                cycle.polygon.push_back(g.points[from]);
                const std::size_t to = forward ? g.ends[edge].second : g.ends[edge].first;

                const auto& list = around[to];
                const auto pos = std::find(list.begin(), list.end(), edge);
                if (pos == list.end()) {
                    ok = false;
                    break;
                }
                const std::size_t i = static_cast<std::size_t>(pos - list.begin());
                edge = list[(i + list.size() - 1) % list.size()];
                from = to;
            }
            if (!ok || cycle.loop.uses.size() < 3) {
                continue;
            }
            cycle.area = math::signedArea(cycle.polygon);
            cycle.component = findRoot(parent, g.ends[startEdge].first);
            if (std::abs(cycle.area) > tol::kLength * tol::kLength) {
                cycles.push_back(std::move(cycle));
            }
        }
    }

    std::vector<std::size_t> bounded;
    std::vector<std::size_t> outlines;
    for (std::size_t i = 0; i < cycles.size(); ++i) {
        (cycles[i].area > 0.0 ? bounded : outlines).push_back(i);
    }

    std::vector<PlanarRegion> regions(bounded.size());
    for (std::size_t r = 0; r < bounded.size(); ++r) {
        regions[r].outer = cycles[bounded[r]].loop;
    }

    // Conturul unei componente devine gaură în cea mai mică regiune (a altei componente) care îl conține.
    for (std::size_t o : outlines) {
        const Cycle& outline = cycles[o];
        std::size_t best = bounded.size();
        double bestArea = std::numeric_limits<double>::max();
        for (std::size_t r = 0; r < bounded.size(); ++r) {
            const Cycle& candidate = cycles[bounded[r]];
            if (candidate.component == outline.component || candidate.area >= bestArea) {
                continue;
            }
            if (math::locatePoint(outline.polygon.front(), candidate.polygon, tol::kLength)
                == math::PointLocation::Inside) {
                best = r;
                bestArea = candidate.area;
            }
        }
        if (best < bounded.size()) {
            regions[best].holes.push_back(outline.loop);
        }
    }
    return regions;
}

} // namespace aion::geom
