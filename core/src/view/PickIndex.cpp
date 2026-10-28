#include "aion/core/geom/Mesh.h"
#include "aion/core/view/Picking.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <utility>
#include <vector>

// Indexul pentru picking și inferență pe modele mari: câte un BVH (arbore de cutii) pe fețe și pe muchii (vârfurile
// se găsesc prin muchiile lor), construit la cerere și păstrat cât timp geometria nu se schimbă (`Mesh::revision`).
namespace aion::view {

namespace {

using math::Vec3d;

struct Box {
    Vec3d lo { std::numeric_limits<double>::infinity() };
    Vec3d hi { -std::numeric_limits<double>::infinity() };

    void add(const Vec3d& p)
    {
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(lo[a], p[a]);
            hi[a] = std::max(hi[a], p[a]);
        }
    }
    void add(const Box& b)
    {
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(lo[a], b.lo[a]);
            hi[a] = std::max(hi[a], b.hi[a]);
        }
    }
};

class Bvh {
public:
    void build(std::vector<Box> boxes)
    {
        m_boxes = std::move(boxes);
        const std::size_t n = m_boxes.size();
        m_nodes.clear();
        m_order.clear();
        if (n == 0) {
            return;
        }
        // Ordinea pe curba Morton a centrelor (sortare radix, liniară): elementele apropiate în spațiu ajung
        // alături, iar arborele se face apoi doar prin înjumătățirea șirului (rapid și într-un build de debug).
        Box all;
        for (const Box& b : m_boxes) {
            all.add((b.lo + b.hi) * 0.5);
        }
        std::vector<std::uint32_t> keys(n);
        for (std::size_t i = 0; i < n; ++i) {
            std::uint32_t q[3];
            for (int a = 0; a < 3; ++a) {
                const double extent = all.hi[a] - all.lo[a];
                const double c = (m_boxes[i].lo[a] + m_boxes[i].hi[a]) * 0.5;
                const double t = extent > 0.0 ? (c - all.lo[a]) / extent : 0.0;
                q[a] = static_cast<std::uint32_t>(std::clamp(t, 0.0, 1.0) * 1023.0);
            }
            keys[i] = spread(q[0]) | (spread(q[1]) << 1) | (spread(q[2]) << 2);
        }
        m_order.resize(n);
        std::iota(m_order.begin(), m_order.end(), 0u);
        std::vector<std::uint32_t> scratch(n);
        for (int shift = 0; shift < 32; shift += 8) {
            std::array<std::size_t, 257> count {};
            for (std::uint32_t i : m_order) {
                ++count[((keys[i] >> shift) & 0xFFu) + 1];
            }
            for (std::size_t b = 1; b < count.size(); ++b) {
                count[b] += count[b - 1];
            }
            for (std::uint32_t i : m_order) {
                scratch[count[(keys[i] >> shift) & 0xFFu]++] = i;
            }
            m_order.swap(scratch);
        }
        m_nodes.reserve(n / kLeaf * 4 + 1);
        m_nodes.push_back({});
        fill(0, 0, static_cast<std::uint32_t>(n));
    }

    // `test(box)` decide dacă se coboară în nod; `visit(index)` primește elementele frunzelor atinse.
    template <class Test, class Visit>
    void query(const Test& test, const Visit& visit) const
    {
        if (m_nodes.empty()) {
            return;
        }
        std::array<std::uint32_t, 64> stack {};
        std::size_t top = 0;
        stack[top++] = 0;
        while (top > 0) {
            const Node& n = m_nodes[stack[--top]];
            if (!test(n.box)) {
                continue;
            }
            if (n.count > 0) {
                for (std::uint32_t i = n.first; i < n.first + n.count; ++i) {
                    if (test(m_boxes[m_order[i]])) {
                        visit(m_order[i]);
                    }
                }
            } else if (top + 2 <= stack.size()) {
                stack[top++] = n.left;
                stack[top++] = n.left + 1;
            }
        }
    }

private:
    static constexpr std::uint32_t kLeaf = 8;

