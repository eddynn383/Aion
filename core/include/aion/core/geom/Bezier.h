#pragma once

#include "aion/core/math/Transform.h"
#include "aion/core/math/Vec.h"

#include <cstddef>
#include <vector>

// Curbele Bézier cubice ale Pen-ului (ca în Figma): puncte cu câte două mânere. În Modelare devin muchii (eșantionate),
// în Animare sunt căi de parcurgere (parametrizate după lungime, ca viteza să urmeze easing-ul pe drumul real).
namespace aion::geom {

using math::Vec3d;

struct BezierPoint {
    Vec3d point { 0.0 };
    Vec3d in { 0.0 }; // mânerul de intrare (absolut); == point → fără mâner
    Vec3d out { 0.0 }; // mânerul de ieșire
    bool smooth = false; // mânerele legate (o tragere a unuia îl oglindește pe celălalt)
    bool operator==(const BezierPoint&) const = default;
};

struct BezierPath {
    std::vector<BezierPoint> points;
    bool closed = false;
    // closed ? n : n − 1 (0 sub 2 puncte). Segmentul i merge de la punctul i la (i + 1) mod n.
    [[nodiscard]] std::size_t segmentCount() const;
    bool operator==(const BezierPath&) const = default;
};

[[nodiscard]] Vec3d bezierPoint(const BezierPath& path, std::size_t segment, double t);
// Tangenta unitară; unde derivata e nulă (mânere pe punct), direcția din vecinătate, apoi coarda, apoi (1, 0, 0).
[[nodiscard]] Vec3d bezierTangent(const BezierPath& path, std::size_t segment, double t);

// Eșantionarea pentru muchii: abaterea față de coardă ≤ tolerance (mm); un segment drept dă o singură coardă, unul
// curb 2..64. Fără puncte duplicate; pentru o cale închisă, primul punct nu se repetă la final.
[[nodiscard]] std::vector<Vec3d> sampleBezier(const BezierPath& path, double tolerance);
// Toleranța implicită: 0.2% din diagonala cutiei punctelor și mânerelor, între 0.5 și 10 mm.
[[nodiscard]] double defaultTolerance(const BezierPath& path);

struct PathFrame {
    Vec3d point { 0.0 };
    Vec3d tangent { 1.0, 0.0, 0.0 };
};

// Parametrizarea după lungime (tabel dens: 64 de subdiviziuni pe segment); tangenta din derivata Bézier.
class ArcLength {
public:
    explicit ArcLength(const BezierPath& path);
    [[nodiscard]] double length() const { return m_length; }
    // distance < 0 sau > length: pe o cale deschisă, prelungirea dreaptă pe tangenta capătului; pe una închisă,
    // distanța se ia modulo lungimea.
    [[nodiscard]] PathFrame at(double distance) const;
    // La fel, dar la un colț (un punct fără mânere netede) tangenta se întoarce treptat, pe `cornerBlend` mm în jurul
    // lui (punctul rămâne pe curbă): un obiect pe cale nu se rotește brusc.
    [[nodiscard]] PathFrame at(double distance, double cornerBlend) const;

private:
    struct Entry {
        double distance = 0.0;
        std::size_t segment = 0;
        double t = 0.0;
    };
    struct Corner {
        double distance = 0.0;
        Vec3d in { 1.0, 0.0, 0.0 }; // tangenta la sosire
        Vec3d out { 1.0, 0.0, 0.0 }; // tangenta la plecare
    };
    [[nodiscard]] std::pair<std::size_t, double> locate(double distance) const; // pe curbă, 0 ≤ distance ≤ length
    [[nodiscard]] double segmentLength(std::size_t segment) const;
    BezierPath m_path;
    std::vector<Entry> m_table;
    std::vector<Corner> m_corners;
    double m_length = 0.0;
};

// Editare. La inserare forma rămâne aceeași (de Casteljau): punctul nou e neted, cu mânerele din împărțire.
[[nodiscard]] BezierPath insertPoint(const BezierPath& path, std::size_t segment, double t);
[[nodiscard]] BezierPath removePoint(const BezierPath& path, std::size_t index);
[[nodiscard]] BezierPath transformed(const BezierPath& path, const math::Transform& t);

} // namespace aion::geom
