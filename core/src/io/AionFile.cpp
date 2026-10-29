#include "aion/core/io/AionFile.h"

#include "aion/core/Version.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/CameraLens.h"
#include "aion/core/model/Operations.h"

#include <miniz.h>
#include <nlohmann/json.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <system_error>

namespace aion::io {

namespace {

using nlohmann::json;

constexpr const char* kModelEntry = "model.json";
constexpr const char* kThumbnailEntry = "thumbnail.png";

template <class IdT, class T>
std::vector<std::size_t> sortedIndices(const geom::SlotMap<IdT, T>& map)
{
    std::vector<std::size_t> order(map.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return map.keys()[a] < map.keys()[b]; });
    return order;
}

json loopToJson(const geom::Loop& loop)
{
    json uses = json::array();
    for (const geom::EdgeUse& u : loop.uses) {
        const auto id = static_cast<std::int64_t>(u.edge.value);
        uses.push_back(u.reversed ? -id : id);
    }
    return uses;
}

json meshToJson(const geom::Mesh& mesh)
{
    json vertices = json::array();
    for (std::size_t i : sortedIndices(mesh.vertices())) {
        const geom::Vertex& v = mesh.vertices().values()[i];
        vertices.push_back({ mesh.vertices().keys()[i].value, v.position.x, v.position.y, v.position.z });
    }
    json edges = json::array();
    for (std::size_t i : sortedIndices(mesh.edges())) {
        const geom::Edge& e = mesh.edges().values()[i];
        json edge = { mesh.edges().keys()[i].value, e.v0.value, e.v1.value, e.soft ? 1 : 0, e.curve,
            e.smoothCurve ? 1 : 0 };
        if (e.tag) {
            edge.push_back(e.tag.value); // al 7-lea câmp, opțional
        }
        edges.push_back(std::move(edge));
    }
    json faces = json::array();
    for (std::size_t i : sortedIndices(mesh.faces())) {
        const geom::Face& f = mesh.faces().values()[i];
        json inner = json::array();
        for (const geom::Loop& h : f.inner) {
            inner.push_back(loopToJson(h));
        }
        json face {
            { "id", mesh.faces().keys()[i].value },
            { "material", f.material.value },
            { "backMaterial", f.backMaterial.value },
            { "tag", f.tag.value },
            { "plane", { f.plane.normal.x, f.plane.normal.y, f.plane.normal.z, f.plane.d } },
            { "outer", loopToJson(f.outer) },
            { "inner", inner },
        };
        if (!f.texture.isIdentity()) {
            // [deplasare X, deplasare Y, rotație, scară X, scară Y]
            face["uv"] = { f.texture.offset.x, f.texture.offset.y, f.texture.rotation, f.texture.scale.x, f.texture.scale.y };
        }
        faces.push_back(std::move(face));
    }
    return { { "vertices", vertices }, { "edges", edges }, { "faces", faces } };
}

geom::Loop loopFromJson(const json& j)
{
    geom::Loop loop;
    for (const json& u : j) {
        const std::int64_t id = u.get<std::int64_t>();
        loop.uses.push_back({ geom::EdgeId { static_cast<std::uint64_t>(id < 0 ? -id : id) }, id < 0 });
    }
    return loop;
}

// Geometria unei definiții, ca ChangeSet de creare (ID-urile păstrate); vecinătățile se refac din muchii și fețe.
geom::ChangeSet meshFromJson(const json& j)
{
    std::map<geom::VertexId, geom::Vertex> vertices;
    std::map<geom::EdgeId, geom::Edge> edges;
    std::map<geom::FaceId, geom::Face> faces;
    for (const json& v : j.at("vertices")) {
        vertices[geom::VertexId { v.at(0).get<std::uint64_t>() }]
            = geom::Vertex { { v.at(1).get<double>(), v.at(2).get<double>(), v.at(3).get<double>() }, {} };
    }
    for (const json& e : j.at("edges")) {
        geom::Edge edge;
        edge.v0 = geom::VertexId { e.at(1).get<std::uint64_t>() };
        edge.v1 = geom::VertexId { e.at(2).get<std::uint64_t>() };
        edge.soft = e.at(3).get<int>() != 0;
        edge.curve = e.at(4).get<std::uint32_t>();
        edge.smoothCurve = e.at(5).get<int>() != 0;
        if (e.size() > 6) {
            edge.tag = model::TagId { e.at(6).get<std::uint64_t>() };
        }
        edges[geom::EdgeId { e.at(0).get<std::uint64_t>() }] = edge;
    }
    for (const json& f : j.at("faces")) {
        const json& p = f.at("plane");
        geom::Face face;
        face.plane = { { p.at(0).get<double>(), p.at(1).get<double>(), p.at(2).get<double>() }, p.at(3).get<double>() };
        face.outer = loopFromJson(f.at("outer"));
        face.material = model::MaterialId { f.value("material", std::uint64_t { 0 }) };
        face.backMaterial = model::MaterialId { f.value("backMaterial", std::uint64_t { 0 }) };
        face.tag = model::TagId { f.value("tag", std::uint64_t { 0 }) };
        if (const auto uv = f.find("uv"); uv != f.end() && uv->is_array() && uv->size() == 5) {
            face.texture.offset = { uv->at(0).get<double>(), uv->at(1).get<double>() };
            face.texture.rotation = uv->at(2).get<double>();
            face.texture.scale = { uv->at(3).get<double>(), uv->at(4).get<double>() };
        }
        for (const json& h : f.at("inner")) {
            face.inner.push_back(loopFromJson(h));
        }
        faces[geom::FaceId { f.at("id").get<std::uint64_t>() }] = face;
    }
    for (auto& [id, e] : edges) {
        vertices.at(e.v0).edges.push_back(id);
        vertices.at(e.v1).edges.push_back(id);
    }
    for (auto& [id, f] : faces) {
        const auto attach = [&](const geom::Loop& loop) {
            for (const geom::EdgeUse& u : loop.uses) {
                edges.at(u.edge).faces.push_back(id);
            }
        };
        attach(f.outer);
        for (const geom::Loop& h : f.inner) {
            attach(h);
        }
    }
    geom::ChangeSet changes;
    for (auto& [id, v] : vertices) {
        changes.vertices[id].after = std::move(v);
    }
    for (auto& [id, e] : edges) {
        changes.edges[id].after = std::move(e);
    }
    for (auto& [id, f] : faces) {
        changes.faces[id].after = std::move(f);
    }
    return changes;
}

// Numele fișierului unei texturi în arhivă, cu extensia după semnătura imaginii.
std::string textureEntry(std::uint64_t id, const model::Texture& t)
{
    const bool jpeg = t.data.size() > 2 && t.data[0] == 0xFF && t.data[1] == 0xD8;
    return "textures/" + std::to_string(id) + (jpeg ? ".jpg" : ".png");
}

json colorToJson(const model::Color& c)
{
    return { c.r, c.g, c.b, c.a };
}

// Camerele și luminile.
json camerasToJson(const model::Model& model)
{
    const auto vec = [](const math::Vec3d& v) { return json { v.x, v.y, v.z }; };
    json out = json::array();
    for (std::size_t i = 0; i < model.cameras().size(); ++i) {
        const model::CameraObject& c = model.cameras().values()[i];
        out.push_back({ { "id", model.cameras().keys()[i].value }, { "name", c.name }, { "position", vec(c.position) },
            { "target", vec(c.target) }, { "focalLength", c.focalLength }, { "sensorWidth", c.sensorWidth },
            { "format", json { c.formatWidth, c.formatHeight } }, { "shift", json { c.shift.x, c.shift.y } },
            { "roll", c.roll }, { "orthographic", c.orthographic }, { "orthoScale", c.orthoScale },
            { "near", c.nearClip }, { "far", c.farClip },
            { "dof", { { "enabled", c.dof.enabled }, { "focus", c.dof.focusDistance }, { "fStop", c.dof.fStop } } },
            { "displaySize", c.displaySize }, { "visible", c.visible } });
    }
    return out;
}

// Curba Bézier: { closed, points: [[px, py, pz, ix, iy, iz, ox, oy, oz, smooth]] }.
json bezierToJson(const geom::BezierPath& path)
{
    json points = json::array();
    for (const geom::BezierPoint& p : path.points) {
        points.push_back(json { p.point.x, p.point.y, p.point.z, p.in.x, p.in.y, p.in.z, p.out.x, p.out.y, p.out.z,
            p.smooth ? 1 : 0 });
    }
    return { { "closed", path.closed }, { "points", points } };
}

geom::BezierPath bezierFromJson(const json& j)
{
    geom::BezierPath path;
    path.closed = j.value("closed", false);
    for (const json& p : j.value("points", json::array())) {
        const auto at = [&](std::size_t i) { return p.at(i).get<double>(); };
        path.points.push_back({ { at(0), at(1), at(2) }, { at(3), at(4), at(5) }, { at(6), at(7), at(8) }, at(9) != 0.0 });
    }
    return path;
}

json penCurvesToJson(const model::Model& model)
{
    json out = json::array();
    for (std::size_t i = 0; i < model.penCurves().size(); ++i) {
        const model::PenCurve& c = model.penCurves().values()[i];
        out.push_back({ { "id", model.penCurves().keys()[i].value }, { "definition", c.definition.value },
            { "curve", c.curve }, { "normal", json { c.normal.x, c.normal.y, c.normal.z } },
            { "path", bezierToJson(c.path) } });
    }
    return out;
}

void penCurvesFromJson(const json& list, model::ChangeSet& changes)
{
    for (const json& j : list) {
        model::PenCurve c;
        c.definition = model::DefinitionId { j.at("definition").get<std::uint64_t>() };
        c.curve = j.at("curve").get<std::uint32_t>();
        const json& n = j.at("normal");
        c.normal = { n.at(0).get<double>(), n.at(1).get<double>(), n.at(2).get<double>() };
        c.path = bezierFromJson(j.at("path"));
        changes.penCurves[model::PenCurveId { j.at("id").get<std::uint64_t>() }].after = std::move(c);
    }
}

json lightsToJson(const model::Model& model)
{
    const auto vec = [](const math::Vec3d& v) { return json { v.x, v.y, v.z }; };
    json out = json::array();
    for (std::size_t i = 0; i < model.lights().size(); ++i) {
        const model::Light& l = model.lights().values()[i];
        out.push_back({ { "id", model.lights().keys()[i].value }, { "name", l.name },
            { "kind", static_cast<int>(l.kind) }, { "position", vec(l.position) }, { "direction", vec(l.direction) },
            { "color", colorToJson(l.color) }, { "intensity", l.intensity }, { "range", l.range }, { "cone", l.cone },
            { "softness", l.softness }, { "width", l.width }, { "height", l.height },
            { "castShadows", l.castShadows }, { "enabled", l.enabled }, { "visible", l.visible } });
    }
    return out;
}

void camerasAndLightsFromJson(const json& doc, model::ChangeSet& changes)
{
    const auto vec = [](const json& v, math::Vec3d fallback) {
        return v.is_array() && v.size() == 3 ? math::Vec3d { v.at(0).get<double>(), v.at(1).get<double>(), v.at(2).get<double>() }
                                             : fallback;
    };
    for (const json& j : doc.value("cameras", json::array())) {
        model::CameraObject c;
        c.name = j.value("name", std::string());
        c.position = vec(j.value("position", json()), c.position);
        c.target = vec(j.value("target", json()), c.target);
        c.sensorWidth = j.value("sensorWidth", c.sensorWidth);
        if (const auto f = j.find("format"); f != j.end() && f->is_array() && f->size() == 2) {
            c.formatWidth = std::max(f->at(0).get<int>(), 1);
            c.formatHeight = std::max(f->at(1).get<int>(), 1);
        }
        if (const auto s = j.find("shift"); s != j.end() && s->is_array() && s->size() == 2) {
            c.shift = { s->at(0).get<double>(), s->at(1).get<double>() };
        }
        c.roll = j.value("roll", 0.0);
        if (j.contains("focalLength")) {
            c.focalLength = j.value("focalLength", c.focalLength);
        } else if (j.contains("fov")) {
            // Fișierele vechi țineau unghiul vertical: focala care îl păstrează în formatul camerei.
            c.focalLength = model::focalForVerticalFov(c, glm::radians(j.value("fov", 50.0)));
        }
        c.orthographic = j.value("orthographic", false);
        c.orthoScale = j.value("orthoScale", c.orthoScale);
        c.nearClip = j.value("near", 0.0);
        c.farClip = j.value("far", 0.0);
        if (const auto d = j.find("dof"); d != j.end() && d->is_object()) {
            c.dof.enabled = d->value("enabled", false);
            c.dof.focusDistance = d->value("focus", c.dof.focusDistance);
            c.dof.fStop = d->value("fStop", c.dof.fStop);
        }
        c.displaySize = j.value("displaySize", 1.0);
        c.visible = j.value("visible", true);
        changes.cameras[model::CameraId { j.at("id").get<std::uint64_t>() }].after = std::move(c);
    }
    for (const json& j : doc.value("lights", json::array())) {
        model::Light l;
        l.name = j.value("name", std::string());
        l.kind = static_cast<model::Light::Kind>(std::clamp(j.value("kind", 0), 0, 3));
        l.position = vec(j.value("position", json()), l.position);
        l.direction = vec(j.value("direction", json()), l.direction);
        if (const auto c = j.find("color"); c != j.end() && c->is_array() && c->size() == 4) {
            l.color = { c->at(0).get<std::uint8_t>(), c->at(1).get<std::uint8_t>(), c->at(2).get<std::uint8_t>(),
                c->at(3).get<std::uint8_t>() };
        }
        l.intensity = j.value("intensity", l.intensity);
        l.range = j.value("range", l.range);
        l.cone = j.value("cone", l.cone);
        l.softness = j.value("softness", l.softness);
        l.width = j.value("width", l.width);
        l.height = j.value("height", l.height);
        l.castShadows = j.value("castShadows", false);
        l.enabled = j.value("enabled", true);
        l.visible = j.value("visible", true);
        changes.lights[model::LightId { j.at("id").get<std::uint64_t>() }].after = std::move(l);
    }
}

// Easing-ul: numele presetării (dacă e chiar presetarea), altfel curba întreagă.
json easingToJson(const model::Easing& e)
{
    if (const auto preset = model::easingPreset(e.name); preset && *preset == e) {
        return e.name;
    }
    return { { "name", e.name }, { "x1", e.x1 }, { "y1", e.y1 }, { "x2", e.x2 }, { "y2", e.y2 }, { "step", e.step } };
}

model::Easing easingFromJson(const json& j)
{
    if (j.is_number()) { // formatVersion 1: Linear, EaseInOut, EaseIn, EaseOut, Step
        static constexpr const char* kLegacy[] = { "linear", "easeInOut", "easeIn", "easeOut", "step" };
        return *model::easingPreset(kLegacy[std::clamp(j.get<int>(), 0, 4)]);
    }
    if (j.is_string()) {
        return model::easingPreset(j.get<std::string>()).value_or(model::Easing {});
    }
    model::Easing e;
    e.name = j.value("name", std::string());
    e.x1 = j.value("x1", e.x1);
    e.y1 = j.value("y1", e.y1);
    e.x2 = j.value("x2", e.x2);
    e.y2 = j.value("y2", e.y2);
    e.step = j.value("step", false);
    return e;
}

// Animațiile: canalele, cu cheile ca liste compacte [timp, valoare, easing] (culoarea: [timp, r, g, b, a, easing]).
json animationsToJson(const model::Model& model)
{
    json out = json::array();
    for (std::size_t i = 0; i < model.animations().size(); ++i) {
        const model::Animation& a = model.animations().values()[i];
        json tracks = json::array();
        for (const model::ObjectTrack& t : a.tracks) {
            json channels = json::array();
            for (const model::Channel& c : t.channels) {
                json keys = json::array();
                for (const model::Key& k : c.keys) {
                    if (c.property == model::Property::Color) {
                        keys.push_back({ k.time, k.color.r, k.color.g, k.color.b, k.color.a, easingToJson(k.easing) });
                    } else {
                        keys.push_back({ k.time, k.value, easingToJson(k.easing) });
                    }
                }
                channels.push_back({ { "property", model::propertyName(c.property) }, { "keys", keys } });
            }
            tracks.push_back({ { "instance", t.instance.value }, { "channels", channels } });
        }
        json camera = json::array();
        for (const model::CameraKey& k : a.camera) {
            camera.push_back({ k.time, easingToJson(k.easing), k.target.x, k.target.y, k.target.z, k.yaw, k.pitch,
                k.distance });
        }
        out.push_back({ { "id", model.animations().keys()[i].value }, { "name", a.name }, { "duration", a.duration },
            { "fps", a.fps }, { "loop", a.loop }, { "defaultEasing", easingToJson(a.defaultEasing) }, { "tracks", tracks },
            { "camera", camera } });
    }
    return out;
}

// Cheile vechi de transformare (formatVersion 1, matricea întreagă), migrate după ce modelul e construit.
struct LegacyTrack {
    model::AnimationId animation;
    model::InstanceId instance;
    std::vector<model::MatrixKey> keys;
};

void animationsFromJson(const json& list, model::ChangeSet& changes, std::vector<LegacyTrack>& legacy)
{
    for (const json& j : list) {
        const model::AnimationId id { j.at("id").get<std::uint64_t>() };
        model::Animation a;
        a.name = j.value("name", std::string());
        a.duration = j.value("duration", 5.0);
        a.fps = j.value("fps", 30);
        a.loop = j.value("loop", false);
        if (const auto e = j.find("defaultEasing"); e != j.end()) {
            a.defaultEasing = easingFromJson(*e);
        }
        for (const json& t : j.at("tracks")) {
            model::ObjectTrack track;
            track.instance = model::InstanceId { t.at("instance").get<std::uint64_t>() };
            for (const json& c : t.value("channels", json::array())) {
                const auto property = model::propertyFromName(c.at("property").get<std::string>());
                if (!property) {
                    continue; // o proprietate dintr-o versiune mai nouă
                }
                model::Channel channel { *property, {} };
                for (const json& k : c.at("keys")) {
                    model::Key key;
                    key.time = k.at(0).get<double>();
                    if (*property == model::Property::Color) {
                        key.color = { k.at(1).get<std::uint8_t>(), k.at(2).get<std::uint8_t>(),
                            k.at(3).get<std::uint8_t>(), k.at(4).get<std::uint8_t>() };
                        key.easing = easingFromJson(k.at(5));
                    } else {
                        key.value = k.at(1).get<double>();
                        key.easing = easingFromJson(k.at(2));
                    }
                    channel.keys.push_back(std::move(key));
                }
                track.channels.push_back(std::move(channel));
            }
            // formatVersion 1: [timp, easing, matricea 4 × 4] și [timp, easing, r, g, b, a].
            if (const auto old = t.find("transform"); old != t.end() && !old->empty()) {
                LegacyTrack lt { id, track.instance, {} };
                for (const json& k : *old) {
                    math::Mat4d m(1.0);
                    for (int c = 0; c < 4; ++c) {
                        for (int r = 0; r < 4; ++r) {
                            m[c][r] = k.at(static_cast<std::size_t>(2 + c * 4 + r)).get<double>();
                        }
                    }
                    lt.keys.push_back({ k.at(0).get<double>(), math::Transform(m), easingFromJson(k.at(1)) });
                }
                legacy.push_back(std::move(lt));
            }
            if (const auto old = t.find("color"); old != t.end() && !old->empty()) {
                model::Channel channel { model::Property::Color, {} };
                for (const json& k : *old) {
                    model::Key key;
                    key.time = k.at(0).get<double>();
                    key.easing = easingFromJson(k.at(1));
                    key.color = { k.at(2).get<std::uint8_t>(), k.at(3).get<std::uint8_t>(), k.at(4).get<std::uint8_t>(),
                        k.at(5).get<std::uint8_t>() };
                    channel.keys.push_back(std::move(key));
                }
                track.channels.push_back(std::move(channel));
            }
            if (!track.channels.empty()) {
                a.tracks.push_back(std::move(track));
            }
        }
        for (const json& k : j.at("camera")) {
            a.camera.push_back({ k.at(0).get<double>(), { k.at(2).get<double>(), k.at(3).get<double>(), k.at(4).get<double>() },
                k.at(5).get<double>(), k.at(6).get<double>(), k.at(7).get<double>(), easingFromJson(k.at(1)) });
        }
        changes.animations[id].after = std::move(a);
    }
}

json matrixToJson(const math::Transform& t)
{
    json m = json::array();
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            m.push_back(t.matrix()[c][r]);
        }
    }
    return m;
}

