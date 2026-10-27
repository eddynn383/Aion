#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"

#include <optional>
#include <vector>

// Formele recunoscute din geometrie (modelul ține doar muchii și fețe, ca SketchUp): o linie, un arc, un cerc, un
// poligon regulat sau un dreptunghi. Inspector-ul le arată mărimile (lungime, rază, segmente, laturi) și le schimbă.
namespace aion::geom {

struct ShapeInfo {
    enum class Kind { Line, Arc, Circle, Polygon, Rectangle };
    Kind kind = Kind::Line;
    std::vector<VertexId> vertices; // în ordine (pentru dreptunghi: colțurile, pentru o curbă: de-a lungul ei)
    std::vector<EdgeId> edges;
    FaceId face; // fața formei (cerc, poligon, dreptunghi plin), dacă există
    Vec3d center { 0.0 };
    Vec3d normal { 0.0, 0.0, 1.0 };
    double length = 0.0; // linie: lungimea; curbă: lungimea totală
    double radius = 0.0; // arc, cerc, poligon
    double angle = 0.0; // arc: unghiul în grade
    int segments = 0; // arc, cerc: segmente; poligon: laturi
    double width = 0.0; // dreptunghi (prima latură)
    double height = 0.0;
    // Numărul de segmente se poate schimba doar pentru o formă plană de sine stătătoare (înainte de Push/Pull).
    bool segmentsEditable = false;
};

// Forma selecției: o muchie, o curbă întreagă sau o față (cu muchiile ei); nullopt dacă nu e o formă cunoscută.
[[nodiscard]] std::optional<ShapeInfo> recognizeShape(const Mesh& mesh, const Selection& selection);

// Schimbările (fiecare păstrează restul geometriei legate, ca Move / Scale pe vârfuri). Întorc false dacă nu se poate.
bool setLineLength(Mesh& mesh, const ShapeInfo& shape, double length);
bool setShapeRadius(Mesh& mesh, const ShapeInfo& shape, double radius);
bool setRectangleSize(Mesh& mesh, const ShapeInfo& shape, double width, double height);
// Redesenează curba cu alt număr de segmente; întoarce selecția nouă (muchiile curbei și fața ei).
std::optional<Selection> setShapeSegments(Mesh& mesh, const ShapeInfo& shape, int segments);

} // namespace aion::geom
