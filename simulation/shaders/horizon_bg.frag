#version 450

// horizon_bg.frag
// Classifies each fragment as sky, earth, or horizon band by un-rotating
// the screen NDC coordinate to attitude space and comparing with pitchNdc.
//
// Push constant layout (32 bytes, shared with line pipeline):
//   offset  0 : float cosR       cos of current roll angle
//   offset  4 : float sinR       sin of current roll angle
//   offset  8 : float pitchNdc   pitch offset (pitchDeg / 30.0)
//   offset 12 : float pad
//   offset 16 : float r          (unused in BG shader)
//   offset 20 : float g
//   offset 24 : float b
//   offset 28 : float a
//
// Colour scheme (aviation convention):
//   Sky   : #456D95  (0.271, 0.427, 0.584)
//   Band  : #F2F2F2  (0.949, 0.949, 0.949)  -- 1-degree transition band
//   Earth : #7A522F  (0.478, 0.322, 0.184)

layout(push_constant) uniform PC {
    float cosR;
    float sinR;
    float pitchNdc;
    float pad;
    float r;
    float g;
    float b;
    float a;
} pc;

layout(location = 0) in  vec2 vNdc;
layout(location = 0) out vec4 outColor;

void main()
{
    // Un-rotate from screen NDC to attitude-space yScreen.
    // Derivation: if xNdc = cosR*xAtt - sinR*yScreen
    //                yNdc = sinR*xAtt + cosR*yScreen
    // then yScreen = -sinR * xNdc + cosR * yNdc
    float yScreen = -pc.sinR * vNdc.x + pc.cosR * vNdc.y;

    // Band half-width in NDC (1 degree = 1/30 NDC).
    float bandHalf = (1.0 / 30.0) * 0.25;

    float distFromHorizon = yScreen - pc.pitchNdc;

    vec4 sky   = vec4(0.271, 0.427, 0.584, 1.0);
    vec4 earth = vec4(0.478, 0.322, 0.184, 1.0);
    vec4 band  = vec4(0.949, 0.949, 0.949, 1.0);

    if (distFromHorizon < -bandHalf)
    {
        outColor = sky;
    }
    else if (distFromHorizon > bandHalf)
    {
        outColor = earth;
    }
    else
    {
        // Smooth blend through the band.
        float t = (distFromHorizon + bandHalf) / (2.0 * bandHalf);
        outColor = mix(sky, earth, t) * 0.5 + band * 0.5;
    }
}
