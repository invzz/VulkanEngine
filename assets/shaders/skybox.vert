#version 450

layout(location = 0) out vec3 fragTexCoord;

// Cube vertices (no vertex buffer needed - generate from gl_VertexIndex)
vec3 positions[36] = vec3[](
        // Front face
        vec3(-1.0, -1.0, 1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(-1.0, 1.0, 1.0),
        vec3(-1.0, -1.0, 1.0),
        // Back face
        vec3(1.0, -1.0, -1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, -1.0, -1.0),
        // Top face
        vec3(-1.0, 1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, 1.0, 1.0),
        // Bottom face
        vec3(-1.0, -1.0, -1.0),
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(-1.0, -1.0, 1.0),
        vec3(-1.0, -1.0, -1.0),
        // Right face
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, -1.0, 1.0),
        // Left face
        vec3(-1.0, -1.0, -1.0),
        vec3(-1.0, -1.0, 1.0),
        vec3(-1.0, 1.0, 1.0),
        vec3(-1.0, 1.0, 1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, -1.0, -1.0));

layout(push_constant) uniform PushConstants
{
  mat4 viewProjection;
}
push;

void main()
{
  vec3 pos = positions[gl_VertexIndex];

  // Use position as texture coordinate (cubemap sampling)
  fragTexCoord = pos;

  // Transform position by view-projection
  // Note: We remove translation from view matrix so skybox stays at origin
  vec4 clipPos = push.viewProjection * vec4(pos, 1.0);

  // Set z = w so depth is always 1.0 (at far plane)
  // This ensures skybox renders behind everything
  gl_Position = clipPos.xyww;
}
