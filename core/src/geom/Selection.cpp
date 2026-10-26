#include "aion/core/geom/Selection.h"

#include "aion/core/geom/Mesh.h"

#include <algorithm>
#include <vector>

namespace aion::geom {

void Selection::add(const Selection& other)
{
    edges.insert(other.edges.begin(), other.edges.end());
    faces.insert(other.faces.begin(), other.faces.end());
    instances.insert(other.instances.begin(), other.instances.end());
}

void Selection::remove(const Selection& other)
{
    for (EdgeId e : other.edges) {
        edges.erase(e);
    }
    for (FaceId f : other.faces) {
        faces.erase(f);
    }
    for (model::InstanceId i : other.instances) {
        instances.erase(i);
    }
}

void Selection::toggle(const Selection& other)
{
    for (EdgeId e : other.edges) {
        if (!edges.erase(e)) {
            edges.insert(e);
        }
    }
    for (FaceId f : other.faces) {
        if (!faces.erase(f)) {
            faces.insert(f);
        }
    }
    for (model::InstanceId i : other.instances) {
        if (!instances.erase(i)) {
            instances.insert(i);
        }
    }
}

void Selection::prune(const Mesh& mesh)
{
    std::erase_if(edges, [&](EdgeId e) { return !mesh.edges().contains(e); });
    std::erase_if(faces, [&](FaceId f) { return !mesh.faces().contains(f); });
}

Selection faceWithEdges(const Mesh& mesh, FaceId face)
{
    Selection s;
    s.faces.insert(face);
    const Face& f = mesh.face(face);
    for (const EdgeUse& u : f.outer.uses) {
        s.edges.insert(u.edge);
    }
    for (const Loop& loop : f.inner) {
        for (const EdgeUse& u : loop.uses) {
            s.edges.insert(u.edge);
        }
    }
    return s;
}

Selection edgeWithFaces(const Mesh& mesh, EdgeId edge)
{
    Selection s;
    s.edges.insert(edge);
    for (FaceId f : mesh.edge(edge).faces) {
        s.faces.insert(f);
    }
    return s;
}

Selection connectedTo(const Mesh& mesh, const Selection& seed)
{
    std::set<VertexId> visited;
    std::vector<VertexId> stack;
    const auto pushEdge = [&](EdgeId e) {
        stack.push_back(mesh.edge(e).v0);
        stack.push_back(mesh.edge(e).v1);
    };
    for (EdgeId e : seed.edges) {
        pushEdge(e);
    }
    for (FaceId f : seed.faces) {
        for (EdgeId e : faceWithEdges(mesh, f).edges) {
            pushEdge(e);
        }
    }

    Selection out;
    while (!stack.empty()) {
        const VertexId v = stack.back();
        stack.pop_back();
        if (!visited.insert(v).second) {
            continue;
        }
        for (EdgeId e : mesh.vertex(v).edges) {
            out.edges.insert(e);
            for (FaceId f : mesh.edge(e).faces) {
                out.faces.insert(f);
            }
            stack.push_back(mesh.edge(e).other(v));
        }
    }
    return out;
}

} // namespace aion::geom
