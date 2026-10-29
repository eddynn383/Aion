#include "aion/core/geom/Bezier.h"

#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace aion::geom {

namespace {

constexpr int kTableSubdivisions = 64;
constexpr int kMaxSubdivisions = 64;

std::array<Vec3d, 4> controls(const BezierPath& path, std::size_t segment)
{
    const std::size_t n = path.points.size();
    const BezierPoint& a = path.points[segment % n];
    const BezierPoint& b = path.points[(segment + 1) % n];
    return { a.point, a.out, b.in, b.point };
}

Vec3d derivative(const std::array<Vec3d, 4>& c, double t)
{
    const double u = 1.0 - t;
    return 3.0 * u * u * (c[1] - c[0]) + 6.0 * u * t * (c[2] - c[1]) + 3.0 * t * t * (c[3] - c[2]);
}

Vec3d evaluate(const std::array<Vec3d, 4>& c, double t)
{
    const double u = 1.0 - t;
    return u * u * u * c[0] + 3.0 * u * u * t * c[1] + 3.0 * u * t * t * c[2] + t * t * t * c[3];
}

Vec3d lerp(const Vec3d& a, const Vec3d& b, double t)
{
    return a + (b - a) * t;
}

} // namespace

std::size_t BezierPath::segmentCount() const
{
    if (points.size() < 2) {
        return 0;
    }
    return closed ? points.size() : points.size() - 1;
}

Vec3d bezierPoint(const BezierPath& path, std::size_t segment, double t)
{
    return evaluate(controls(path, segment), std::clamp(t, 0.0, 1.0));
}

Vec3d bezierTangent(const BezierPath& path, std::size_t segment, double t)
{
    const std::array<Vec3d, 4> c = controls(path, segment);
    t = std::clamp(t, 0.0, 1.0);
    Vec3d d = derivative(c, t);
    if (glm::length(d) <= 1e-9) {
        // Mânere pe punct: derivata se anulează la capăt; direcția din vecinătate.
        constexpr double h = 1e-4;
        d = evaluate(c, std::min(t + h, 1.0)) - evaluate(c, std::max(t - h, 0.0));
    }
    if (glm::length(d) <= 1e-9) {
        d = c[3] - c[0];
    }
    if (glm::length(d) <= 1e-9) {
        return { 1.0, 0.0, 0.0 };
    }
    return glm::normalize(d);
}

std::vector<Vec3d> sampleBezier(const BezierPath& path, double tolerance)
{
    std::vector<Vec3d> out;
    const std::size_t segments = path.segmentCount();
    if (segments == 0) {
        return out;
    }
    const double tol = std::max(tolerance, tol::kLength);
    const auto push = [&](const Vec3d& p) {
        if (out.empty() || glm::length(p - out.back()) > tol::kLength) {
            out.push_back(p);
        }
    };
    for (std::size_t s = 0; s < segments; ++s) {
        const std::array<Vec3d, 4> c = controls(path, s);
        // Abaterea maximă a unei curbe cubice de la coardă, după n subdiviziuni egale, e cel mult
        // ¾ · max(|P0 − 2P1 + P2|, |P1 − 2P2 + P3|) / n². Mânerele pe coardă = un segment drept: o singură coardă.
        const double bend = std::max(glm::length(c[0] - 2.0 * c[1] + c[2]), glm::length(c[1] - 2.0 * c[2] + c[3]));
        const auto offChord = [&](const Vec3d& p) {
            const Vec3d chord = c[3] - c[0];
            const double len = glm::length(chord);
            if (len <= tol::kLength) {
                return glm::length(p - c[0]);
            }
            const double along = glm::dot(p - c[0], chord) / len;
            if (along < -tol || along > len + tol) {
                return std::abs(along) + tol; // mânerul trece de capete: nu e drept
            }
            return glm::length(p - c[0] - chord * (along / len));
        };
        int n = 1;
        if (0.75 * bend > tol && (offChord(c[1]) > tol || offChord(c[2]) > tol)) {
            n = std::clamp(static_cast<int>(std::ceil(std::sqrt(0.75 * bend / tol))), 2, kMaxSubdivisions);
        }
        if (s == 0) {
            push(c[0]);
        }
        for (int i = 1; i <= n; ++i) {
            push(evaluate(c, static_cast<double>(i) / n));
        }
    }
    if (path.closed && out.size() > 1 && glm::length(out.back() - out.front()) <= tol::kLength) {
        out.pop_back();
    }
    return out;
}

