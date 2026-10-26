#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Tipurile de bază pentru geometrie: double peste tot în core (float doar pe GPU).
namespace aion::math {

using Vec2d = glm::dvec2;
using Vec3d = glm::dvec3;
using Mat4d = glm::dmat4;

} // namespace aion::math
