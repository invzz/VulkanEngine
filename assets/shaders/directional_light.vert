#version 450

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UBO
{
  mat4 proj;
  mat4 view;
  vec4 ambientLightColor;
  vec4 cameraPosition;
} ubo;

layout(push_constant) uniform push_t
{
  mat4 modelMatrix;
  vec4 color;
} push;

const float PI = 3.14159265359;
const int   SEG = 12;
const float R_DISC   = 0.25;
const float R_INNER  = 0.20;
const float SPOKE_LEN = 0.35;
const float RAY_OFF  = 0.003; // small offset to avoid z-fighting

// Total vertices:
//   Disc outline        : SEG * 2         = 24
//   Inner ring          : SEG * 2         = 24
//   Spokes (from inner to outer) : SEG * 2 = 24
//   Direction arrows (4 spokes * 2)       =  8
//   Total = 80

const int DISC_END   = SEG * 2;          // 24
const int RING_END   = DISC_END + SEG * 2; // 48
const int SPOKE_END  = RING_END + SEG * 2; // 72
const int ARROW_END  = SPOKE_END + 8;      // 80
const int TOTAL      = ARROW_END;          // 80

vec3 getArrowVertex(int idx)
{
  int   seg;
  float a, a2, x, y, x2, y2;

  // --- Disc outer ring (line strip) ---
  if (idx < DISC_END)
  {
    seg = idx / 2;
    a   = float(seg) * 2.0 * PI / float(SEG);
    x   = R_DISC * cos(a);
    y   = R_DISC * sin(a);
    if ((idx % 2) == 0) return vec3(x, y, 0.0);
    a2  = float(seg + 1) * 2.0 * PI / float(SEG);
    x2  = R_DISC * cos(a2);
    y2  = R_DISC * sin(a2);
    return vec3(x2, y2, 0.0);
  }

  // --- Inner ring ---
  if (idx < RING_END)
  {
    int lidx = idx - DISC_END;
    seg      = lidx / 2;
    a        = float(seg) * 2.0 * PI / float(SEG);
    x        = R_INNER * cos(a);
    y        = R_INNER * sin(a);
    if ((lidx % 2) == 0) return vec3(x, y, 0.0);
    a2       = float(seg + 1) * 2.0 * PI / float(SEG);
    x2       = R_INNER * cos(a2);
    y2       = R_INNER * sin(a2);
    return vec3(x2, y2, 0.0);
  }

  // --- Spokes (inner ring to outer disc) ---
  if (idx < SPOKE_END)
  {
    int lidx = idx - RING_END;
    seg      = lidx / 2;
    a        = float(seg) * 2.0 * PI / float(SEG);
    a2       = a + 2.0 * PI / float(SEG) * 0.5;
    if ((lidx % 2) == 0)
    {
      // from inner ring
      return vec3(R_INNER * cos(a), R_INNER * sin(a), 0.0);
    }
    // to outer ring (slightly outward for a subtle ray effect)
    return vec3(R_DISC * cos(a2), R_DISC * sin(a2), 0.0);
  }

  // --- Small direction lines along +Z (4 spokes at 0, 90, 180, 270) ---
  {
    int   lidx  = idx - SPOKE_END;
    int   arm   = lidx / 2;
    float angle = float(arm) * PI / 2.0;
    float dx    = cos(angle) * 0.1;
    float dy    = sin(angle) * 0.1;
    // from disc center outward in Z
    float z_out = SPOKE_LEN;
    float z_in  = 0.0;
    // slightly offset from center to avoid z-fighting with disc lines
    if ((lidx % 2) == 0) return vec3(dx, dy, z_in);
    return vec3(dx, dy, z_out);
  }

  return vec3(0.0);
}

void main()
{
  vec3 vertexPos = getArrowVertex(gl_VertexIndex);
  vec4 worldPos  = push.modelMatrix * vec4(vertexPos, 1.0);
  gl_Position    = ubo.proj * ubo.view * worldPos;
  fragColor      = push.color.rgb;
}