math::Transform matrixFromJson(const json& m)
{
    math::Mat4d matrix(1.0);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            matrix[c][r] = m.at(static_cast<std::size_t>(c * 4 + r)).get<double>();
        }
    }
    return math::Transform(matrix);
}

json motionPathsToJson(const model::Model& model)
{
    json out = json::array();
    for (std::size_t i = 0; i < model.motionPaths().size(); ++i) {
        const model::MotionPath& p = model.motionPaths().values()[i];
        json anchors = json::array();
        for (const model::PathAnchor& a : p.anchors) {
            anchors.push_back({ { "instance", a.instance.value }, { "along", a.along }, { "lateral", a.lateral },
                { "up", a.up }, { "yaw", a.yaw }, { "front", a.front }, { "before", matrixToJson(a.before) } });
        }
        out.push_back({ { "id", model.motionPaths().keys()[i].value }, { "name", p.name },
            { "transform", matrixToJson(p.transform) }, { "normal", json { p.normal.x, p.normal.y, p.normal.z } },
            { "path", bezierToJson(p.curve) }, { "animation", p.animation.value }, { "start", p.start },
            { "duration", p.duration }, { "easing", easingToJson(p.easing) }, { "visible", p.visible },
            { "anchors", anchors } });
    }
    return out;
}

