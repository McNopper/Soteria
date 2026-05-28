#version 450

// horizon_line.vert
// Passthrough vertex shader for pre-transformed NDC line geometry.
// Vertices are already in NDC (computed by the CPU in horizon_geometry.cpp).

layout(location = 0) in vec2 inPos;

void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
}
