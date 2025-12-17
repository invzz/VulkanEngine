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
        vec3(1.0, -1.0, -1.0),
        vec3(1.0, 1.0, -1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, -1.0, 1.0),
        vec3(1.0, -1.0, -1.0),
        // Left face
        vec3(-1.0, -1.0, 1.0),
        vec3(-1.0, 1.0, 1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, 1.0, -1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3(-1.0, -1.0, 1.0));

layout(push_constant) uniform PushConstants
{
  mat4  viewProjection;
  vec4  sunDirection; // w = intensity
  vec4  sunColor;     // w = padding
  float rayleigh;
  float mie;
  float mieEccentricity;
  float padding;
}
push;

void main()
{
  vec3 pos     = positions[gl_VertexIndex];
  fragTexCoord = pos;

  // Remove translation from view matrix for skybox
  // We assume the viewProjection passed in already has translation removed or we handle it here
  // Usually for skybox we want it centered on camera.
  // The standard way is to use a view matrix with 0 translation.

  vec4 clipPos = push.viewProjection * vec4(pos, 1.0);

  // Force z to 1.0 (far plane) for maximum depth
  gl_Position = clipPos.xyww;
}
