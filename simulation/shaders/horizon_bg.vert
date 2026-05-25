#version 450

// horizon_bg.vert
// Fullscreen triangle using the "oversized triangle" technique.
// No vertex buffer needed: gl_VertexIndex drives the position.
//
// Output: vNdc carries the NDC coordinate to the fragment shader
// so the background fragment shader can un-rotate to attitude space.

layout(location = 0) out vec2 vNdc;

void main()
{
    // Three oversized triangle vertices that cover the entire NDC clip volume.
    // Vertex 0: (-1, -1)  Vertex 1: (3, -1)  Vertex 2: (-1, 3)
    vec2 pos = vec2(
        float((gl_VertexIndex & 1) << 2) - 1.0,
        float((gl_VertexIndex & 2) << 1) - 1.0
    );
    vNdc        = pos;
    gl_Position = vec4(pos, 0.0, 1.0);
}
