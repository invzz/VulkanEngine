#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out float outAlpha;
layout(location = 1) out vec3 outColor;

layout(push_constant) uniform PushConstants
{
  mat4 viewProjection;
  vec4 cameraPosition; // w = boxSize
  vec4 params;         // x = time, y = size, z = alpha, w = heightFalloff
  vec4 sunDirection;   // xyz = direction, w = unused
  vec4 sunColor;       // xyz = color, w = unused
  vec4 ambientColor;   // xyz = color, w = unused
}
push;

void main()
{
  vec3  camPos        = push.cameraPosition.xyz;
  float boxSize       = push.cameraPosition.w;
  float time          = push.params.x;
  float baseSize      = push.params.y;
  float baseAlpha     = push.params.z;
  float heightFalloff = push.params.w;

  // Add some drift based on position and time
  vec3 drift = vec3(sin(time * 0.2 + inPosition.y * 5.0),
                    cos(time * 0.1 + inPosition.x * 5.0) * 0.5 + 0.2, // Slight upward drift
                    sin(time * 0.15 + inPosition.z * 5.0)) *
               0.5;

  // Calculate wrapped position relative to camera
  // We want the particles to exist in a volume around the camera
  // The 'inPosition' is a random seed in [0, boxSize]

  vec3 worldSeed = inPosition + drift;

  // Wrap logic:
  // We want the particle to be within [camPos - boxSize/2, camPos + boxSize/2]
  vec3 relativePos = mod(worldSeed - camPos, boxSize);
  // mod returns [0, boxSize]. Shift to [-boxSize/2, boxSize/2]
  relativePos -= boxSize * 0.5;

  vec3 worldPos = camPos + relativePos;

  gl_Position = push.viewProjection * vec4(worldPos, 1.0);

  // Point size attenuation
  float dist = length(relativePos);
  // Simple linear attenuation
  gl_PointSize = baseSize * (1.0 / (1.0 + dist * 0.1));
  // Clamp size
  gl_PointSize = clamp(gl_PointSize, 1.0, 10.0);

  // Fade out at edges of box to avoid popping
  float edgeDist = max(max(abs(relativePos.x), abs(relativePos.y)), abs(relativePos.z));
  float edgeFade = 1.0 - smoothstep(boxSize * 0.4, boxSize * 0.5, edgeDist);

  // Fade out if too close to camera (clipping)
  float closeFade = smoothstep(0.5, 2.0, dist);

  // Height-based density
  float heightFade = exp(-worldPos.y * heightFalloff);

  // Lighting interaction (simple phase function approximation)
  // Dust scatters light forward more than backward (Mie scattering)
  // But for simple dust motes, we can just make them brighter when looking towards the sun
  // or just generally lit by the sun.
  // Let's assume they are lit by the sun.
  // We don't have shadows here, so we assume they are lit.
  // We can add a simple phase function:
  vec3  viewDir  = normalize(worldPos - camPos);
  vec3  sunDir   = normalize(push.sunDirection.xyz);
  float cosTheta = dot(viewDir, sunDir);
  // Henyey-Greenstein phase function approximation or just simple forward scattering boost
  float scattering = 1.0 + max(0.0, cosTheta) * 2.0;

  vec3 lighting = push.sunColor.rgb * scattering + push.ambientColor.rgb;

  outColor = lighting;
  outAlpha = baseAlpha * edgeFade * closeFade * heightFade;
}