void motionPathsFromJson(const json& list, model::ChangeSet& changes)
{
    for (const json& j : list) {
        model::MotionPath p;
        p.name = j.value("name", std::string());
        p.transform = matrixFromJson(j.at("transform"));
        const json& n = j.at("normal");
        p.normal = { n.at(0).get<double>(), n.at(1).get<double>(), n.at(2).get<double>() };
        p.curve = bezierFromJson(j.at("path"));
        p.animation = model::AnimationId { j.at("animation").get<std::uint64_t>() };
        p.start = j.value("start", 0.0);
        p.duration = j.value("duration", 5.0);
        p.easing = easingFromJson(j.at("easing"));
        p.visible = j.value("visible", true);
        for (const json& a : j.value("anchors", json::array())) {
            model::PathAnchor anchor;
            anchor.instance = model::InstanceId { a.at("instance").get<std::uint64_t>() };
            anchor.along = a.value("along", 0.0);
            anchor.lateral = a.value("lateral", 0.0);
            anchor.up = a.value("up", 0.0);
            anchor.yaw = a.value("yaw", 0.0);
            anchor.front = a.value("front", 0);
            anchor.before = matrixFromJson(a.at("before"));
            p.anchors.push_back(anchor);
        }
        changes.motionPaths[model::MotionPathId { j.at("id").get<std::uint64_t>() }].after = std::move(p);
    }
}

