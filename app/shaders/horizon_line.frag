#version 450

// horizon_line.frag
// Solid-colour fragment shader for horizon overlay lines.
// Colour is supplied via push constants.
//
// Push constant layout (32 bytes, shared with BG pipeline):
//   offset  0 : float cosR      (unused in line shader)
//   offset  4 : float sinR      (unused in line shader)
//   offset  8 : float pitchNdc  (unused in line shader)
//   offset 12 : float pad
//   offset 16 : float r
//   offset 20 : float g
//   offset 24 : float b
//   offset 28 : float a

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

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(pc.r, pc.g, pc.b, pc.a);
}