    struct Node {
        Box box;
        std::uint32_t first = 0;
        std::uint32_t count = 0; // > 0: frunză
        std::uint32_t left = 0; // copiii: left și left + 1
    };

    // 10 biți, cu câte doi zero între ei (pentru intercalarea Morton pe trei axe).
    static std::uint32_t spread(std::uint32_t v)
    {
        v = (v | (v << 16)) & 0x030000FFu;
        v = (v | (v << 8)) & 0x0300F00Fu;
        v = (v | (v << 4)) & 0x030C30C3u;
        v = (v | (v << 2)) & 0x09249249u;
        return v;
    }

    // Copiii unui nod stau unul lângă altul (left, left + 1); nodurile se adresează prin indici (vectorul crește).
    // Cutia unui nod e reuniunea cutiilor copiilor. Adâncimea ≈ log2(n / kLeaf), sub limita stivei din query.
    void fill(std::uint32_t node, std::uint32_t first, std::uint32_t count)
    {
        if (count <= kLeaf) {
            Box box;
            for (std::uint32_t i = first; i < first + count; ++i) {
                box.add(m_boxes[m_order[i]]);
            }
            m_nodes[node].box = box;
            m_nodes[node].first = first;
            m_nodes[node].count = count;
            return;
        }
        const std::uint32_t half = count / 2;
        const auto left = static_cast<std::uint32_t>(m_nodes.size());
        m_nodes[node].left = left;
        m_nodes.push_back({});
        m_nodes.push_back({});
        fill(left, first, half);
        fill(left + 1, first + half, count - half);
        Box box = m_nodes[left].box;
        box.add(m_nodes[left + 1].box);
        m_nodes[node].box = box;
    }

    std::vector<Box> m_boxes;
    std::vector<std::uint32_t> m_order;
    std::vector<Node> m_nodes;
};

struct MeshIndex {
    std::uint64_t revision = 0;
    std::vector<geom::FaceId> faces;
    std::vector<geom::EdgeId> edges;
    std::vector<geom::VertexId> isolated; // vârfurile fără muchii (celelalte se găsesc prin muchiile lor)
    Bvh faceTree;
    Bvh edgeTree;
};

std::size_t& threshold()
{
    static std::size_t value = 4000;
    return value;
}

bool useIndex(const geom::Mesh& mesh)
{
    return mesh.faces().size() + mesh.edges().size() >= threshold();
}

std::shared_ptr<const MeshIndex> build(const geom::Mesh& mesh)
{
    auto index = std::make_shared<MeshIndex>();
    index->revision = mesh.revision();
    std::vector<Box> boxes;
    boxes.reserve(mesh.faces().size());
    for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
        Box b;
        for (const geom::EdgeUse& u : mesh.faces().values()[i].outer.uses) {
            b.add(mesh.position(mesh.useStart(u)));
        }
        index->faces.push_back(mesh.faces().keys()[i]);
        boxes.push_back(b);
    }
    index->faceTree.build(std::move(boxes));
    boxes = {};
    boxes.reserve(mesh.edges().size());
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const geom::Edge& e = mesh.edges().values()[i];
        Box b;
        b.add(mesh.position(e.v0));
        b.add(mesh.position(e.v1));
        index->edges.push_back(mesh.edges().keys()[i]);
        boxes.push_back(b);
    }
    index->edgeTree.build(std::move(boxes));
    for (std::size_t i = 0; i < mesh.vertices().size(); ++i) {
        if (mesh.vertices().values()[i].edges.empty()) {
            index->isolated.push_back(mesh.vertices().keys()[i]);
        }
    }
    return index;
}

// Ultimele indexuri construite (câte unul pe geometrie: modelul și definițiile obiectelor).
std::shared_ptr<const MeshIndex> indexFor(const geom::Mesh& mesh)
{
    static std::mutex mutex;
    static std::vector<std::shared_ptr<const MeshIndex>> cache;
    constexpr std::size_t kCapacity = 128;
    std::lock_guard lock(mutex);
    for (std::size_t i = 0; i < cache.size(); ++i) {
        if (cache[i]->revision == mesh.revision()) {
            std::rotate(cache.begin(), cache.begin() + std::ptrdiff_t(i), cache.begin() + std::ptrdiff_t(i) + 1);
            return cache.front();
        }
    }
    auto index = build(mesh);
    cache.insert(cache.begin(), index);
    if (cache.size() > kCapacity) {
        cache.pop_back();
    }
    return index;
}