double defaultTolerance(const BezierPath& path)
{
    if (path.points.empty()) {
        return 0.5;
    }
    Vec3d lo { path.points.front().point };
    Vec3d hi { lo };
    for (const BezierPoint& p : path.points) {
        for (const Vec3d& v : { p.point, p.in, p.out }) {
            lo = glm::min(lo, v);
            hi = glm::max(hi, v);
        }
    }
    return std::clamp(glm::length(hi - lo) * 0.002, 0.5, 10.0);
}

ArcLength::ArcLength(const BezierPath& path)
    : m_path(path)
{
    const std::size_t segments = m_path.segmentCount();
    double total = 0.0;
    for (std::size_t s = 0; s < segments; ++s) {
        const std::array<Vec3d, 4> c = controls(m_path, s);
        Vec3d previous = c[0];
        m_table.push_back({ total, s, 0.0 });
        for (int i = 1; i <= kTableSubdivisions; ++i) {
            const double t = static_cast<double>(i) / kTableSubdivisions;
            const Vec3d p = evaluate(c, t);
            total += glm::length(p - previous);
            previous = p;
            m_table.push_back({ total, s, t });
        }
    }
    m_length = total;
    // Colțurile: punctele (interioare, sau toate pe o cale închisă) unde tangenta de sosire și cea de plecare diferă.
    const std::size_t n = m_path.points.size();
    for (std::size_t i = m_path.closed ? 0 : 1; i < n && segments > 0; ++i) {
        if (!m_path.closed && i + 1 >= n) {
            break;
        }
        const std::size_t before = (i + segments - 1) % segments; // segmentul care se termină în punctul i
        const std::size_t after = i % segments;
        if (segmentLength(before) <= tol::kLength || segmentLength(after) <= tol::kLength) {
            continue;
        }
        const Vec3d in = bezierTangent(m_path, before, 1.0);
        const Vec3d out = bezierTangent(m_path, after, 0.0);
        if (glm::dot(in, out) < std::cos(glm::radians(1.0))) {
            m_corners.push_back({ m_table[after * (kTableSubdivisions + 1)].distance, in, out });
        }
    }
}

double ArcLength::segmentLength(std::size_t segment) const
{
    const std::size_t first = segment * (kTableSubdivisions + 1);
    return first + kTableSubdivisions < m_table.size() ? m_table[first + kTableSubdivisions].distance - m_table[first].distance
                                                       : 0.0;
}

std::pair<std::size_t, double> ArcLength::locate(double distance) const
{
    // Intervalul din tabel (în același segment): t interpolat liniar după distanță.
    const auto it = std::lower_bound(m_table.begin(), m_table.end(), distance,
        [](const Entry& e, double d) { return e.distance < d; });
    if (it == m_table.begin()) {
        return { 0, 0.0 };
    }
    const Entry& b = it == m_table.end() ? m_table.back() : *it;
    const Entry& a = *(it - 1);
    double t = b.t;
    if (a.segment == b.segment && b.distance - a.distance > 1e-12) {
        t = a.t + (b.t - a.t) * (distance - a.distance) / (b.distance - a.distance);
        // Viteza curbei se schimbă în interval (mai ales lângă mânere scurte): doi pași Newton pe lungime.
        const std::array<Vec3d, 4> c = controls(m_path, b.segment);
        const Vec3d start = evaluate(c, a.t);
        for (int i = 0; i < 2; ++i) {
            const double speed = glm::length(derivative(c, t));
            if (speed <= 1e-9) {
                break;
            }
            const double error = (distance - a.distance) - glm::length(evaluate(c, t) - start);
            t = std::clamp(t + error / speed, a.t, b.t);
        }
    }
    return { b.segment, t };
}

PathFrame ArcLength::at(double distance) const
{
    return at(distance, 0.0);
}

