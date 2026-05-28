/// @file horizon_geometry.hpp
/// @brief Artificial-horizon vertex geometry computation -- no Vulkan dependency.
///
/// This module is simulation/example code.  It computes the NDC-space vertex
/// positions for the artificial horizon display.  The computation is pure
/// arithmetic and is fully testable without a GPU or Vulkan context.
///
/// Coordinate conventions
/// ----------------------
/// Vulkan NDC: X in [-1, 1] left-to-right, Y in [-1, 1] top-to-bottom (Y+ = DOWN).
///
/// Attitude space: a 2D space aligned with the aircraft horizon.
///   yAtt > 0  = above the horizon (upper screen area, negative Vulkan Y).
///   yAtt < 0  = below the horizon (lower screen area, positive Vulkan Y).
///
/// Attitude-to-NDC transform (applied to horizon line and pitch ladder):
///   yScreen  = pitchNdc - yAtt             (pitchNdc = pitchDeg / 30.0)
///   xNdc     = cosR * xAtt - sinR * yScreen
///   yNdc     = sinR * xAtt + cosR * yScreen
///
/// Roll arc, tick marks, and aircraft symbol are drawn in screen-space NDC
/// (no roll rotation applied -- they are computed directly in NDC).
///
/// Vertex layout (LINE_LIST primitive -- every two vertices form one line):
///   [  0..  1]  Horizon line                       (1 line,  2 verts)
///   [  2.. 13]  Pitch ladder +/-5,+/-10,+/-15 deg  (6 lines, 12 verts)
///   [ 14.. 61]  Roll arc 24 segments               (48 verts)
///   [ 62.. 67]  Roll reference triangle             (3 lines, 6 verts)
///   [ 68.. 87]  Roll tick marks +/-10,20,30,45,60  (10 marks, 20 verts)
///   [ 88.. 93]  Aircraft symbol (wings + nose)      (3 lines, 6 verts)
///   Total: 94 vertices
///
/// @satisfies SWS_HORIZON_010  Geometry computation is isolated from Vulkan.
/// @satisfies SWS_HORIZON_011  All array sizes are compile-time constants.

#ifndef VKSC_SIM_RENDERING_HORIZON_GEOMETRY_HPP
#define VKSC_SIM_RENDERING_HORIZON_GEOMETRY_HPP

#include <cstdint>

namespace sim {
namespace rendering {

/// @brief 2D vertex in NDC space.
struct Vertex2D
{
    float x{0.0F};
    float y{0.0F};
};

/// @brief Total number of vertices in the horizon vertex buffer.
static constexpr uint32_t kHorizonVertexCount{94U};

/// @brief Compute the full set of artificial-horizon NDC vertices.
///
/// @param rollDeg    Current roll angle (positive = right bank).
/// @param pitchDeg   Current pitch angle (positive = nose up).
/// @param aspectRatio  Display width / height (used to keep arc circular).
/// @param[out] verts  Output array of exactly kHorizonVertexCount vertices.
void ComputeHorizonVertices(float rollDeg,
                            float pitchDeg,
                            float aspectRatio,
                            Vertex2D (&verts)[kHorizonVertexCount]) noexcept;

} /* namespace rendering */
} /* namespace sim */

#endif /* VKSC_SIM_RENDERING_HORIZON_GEOMETRY_HPP */
