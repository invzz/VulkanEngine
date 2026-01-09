#version 450

layout(location = 0) out vec2 vNdc;

// Fullscreen triangle
vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2(3.0, -1.0),
    vec2(-1.0, 3.0));

void main() {
    vec2 pos = positions[gl_VertexIndex];
    vNdc = pos;
    // Write skybox at far plane so it only appears where the depth buffer is still 1.0.
    gl_Position = vec4(pos, 1.0, 1.0);
}
