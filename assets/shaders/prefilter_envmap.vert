#version 450

layout(push_constant) uniform PushConstants
{
  mat4  viewProjection;
  int   faceIndex;
  float roughness;
}
push;

layout(location = 0) out vec3 localPos;

// Cube vertices (no vertex buffer needed)
const vec3 positions[36] = vec3[](
        // Front face
        vec3(-1, -1, 1),
        vec3(1, -1, 1),
        vec3(1, 1, 1),
        vec3(1, 1, 1),
        vec3(-1, 1, 1),
        vec3(-1, -1, 1),
        // Back face
        vec3(1, -1, -1),
        vec3(-1, -1, -1),
        vec3(-1, 1, -1),
        vec3(-1, 1, -1),
        vec3(1, 1, -1),
        vec3(1, -1, -1),
        // Top face
        vec3(-1, 1, 1),
        vec3(1, 1, 1),
        vec3(1, 1, -1),
        vec3(1, 1, -1),
        vec3(-1, 1, -1),
        vec3(-1, 1, 1),
        // Bottom face
        vec3(-1, -1, -1),
        vec3(1, -1, -1),
        vec3(1, -1, 1),
        vec3(1, -1, 1),
        vec3(-1, -1, 1),
        vec3(-1, -1, -1),
        // Right face
        vec3(1, -1, 1),
        vec3(1, -1, -1),
        vec3(1, 1, -1),
        vec3(1, 1, -1),
        vec3(1, 1, 1),
        vec3(1, -1, 1),
        // Left face
        vec3(-1, -1, -1),
        vec3(-1, -1, 1),
        vec3(-1, 1, 1),
        vec3(-1, 1, 1),
        vec3(-1, 1, -1),
        vec3(-1, -1, -1));

void main()
{
  localPos    = positions[gl_VertexIndex];
  gl_Position = push.viewProjection * vec4(localPos, 1.0);
}