PathFrame ArcLength::at(double distance, double cornerBlend) const
{
    const std::size_t segments = m_path.segmentCount();
    if (segments == 0) {
        return { m_path.points.empty() ? Vec3d { 0.0 } : m_path.points.front().point, { 1.0, 0.0, 0.0 } };
    }
    // Capetele unei căi deschise: prelungirea dreaptă pe tangenta primului / ultimului segment care are lungime.
    const auto startTangent = [&] {
        std::size_t s = 0;
        while (s + 1 < segments && segmentLength(s) <= tol::kLength) {
            ++s;
        }
        return bezierTangent(m_path, s, 0.0);
    };
    const auto endTangent = [&] {
        std::size_t s = segments - 1;
        while (s > 0 && segmentLength(s) <= tol::kLength) {
            --s;
        }
        return bezierTangent(m_path, s, 1.0);
    };
    if (m_path.closed && m_length > tol::kLength) {
        distance = std::fmod(distance, m_length);
        if (distance < 0.0) {
            distance += m_length;
        }
    } else if (distance < 0.0 || m_length <= tol::kLength) {
        const Vec3d tangent = startTangent();
        return { m_path.points.front().point + tangent * distance, tangent };
    } else if (distance > m_length) {
        const Vec3d tangent = endTangent();
        return { bezierPoint(m_path, segments - 1, 1.0) + tangent * (distance - m_length), tangent };
    }
    const auto [segment, t] = locate(distance);
    PathFrame frame { bezierPoint(m_path, segment, t), bezierTangent(m_path, segment, t) };
    if (cornerBlend <= 0.0) {
        return frame;
    }
    for (const Corner& c : m_corners) {
        double delta = distance - c.distance;
        if (m_path.closed) {
            delta = std::remainder(delta, m_length); // în jurul punctului de închidere, de o parte și de alta
        }
        if (std::abs(delta) >= cornerBlend * 0.5) {
            continue;
        }
        // De la tangenta de sosire la cea de plecare, lin (smoothstep): înainte de colț pe tangenta exactă de sosire,
        // după el pe cea exactă de plecare.
        const double u = (delta + cornerBlend * 0.5) / cornerBlend;
        const double k = u * u * (3.0 - 2.0 * u);
        const Vec3d in = delta < -tol::kLength ? frame.tangent : c.in;
        const Vec3d out = delta > tol::kLength ? frame.tangent : c.out;
        const Vec3d mixed = in * (1.0 - k) + out * k;
        if (glm::length(mixed) > 1e-9) {
            frame.tangent = glm::normalize(mixed);
        }
        break;
    }
    return frame;
}

BezierPath insertPoint(const BezierPath& path, std::size_t segment, double t)
{
    if (segment >= path.segmentCount()) {
        return path;
    }
    t = std::clamp(t, 0.0, 1.0);
    const std::size_t n = path.points.size();
    const std::size_t next = (segment + 1) % n;
    const std::array<Vec3d, 4> c = controls(path, segment);
    const Vec3d p01 = lerp(c[0], c[1], t);
    const Vec3d p12 = lerp(c[1], c[2], t);
    const Vec3d p23 = lerp(c[2], c[3], t);
    const Vec3d p012 = lerp(p01, p12, t);
    const Vec3d p123 = lerp(p12, p23, t);
    BezierPath out = path;
    out.points[segment].out = p01;
    out.points[next].in = p23;
    const BezierPoint inserted { lerp(p012, p123, t), p012, p123, true };
    out.points.insert(out.points.begin() + static_cast<std::ptrdiff_t>(segment + 1), inserted);
    return out;
}

BezierPath removePoint(const BezierPath& path, std::size_t index)
{
    BezierPath out = path;
    if (index < out.points.size()) {
        out.points.erase(out.points.begin() + static_cast<std::ptrdiff_t>(index));
    }
    if (out.points.size() < 3) {
        out.closed = false;
    }
    return out;
}

BezierPath transformed(const BezierPath& path, const math::Transform& t)
{
    BezierPath out = path;
    for (BezierPoint& p : out.points) {
        p.point = t.applyToPoint(p.point);
        p.in = t.applyToPoint(p.in);
        p.out = t.applyToPoint(p.out);
    }
    return out;
}

} // namespace aion::geom