// formatVersion < 3: „Mută la” ducea colțul minim al țintei în punctul dat; acum duce pivotul.
void migrateMoveToRules(model::Model& model)
{
    std::vector<std::pair<model::InstanceId, model::Instance>> changed;
    for (std::size_t i = 0; i < model.instances().size(); ++i) {
        const model::InstanceId self = model.instances().keys()[i];
        model::Instance inst = model.instances().values()[i];
        bool any = false;
        const std::function<void(std::vector<model::Action>&)> fix = [&](std::vector<model::Action>& actions) {
            for (model::Action& a : actions) {
                const model::InstanceId target = a.target ? a.target : self;
                if (a.kind == model::ActionKind::MoveTo && model.instances().contains(target)) {
                    const model::Instance& t = model.instance(target);
                    const auto [lo, hi] = model::bounds(model, t.definition);
                    if (lo.x <= hi.x) {
                        a.vector += model::pivotPoint(model, target) - t.transform.applyToPoint(lo);
                        any = true;
                    }
                }
                fix(a.first);
                fix(a.second);
            }
        };
        for (model::Rule& r : inst.rules) {
            fix(r.actions);
        }
        if (any) {
            changed.emplace_back(self, std::move(inst));
        }
    }
    for (auto& [id, inst] : changed) {
        model.setInstance(id, std::move(inst));
    }
}

