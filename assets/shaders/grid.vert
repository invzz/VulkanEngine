#version 450

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UBO
{
  mat4 proj;
  mat4 view;
  // ... rest of UBO (we only need proj and view)
}
ubo;

// Procedural ground grid on the XZ plane (Y=0).
// No vertex buffers: positions are derived from gl_VertexIndex.

const int   GRID_EXTENT = 20;  // number of cells from origin to edge
const float GRID_STEP   = 1.0; // world units per cell

void main()
{
  int linesPerAxis = GRID_EXTENT * 2 + 1; // e.g. 41
  int totalLines   = linesPerAxis * 2;    // X-lines + Z-lines
  int totalVerts   = totalLines * 2;

  int v = gl_VertexIndex;
  if (v >= totalVerts)
  {
    gl_Position = vec4(0.0);
    fragColor   = vec3(0.0);
    return;
  }

  int lineIndex = v / 2; // which line
  int endIndex  = v % 2; // 0 or 1

  float halfSize = float(GRID_EXTENT) * GRID_STEP;

  float x;
  float z;

  if (lineIndex < linesPerAxis)
  {
    // Lines parallel to X, varying Z
    float t = float(lineIndex - GRID_EXTENT) * GRID_STEP;
    x       = (endIndex == 0) ? -halfSize : halfSize;
    z       = t;
  }
  else
  {
    // Lines parallel to Z, varying X
    int   local = lineIndex - linesPerAxis;
    float t     = float(local - GRID_EXTENT) * GRID_STEP;
    x           = t;
    z           = (endIndex == 0) ? -halfSize : halfSize;
  }

  vec4 worldPos = vec4(x, 0.0, z, 1.0);
  gl_Position   = ubo.proj * ubo.view * worldPos;

  fragColor = vec3(0.35, 0.35, 0.35);
}
