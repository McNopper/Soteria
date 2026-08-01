/// @file horizon_geometry.cpp
/// @brief Artificial-horizon vertex geometry computation.

#include "horizon_geometry.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace sim {
namespace rendering {

namespace {

static constexpr float kPi     {3.14159265358979323846F};
static constexpr float kDegToRad{kPi / 180.0F};

/// Transform an attitude-space point to NDC using roll and pitch offset.
Vertex2D AttToNdc(float xAtt, float yAtt,
                  float cosR, float sinR, float pitchNdc) noexcept
{
    const float yScreen = pitchNdc - yAtt;
    Vertex2D v{};
    v.x = cosR * xAtt - sinR * yScreen;
    v.y = sinR * xAtt + cosR * yScreen;
    return v;
}

} /* anonymous namespace */

// ---- ComputeHorizonVertices -------------------------------------------------

void ComputeHorizonVertices(float rollDeg, float pitchDeg, float aspectRatio,
                             std::array<Vertex2D, kHorizonVertexCount>& verts) noexcept
{
    const float rollRad  = rollDeg  * kDegToRad;
    const float cosR     = std::cos(rollRad);
    const float sinR     = std::sin(rollRad);
    const float pitchNdc = pitchDeg / 30.0F;

    // Guard against invalid aspect ratio (division safety).
    const float safeAR = (aspectRatio > 0.1F) ? aspectRatio : (16.0F / 9.0F);

    uint32_t vi{0U};

    // ---- Horizon line [0..1] ------------------------------------------------
    // Full-width line in attitude space at yAtt = 0 (the horizon).
    verts[vi++] = AttToNdc(-0.92F, 0.0F, cosR, sinR, pitchNdc);
    verts[vi++] = AttToNdc( 0.92F, 0.0F, cosR, sinR, pitchNdc);

    // ---- Pitch ladder [2..13] (+/-5, +/-10, +/-15 degrees) -----------------
    // Line half-widths in NDC.  Divided by aspectRatio so they appear as
    // equal-length lines on screen regardless of display resolution.
    static constexpr std::array<float, 3U> kPitchAngles{5.0F, 10.0F, 15.0F};
    static constexpr std::array<float, 3U> kPitchWidths{0.22F, 0.30F, 0.38F};

    for (uint32_t i{0U}; i < 3U; ++i)
    {
        const float yP = kPitchAngles[i] / 30.0F;
        const float w  = kPitchWidths[i] / safeAR;

        // Above horizon (positive yAtt = above = upper screen area).
        verts[vi++] = AttToNdc(-w,  yP, cosR, sinR, pitchNdc);
        verts[vi++] = AttToNdc( w,  yP, cosR, sinR, pitchNdc);
        // Below horizon (negative yAtt = below = lower screen area).
        verts[vi++] = AttToNdc(-w, -yP, cosR, sinR, pitchNdc);
        verts[vi++] = AttToNdc( w, -yP, cosR, sinR, pitchNdc);
    }
    /* vi == 14 */

    // ---- Roll arc [14..61] (screen-space, not rotated by roll) -------------
    // Arc centred at (0, 0), radius kArcR, spans -60 to +60 degrees from
    // vertical ("up" = negative Y in Vulkan NDC).
    // x / aspectRatio keeps the arc circular on non-square displays.
    static constexpr float    kArcR     {0.62F};
    static constexpr uint32_t kArcSegs  {24U};
    static constexpr float    kArcStart {-60.0F * kDegToRad};
    static constexpr float    kArcStep  {(120.0F * kDegToRad) / static_cast<float>(kArcSegs)};

    for (uint32_t s{0U}; s < kArcSegs; ++s)
    {
        const float a0 = kArcStart + static_cast<float>(s)      * kArcStep;
        const float a1 = kArcStart + static_cast<float>(s + 1U) * kArcStep;
        verts[vi].x = kArcR * std::sin(a0) / safeAR;
        verts[vi].y = -kArcR * std::cos(a0);
        ++vi;
        verts[vi].x = kArcR * std::sin(a1) / safeAR;
        verts[vi].y = -kArcR * std::cos(a1);
        ++vi;
    }
    /* vi == 62 */

    // ---- Roll reference triangle [62..67] -----------------------------------
    // Downward-pointing equilateral triangle at the 0-degree (top-centre)
    // position of the arc.  Indicates zero roll.
    static constexpr float kTriApexY{-kArcR};          /* top of arc at 0 deg */
    static constexpr float kTriH    {0.055F};           /* triangle height     */
    static constexpr float kTriW    {0.035F / (16.0F / 9.0F)}; /* half-base   */

    // Line 1: left edge
    verts[vi].x = -kTriW;  verts[vi].y = kTriApexY + kTriH;  ++vi;
    verts[vi].x =  0.0F;   verts[vi].y = kTriApexY;          ++vi;
    // Line 2: right edge
    verts[vi].x =  0.0F;   verts[vi].y = kTriApexY;          ++vi;
    verts[vi].x =  kTriW;  verts[vi].y = kTriApexY + kTriH;  ++vi;
    // Line 3: base
    verts[vi].x = -kTriW;  verts[vi].y = kTriApexY + kTriH;  ++vi;
    verts[vi].x =  kTriW;  verts[vi].y = kTriApexY + kTriH;  ++vi;
    /* vi == 68 */

    // ---- Roll tick marks [68..87] ------------------------------------------
    // Tick marks at +/-10, +/-20, +/-30, +/-45, +/-60 degrees on the arc.
    static constexpr std::array<float, 5U> kTickAngles{10.0F, 20.0F, 30.0F, 45.0F, 60.0F};
    static constexpr float kTickOuter{kArcR};
    static constexpr float kTickInner{kArcR - 0.05F};

    for (uint32_t t{0U}; t < 5U; ++t)
    {
        // Positive (right) tick
        {
            const float a = kTickAngles[t] * kDegToRad;
            verts[vi].x = kTickOuter * std::sin(a) / safeAR;
            verts[vi].y = -kTickOuter * std::cos(a);
            ++vi;
            verts[vi].x = kTickInner * std::sin(a) / safeAR;
            verts[vi].y = -kTickInner * std::cos(a);
            ++vi;
        }
        // Negative (left) tick
        {
            const float a = -kTickAngles[t] * kDegToRad;
            verts[vi].x = kTickOuter * std::sin(a) / safeAR;
            verts[vi].y = -kTickOuter * std::cos(a);
            ++vi;
            verts[vi].x = kTickInner * std::sin(a) / safeAR;
            verts[vi].y = -kTickInner * std::cos(a);
            ++vi;
        }
    }
    /* vi == 88 */

    // ---- Aircraft symbol [88..93] (screen-space, fixed at centre) -----------
    // Three lines: left wing, right wing, nose indicator.
    const float wingW = 0.20F / safeAR;
    const float wingGap = 0.07F / safeAR;

    verts[vi].x = -wingW;   verts[vi].y =  0.0F;   ++vi;  /* left wing outer */
    verts[vi].x = -wingGap; verts[vi].y =  0.0F;   ++vi;  /* left wing inner */
    verts[vi].x =  wingGap; verts[vi].y =  0.0F;   ++vi;  /* right wing inner */
    verts[vi].x =  wingW;   verts[vi].y =  0.0F;   ++vi;  /* right wing outer */
    verts[vi].x =  0.0F;    verts[vi].y = -0.04F;  ++vi;  /* nose top */
    verts[vi].x =  0.0F;    verts[vi].y =  0.04F;  ++vi;  /* nose bottom */
    /* vi == 94 */
}

} /* namespace rendering */
} /* namespace sim */