// Cheile vechi devin canale, față de pivotul și poza din model ale fiecărui obiect.
void migrateLegacyTracks(model::Model& model, const std::vector<LegacyTrack>& legacy)
{
    for (const LegacyTrack& lt : legacy) {
        if (!model.animations().contains(lt.animation)) {
            continue;
        }
        model::Animation a = model.animations().at(lt.animation);
        const bool exists = model.instances().contains(lt.instance);
        const math::Transform base = exists ? model.instance(lt.instance).transform : math::Transform {};
        const math::Vec3d pivot = exists ? model::pivotLocal(model, lt.instance) : math::Vec3d { 0.0 };
        model::setMatrixKeys(a, lt.instance, lt.keys, base, pivot);
        model.setAnimation(lt.animation, a);
    }
}


json actionToJson(const model::Action& a)
{
    json first = json::array();
    for (const model::Action& c : a.first) {
        first.push_back(actionToJson(c));
    }
    json second = json::array();
    for (const model::Action& c : a.second) {
        second.push_back(actionToJson(c));
    }
    return { { "kind", static_cast<int>(a.kind) }, { "target", a.target.value },
        { "vector", { a.vector.x, a.vector.y, a.vector.z } }, { "duration", a.duration },
        { "material", a.material.value }, { "animation", a.animation.value }, { "scene", a.scene.value },
        { "first", first }, { "second", second } };
}

model::Action actionFromJson(const json& j)
{
    model::Action a;
    a.kind = static_cast<model::ActionKind>(std::clamp(j.value("kind", 0), 0, 11));
    a.target = model::InstanceId { j.value("target", std::uint64_t { 0 }) };
    const json& v = j.at("vector");
    a.vector = { v.at(0).get<double>(), v.at(1).get<double>(), v.at(2).get<double>() };
    a.duration = j.value("duration", 0.5);
    a.material = model::MaterialId { j.value("material", std::uint64_t { 0 }) };
    a.animation = model::AnimationId { j.value("animation", std::uint64_t { 0 }) };
    a.scene = model::SceneId { j.value("scene", std::uint64_t { 0 }) };
    for (const json& c : j.value("first", json::array())) {
        a.first.push_back(actionFromJson(c));
    }
    for (const json& c : j.value("second", json::array())) {
        a.second.push_back(actionFromJson(c));
    }
    return a;
}

} // namespace