// Marginea (în unități de lume) în jurul razei care acoperă `radiusPx` pixeli la adâncimea cutiei: între razele
// pixelilor vecini, distanța crește liniar cu adâncimea (perspectivă) sau e constantă (ortografic).
struct NearRay {
    math::Ray ray;
    double perPixelAtOrigin = 0.0;
    double perPixelPerDistance = 0.0;
    double radiusPx = 0.0;

    NearRay(const Camera& camera, Vec2d screen, double radius)
        : ray(camera.rayAt(screen))
        , radiusPx(radius)
    {
        const math::Ray dx = camera.rayAt(screen + Vec2d { 1.0, 0.0 });
        const math::Ray dy = camera.rayAt(screen + Vec2d { 0.0, 1.0 });
        perPixelAtOrigin = std::max(glm::length(dx.origin - ray.origin), glm::length(dy.origin - ray.origin));
        perPixelPerDistance = std::max(glm::length(dx.direction - ray.direction), glm::length(dy.direction - ray.direction));
    }

    [[nodiscard]] bool touches(const Box& b) const
    {
        const Vec3d center = (b.lo + b.hi) * 0.5;
        const double far = glm::length(center - ray.origin) + glm::length(b.hi - b.lo) * 0.5;
        const double margin = (radiusPx + 1.0) * (perPixelAtOrigin + perPixelPerDistance * far) + 1e-6;
        return rayHitsBox(ray, b.lo, b.hi, margin);
    }
};

} // namespace

void setPickIndexThreshold(std::size_t entities)
{
    threshold() = entities;
}

void forEachFaceAlong(const geom::Mesh& mesh, const math::Ray& ray, const std::function<void(geom::FaceId)>& visit)
{
    if (!useIndex(mesh)) {
        for (geom::FaceId f : mesh.faces().keys()) {
            visit(f);
        }
        return;
    }
    const auto index = indexFor(mesh);
    index->faceTree.query([&](const Box& b) { return rayHitsBox(ray, b.lo, b.hi, 1e-3); },
        [&](std::uint32_t i) { visit(index->faces[i]); });
}

void forEachVertexNear(const geom::Mesh& mesh, const Camera& camera, Vec2d screen, double radiusPx,
    const std::function<void(geom::VertexId)>& visit)
{
    if (!useIndex(mesh)) {
        for (geom::VertexId v : mesh.vertices().keys()) {
            visit(v);
        }
        return;
    }
    // Un vârf aproape de cursor e capătul unei muchii aproape de cursor (sau un vârf izolat). Un vârf poate fi
    // vizitat de mai multe ori; apelantul compară distanțele, deci nu contează.
    const auto index = indexFor(mesh);
    const NearRay near(camera, screen, radiusPx);
    index->edgeTree.query([&](const Box& b) { return near.touches(b); }, [&](std::uint32_t i) {
        const geom::Edge& e = mesh.edge(index->edges[i]);
        visit(e.v0);
        visit(e.v1);
    });
    for (geom::VertexId v : index->isolated) {
        visit(v);
    }
}

void forEachEdgeNear(const geom::Mesh& mesh, const Camera& camera, Vec2d screen, double radiusPx,
    const std::function<void(geom::EdgeId)>& visit)
{
    if (!useIndex(mesh)) {
        for (geom::EdgeId e : mesh.edges().keys()) {
            visit(e);
        }
        return;
    }
    const auto index = indexFor(mesh);
    const NearRay near(camera, screen, radiusPx);
    index->edgeTree.query([&](const Box& b) { return near.touches(b); }, [&](std::uint32_t i) { visit(index->edges[i]); });
}

} // namespace aion::view