std::string toJson(const model::Model& model)
{
    json definitions = json::array();
    for (std::size_t i : sortedIndices(model.definitions())) {
        const model::DefinitionId id = model.definitions().keys()[i];
        const model::Definition& d = model.definitions().values()[i];
        definitions.push_back({
            { "id", id.value },
            { "name", d.name },
            { "group", d.group },
            { "collection", d.collection },
            { "shape", d.shape },
            { "generator", d.generator },
            { "parameters", d.parameters },
            { "mesh", meshToJson(model.mesh(id)) },
        });
    }
    json instances = json::array();
    for (std::size_t i : sortedIndices(model.instances())) {
        const model::Instance& inst = model.instances().values()[i];
        json matrix = json::array();
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                matrix.push_back(inst.transform.matrix()[c][r]);
            }
        }
        instances.push_back({
            { "id", model.instances().keys()[i].value },
            { "parent", inst.parent.value },
            { "definition", inst.definition.value },
            { "transform", matrix },
            { "name", inst.name },
            { "tag", inst.tag.value },
            { "hidden", inst.hidden },
            { "locked", inst.locked },
            { "material", inst.material.value },
            { "locks", inst.placementLocks },
        });
        if (inst.pivot) {
            instances.back()["pivot"] = { inst.pivot->x, inst.pivot->y, inst.pivot->z };
        }
        if (!inst.rules.empty()) {
            json rules = json::array();
            for (const model::Rule& r : inst.rules) {
                json actions = json::array();
                for (const model::Action& a : r.actions) {
                    actions.push_back(actionToJson(a));
                }
                rules.push_back({ { "trigger", static_cast<int>(r.trigger) }, { "key", r.key }, { "actions", actions } });
            }
            instances.back()["rules"] = rules;
        }
        if (inst.behavior.kind != model::Behavior::Kind::None) {
            instances.back()["behavior"] = {
                { "kind", static_cast<int>(inst.behavior.kind) },
                { "anchor", static_cast<int>(inst.behavior.anchor) },
                { "amount", inst.behavior.amount },
                { "duration", inst.behavior.duration },
                { "open", inst.behavior.open },
            };
        }
    }
    json tags = json::array();
    for (std::size_t i : sortedIndices(model.tags())) {
        const model::Tag& t = model.tags().values()[i];
        tags.push_back({
            { "id", model.tags().keys()[i].value },
            { "name", t.name },
            { "color", colorToJson(t.color) },
            { "visible", t.visible },
        });
    }
    json materials = json::array();
    for (std::size_t i : sortedIndices(model.materials())) {
        const model::Material& m = model.materials().values()[i];
        materials.push_back({
            { "id", model.materials().keys()[i].value },
            { "name", m.name },
            { "color", colorToJson(m.color) },
            { "roughness", m.roughness },
            { "metallic", m.metallic },
            { "texture", m.texture.value },
            { "textureSize", { m.textureSize.x, m.textureSize.y } },
        });
    }
    // Texturile: doar numele și fișierul din arhivă (imaginile le scrie writeArchive).
    json textures = json::array();
    for (std::size_t i : sortedIndices(model.textures())) {
        const model::Texture& t = model.textures().values()[i];
        const std::uint64_t id = model.textures().keys()[i].value;
        textures.push_back({ { "id", id }, { "name", t.name }, { "source", t.source }, { "file", textureEntry(id, t) } });
    }
    const auto vec = [](const math::Vec3d& v) { return json { v.x, v.y, v.z }; };
    json guides = json::array();
    for (std::size_t i : sortedIndices(model.guides())) {
        const model::Guide& g = model.guides().values()[i];
        guides.push_back({ { "id", model.guides().keys()[i].value }, { "context", g.context.value },
            { "point", vec(g.point) }, { "direction", vec(g.direction) } });
    }
    json annotations = json::array();
    for (std::size_t i : sortedIndices(model.annotations())) {
        const model::Annotation& a = model.annotations().values()[i];
        annotations.push_back({ { "id", model.annotations().keys()[i].value },
            { "kind", a.kind == model::Annotation::Kind::Text ? "text" : "dimension" }, { "context", a.context.value },
            { "a", vec(a.a) }, { "b", vec(a.b) }, { "offset", vec(a.offset) }, { "text", a.text } });
    }
    json sections = json::array();
    for (std::size_t i : sortedIndices(model.sections())) {
        const model::SectionPlane& s = model.sections().values()[i];
        sections.push_back({ { "id", model.sections().keys()[i].value }, { "name", s.name }, { "point", vec(s.point) },
            { "normal", vec(s.normal) }, { "active", s.active } });
    }
    json scenes = json::array();
    for (std::size_t i : sortedIndices(model.scenes())) {
        const model::Scene& s = model.scenes().values()[i];
        scenes.push_back({ { "id", model.scenes().keys()[i].value }, { "name", s.name }, { "target", vec(s.target) },
            { "yaw", s.yaw }, { "pitch", s.pitch }, { "distance", s.distance }, { "orthographic", s.orthographic },
            { "style", s.style } });
        if (s.hasState) {
            json transforms = json::array();
            for (const auto& [id, t] : s.transforms) {
                json entry = json::array({ id.value });
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) {
                        entry.push_back(t.matrix()[c][r]);
                    }
                }
                transforms.push_back(std::move(entry));
            }
            json hidden = json::array();
            for (model::InstanceId id : s.hiddenInstances) {
                hidden.push_back(id.value);
            }
            json hiddenTags = json::array();
            for (model::TagId id : s.hiddenTags) {
                hiddenTags.push_back(id.value);
            }
            scenes.back()["state"] = { { "transforms", transforms }, { "hidden", hidden }, { "hiddenTags", hiddenTags } };
        }
    }
    const json doc {
        { "format", "aion" },
        { "formatVersion", kFormatVersion },
        { "generator", std::string("Aion ") + std::string(versionString()) },
        { "units", "mm" },
        { "root", model.root().value },
        { "definitions", definitions },
        { "instances", instances },
        { "tags", tags },
        { "materials", materials },
        { "guides", guides },
        { "annotations", annotations },
        { "sections", sections },
        { "scenes", scenes },
        { "animations", animationsToJson(model) },
        { "cameras", camerasToJson(model) },
        { "lights", lightsToJson(model) },
        { "textures", textures },
        { "penCurves", penCurvesToJson(model) },
        { "motionPaths", motionPathsToJson(model) },
    };
    return doc.dump(1);
}

LoadResult fromJson(std::string_view text)
{
    LoadResult result;
    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        result.error = "Fișierul nu conține un model Aion valid.";
        return result;
    }
    if (doc.value("format", std::string()) != "aion") {
        result.error = "Fișierul nu e un model Aion.";
        return result;
    }
    if (doc.value("formatVersion", 0) > kFormatVersion) {
        result.error = "Fișierul a fost salvat de o versiune mai nouă a Aion.";
        return result;
    }
    try {
        model::Model model;
        if (model::DefinitionId { doc.at("root").get<std::uint64_t>() } != model.root()) {
            result.error = "Fișierul are o structură necunoscută (rădăcina modelului).";
            return result;
        }
        model::ChangeSet changes;
        for (const json& d : doc.at("definitions")) {
            const model::DefinitionId id { d.at("id").get<std::uint64_t>() };
            model::Definition def { d.at("name").get<std::string>(), d.at("group").get<bool>(),
                d.value("collection", false), d.value("shape", std::string()), d.value("generator", std::string()),
                d.value("parameters", std::map<std::string, double> {}) };
            auto& change = changes.definitions[id];
            if (id == model.root()) {
                change.before = model.definition(id);
            }
            change.after = std::move(def);
            changes.meshes[id] = meshFromJson(d.at("mesh"));
        }
        for (const json& i : doc.at("instances")) {
            model::Instance inst;
            inst.parent = model::DefinitionId { i.at("parent").get<std::uint64_t>() };
            inst.definition = model::DefinitionId { i.at("definition").get<std::uint64_t>() };
            const json& m = i.at("transform");
            math::Mat4d matrix(1.0);
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    matrix[c][r] = m.at(static_cast<std::size_t>(c * 4 + r)).get<double>();
                }
            }
            inst.transform = math::Transform(matrix);
            inst.name = i.value("name", std::string());
            inst.tag = model::TagId { i.value("tag", std::uint64_t { 0 }) };
            inst.hidden = i.value("hidden", false);
            inst.locked = i.value("locked", false);
            inst.placementLocks = i.value("locks", std::uint16_t { 0 });
            if (const auto pv = i.find("pivot"); pv != i.end() && pv->is_array() && pv->size() == 3) {
                inst.pivot = math::Vec3d { (*pv)[0].get<double>(), (*pv)[1].get<double>(), (*pv)[2].get<double>() };
            }
            inst.material = model::MaterialId { i.value("material", std::uint64_t { 0 }) };
            for (const json& r : i.value("rules", json::array())) {
                model::Rule rule;
                rule.trigger = static_cast<model::Trigger>(std::clamp(r.value("trigger", 0), 0, 7));
                rule.key = r.value("key", std::string());
                for (const json& a : r.at("actions")) {
                    rule.actions.push_back(actionFromJson(a));
                }
                inst.rules.push_back(std::move(rule));
            }
            if (const auto b = i.find("behavior"); b != i.end()) {
                inst.behavior.kind = static_cast<model::Behavior::Kind>(std::clamp(b->value("kind", 0), 0, 2));
                inst.behavior.anchor = static_cast<model::Behavior::Anchor>(std::clamp(b->value("anchor", 0), 0, 5));
                inst.behavior.amount = b->value("amount", 90.0);
                inst.behavior.duration = b->value("duration", 0.6);
                inst.behavior.open = b->value("open", false);
            }
            changes.instances[model::InstanceId { i.at("id").get<std::uint64_t>() }].after = std::move(inst);
        }
        for (const json& t : doc.at("tags")) {
            const json& c = t.at("color");
            model::Tag tag { t.at("name").get<std::string>(),
                { c.at(0).get<std::uint8_t>(), c.at(1).get<std::uint8_t>(), c.at(2).get<std::uint8_t>(),
                    c.at(3).get<std::uint8_t>() },
                t.value("visible", true) };
            changes.tags[model::TagId { t.at("id").get<std::uint64_t>() }].after = std::move(tag);
        }
        for (const json& tj : doc.value("textures", json::array())) {
            model::Texture texture;
            texture.name = tj.value("name", std::string());
            texture.source = tj.value("source", std::string());
            changes.textures[model::TextureId { tj.at("id").get<std::uint64_t>() }].after = std::move(texture);
        }
        for (const json& mj : doc.value("materials", json::array())) {
            const json& c = mj.at("color");
            model::Material material { mj.at("name").get<std::string>(),
                { c.at(0).get<std::uint8_t>(), c.at(1).get<std::uint8_t>(), c.at(2).get<std::uint8_t>(),
                    c.at(3).get<std::uint8_t>() },
                mj.value("roughness", 0.8f), mj.value("metallic", 0.0f) };
            material.texture = model::TextureId { mj.value("texture", std::uint64_t { 0 }) };
            if (const auto ts = mj.find("textureSize"); ts != mj.end() && ts->is_array() && ts->size() == 2) {
                material.textureSize = { ts->at(0).get<double>(), ts->at(1).get<double>() };
            }
            changes.materials[model::MaterialId { mj.at("id").get<std::uint64_t>() }].after = std::move(material);
        }
        const auto vec = [](const json& v) {
            return math::Vec3d { v.at(0).get<double>(), v.at(1).get<double>(), v.at(2).get<double>() };
        };
        for (const json& g : doc.value("guides", json::array())) {
            changes.guides[model::GuideId { g.at("id").get<std::uint64_t>() }].after = model::Guide {
                model::DefinitionId { g.at("context").get<std::uint64_t>() }, vec(g.at("point")), vec(g.at("direction")) };
        }
        for (const json& a : doc.value("annotations", json::array())) {
            model::Annotation annotation;
            annotation.kind = a.value("kind", std::string()) == "text" ? model::Annotation::Kind::Text
                                                                      : model::Annotation::Kind::Dimension;
            annotation.context = model::DefinitionId { a.at("context").get<std::uint64_t>() };
            annotation.a = vec(a.at("a"));
            annotation.b = vec(a.at("b"));
            annotation.offset = vec(a.at("offset"));
            annotation.text = a.value("text", std::string());
            changes.annotations[model::AnnotationId { a.at("id").get<std::uint64_t>() }].after = std::move(annotation);
        }
        for (const json& s : doc.value("sections", json::array())) {
            changes.sections[model::SectionId { s.at("id").get<std::uint64_t>() }].after = model::SectionPlane {
                s.value("name", std::string()), vec(s.at("point")), vec(s.at("normal")), s.value("active", true) };
        }
        std::vector<LegacyTrack> legacy;
        animationsFromJson(doc.value("animations", json::array()), changes, legacy);
        camerasAndLightsFromJson(doc, changes);
        penCurvesFromJson(doc.value("penCurves", json::array()), changes);
        motionPathsFromJson(doc.value("motionPaths", json::array()), changes);
        for (const json& s : doc.value("scenes", json::array())) {
            model::Scene scene;
            scene.name = s.value("name", std::string());
            scene.target = vec(s.at("target"));
            scene.yaw = s.value("yaw", 0.0);
            scene.pitch = s.value("pitch", 0.0);
            scene.distance = s.value("distance", 10000.0);
            scene.orthographic = s.value("orthographic", false);
            scene.style = s.value("style", std::string());
            if (const auto st = s.find("state"); st != s.end()) {
                scene.hasState = true;
                for (const json& entry : st->at("transforms")) {
                    math::Mat4d m(1.0);
                    for (int c = 0; c < 4; ++c) {
                        for (int r = 0; r < 4; ++r) {
                            m[c][r] = entry.at(static_cast<std::size_t>(1 + c * 4 + r)).get<double>();
                        }
                    }
                    scene.transforms[model::InstanceId { entry.at(0).get<std::uint64_t>() }] = math::Transform(m);
                }
                for (const json& id : st->at("hidden")) {
                    scene.hiddenInstances.insert(model::InstanceId { id.get<std::uint64_t>() });
                }
                for (const json& id : st->at("hiddenTags")) {
                    scene.hiddenTags.insert(model::TagId { id.get<std::uint64_t>() });
                }
            }
            changes.scenes[model::SceneId { s.at("id").get<std::uint64_t>() }].after = std::move(scene);
        }
        model.apply(changes, geom::ChangeDirection::Forward);
        migrateLegacyTracks(model, legacy);
        if (doc.value("formatVersion", 0) < 3) {
            migrateMoveToRules(model);
        }
        const geom::ValidationReport report = model.validate(geom::ValidationLevel::Topology);
        if (!report.ok()) {
            result.error = "Fișierul e deteriorat: " + report.errors.front();
            return result;
        }
        result.model = std::move(model);
    } catch (const std::exception& e) {
        result.error = std::string("Fișierul e deteriorat (") + e.what() + ").";
    }
    return result;
}

std::vector<std::uint8_t> writeArchive(const model::Model& model, std::span<const std::uint8_t> thumbnailPng)
{
    const std::string text = toJson(model);
    mz_zip_archive zip {};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        return {};
    }
    bool ok = mz_zip_writer_add_mem(&zip, kModelEntry, text.data(), text.size(), MZ_BEST_COMPRESSION);
    if (ok && !thumbnailPng.empty()) {
        ok = mz_zip_writer_add_mem(&zip, kThumbnailEntry, thumbnailPng.data(), thumbnailPng.size(), MZ_NO_COMPRESSION);
    }
    for (std::size_t i = 0; ok && i < model.textures().size(); ++i) {
        const model::Texture& t = model.textures().values()[i];
        if (!t.data.empty()) {
            const std::string entry = textureEntry(model.textures().keys()[i].value, t);
            ok = mz_zip_writer_add_mem(&zip, entry.c_str(), t.data.data(), t.data.size(), MZ_NO_COMPRESSION);
        }
    }
    void* buffer = nullptr;
    std::size_t size = 0;
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    std::vector<std::uint8_t> out;
    if (ok) {
        const auto* bytes = static_cast<const std::uint8_t*>(buffer);
        out.assign(bytes, bytes + size);
    }
    mz_zip_writer_end(&zip);
    return out;
}

namespace {

std::optional<std::vector<std::uint8_t>> extract(std::span<const std::uint8_t> bytes, const char* name)
{
    mz_zip_archive zip {};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        return std::nullopt;
    }
    std::size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    std::optional<std::vector<std::uint8_t>> out;
    if (data != nullptr) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        out.emplace(p, p + size);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    return out;
}

} // namespace

LoadResult readArchive(std::span<const std::uint8_t> bytes)
{
    const auto text = extract(bytes, kModelEntry);
    if (!text) {
        LoadResult r;
        r.error = "Fișierul nu e o arhivă Aion (lipsește model.json).";
        return r;
    }
    LoadResult result = fromJson(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
    if (result.model) {
        // Imaginile texturilor, din fișierele arhivei.
        model::Model& m = *result.model;
        for (std::size_t i = 0; i < m.textures().size(); ++i) {
            const model::TextureId id = m.textures().keys()[i];
            model::Texture t = m.textures().values()[i];
            for (const char* ext : { ".png", ".jpg" }) {
                const std::string entry = "textures/" + std::to_string(id.value) + ext;
                if (auto data = extract(bytes, entry.c_str())) {
                    t.data = std::move(*data);
                    m.setTexture(id, std::move(t));
                    break;
                }
            }
        }
    }
    return result;
}

std::vector<std::uint8_t> readThumbnail(std::span<const std::uint8_t> bytes)
{
    return extract(bytes, kThumbnailEntry).value_or(std::vector<std::uint8_t> {});
}

bool saveFile(const std::filesystem::path& path, const model::Model& model, std::span<const std::uint8_t> thumbnailPng,
    std::string* error)
{
    const std::vector<std::uint8_t> bytes = writeArchive(model, thumbnailPng);
    if (bytes.empty()) {
        if (error != nullptr) {
            *error = "Arhiva nu a putut fi creată.";
        }
        return false;
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            if (error != nullptr) {
                *error = "Fișierul nu a putut fi scris.";
            }
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        if (error != nullptr) {
            *error = "Fișierul nu a putut fi înlocuit.";
        }
        return false;
    }
    return true;
}

LoadResult loadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LoadResult r;
        r.error = "Fișierul nu poate fi deschis.";
        return r;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return readArchive(bytes);
}

} // namespace aion::io
