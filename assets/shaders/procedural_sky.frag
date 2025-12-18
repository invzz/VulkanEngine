#version 450

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants
{
  mat4  viewProjection;
  vec4  sunDirection;    // xyz = direction, w = intensity
  vec4  sunColor;        // xyz = color
  float rayleigh;        // Rayleigh scattering coefficient
  float mie;             // Mie scattering coefficient
  float mieEccentricity; // Phase function param
  float padding;
}
push;

// Constants for atmosphere
const float PI = 3.14159265359;
const float RE = 6360e3; // Earth radius
const float RA = 6420e3; // Atmosphere radius
const float HR = 8e3;    // Rayleigh scale height
const float HM = 1.2e3;  // Mie scale height

// Intersect ray with sphere
vec2 intersectSphere(vec3 ro, vec3 rd, float rad)
{
  float b   = dot(ro, rd);
  float c   = dot(ro, ro) - rad * rad;
  float det = b * b - c;
  if (det < 0.0) return vec2(-1.0);
  det = sqrt(det);
  return vec2(-b - det, -b + det);
}

// Rayleigh phase function
float phaseRayleigh(float mu)
{
  return 3.0 * (1.0 + mu * mu) / (16.0 * PI);
}

// Mie phase function
float phaseMie(float mu, float g)
{
  float g2 = g * g;
  return 3.0 * (1.0 - g2) * (1.0 + mu * mu) / (8.0 * PI * (2.0 + g2) * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
}

// --- Stars & Moon ---

// Hash function for noise
float hash(vec3 p)
{
  p = fract(p * 0.3183099 + 0.1);
  p *= 17.0;
  return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3D Noise
float noise(vec3 x)
{
  vec3 i = floor(x);
  vec3 f = fract(x);
  f      = f * f * (3.0 - 2.0 * f);

  return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x), mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
             mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x), mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y),
             f.z);
}

// Star field
vec3 getStars(vec3 viewDir)
{
  // Frequency controls star density/size
  float n = noise(viewDir * 150.0);
  // Threshold to create sparse stars
  float stars = smoothstep(0.95, 1.0, n);
  // Twinkle effect (optional, requires time)
  return vec3(stars);
}

// Moon
vec3 getMoon(vec3 viewDir, vec3 sunDir)
{
  vec3  moonDir    = -sunDir; // Moon opposite to sun
  float moonRadius = 0.03;    // Angular radius (approx)
  float dist       = dot(viewDir, moonDir);

  if (dist > (1.0 - moonRadius))
  {
    // Moon disk
    float moonMask = smoothstep(1.0 - moonRadius, 1.0 - moonRadius + 0.002, dist);

    // Simple crater noise
    float craters   = noise(viewDir * 50.0);
    vec3  moonColor = vec3(0.8) * (0.8 + 0.2 * craters);

    // Phase (lit by sun)
    // Since moon is opposite to sun, it should be full moon.
    // But to make it interesting, let's offset the light slightly or just render full moon for now.
    // Realistically, if moon is opposite to sun, it IS full moon.

    // Let's add a glow
    float glow = exp(-100.0 * (1.0 - dist));

    return moonColor * moonMask + vec3(glow * 0.2);
  }
  return vec3(0.0);
}

void main()
{
  // Flip Y to match the coordinate system used by the skybox mesh/projection
  vec3 rd = normalize(vec3(fragTexCoord.x, -fragTexCoord.y, fragTexCoord.z));
  vec3 ro = vec3(0.0, RE + 1.0, 0.0); // Observer on ground

  vec3  sunDir       = normalize(push.sunDirection.xyz);
  float sunIntensity = push.sunDirection.w;

  // Intersect atmosphere
  vec2 t = intersectSphere(ro, rd, RA);
  if (t.y < 0.0)
  {
    outColor = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }

  float t0 = 0.0;
  float t1 = t.y;

  // Optical depth accumulation
  vec2 opticalDepth  = vec2(0.0);
  vec3 totalRayleigh = vec3(0.0);
  vec3 totalMie      = vec3(0.0);

  // Raymarching
  const int STEPS    = 16;
  float     dt       = (t1 - t0) / float(STEPS);
  float     tCurrent = 0.0;

  for (int i = 0; i < STEPS; ++i)
  {
    vec3  p = ro + rd * (tCurrent + dt * 0.5);
    float h = length(p) - RE;

    if (h < 0.0) break;

    vec2 density = vec2(exp(-h / HR), exp(-h / HM));
    opticalDepth += density * dt;

    // Light path to sun
    vec2  tSun            = intersectSphere(p, sunDir, RA);
    float dtSun           = tSun.y / 8.0;
    vec2  sunOpticalDepth = vec2(0.0);
    float tSunCurrent     = 0.0;

    for (int j = 0; j < 8; ++j)
    {
      vec3  pSun = p + sunDir * (tSunCurrent + dtSun * 0.5);
      float hSun = length(pSun) - RE;
      sunOpticalDepth += vec2(exp(-hSun / HR), exp(-hSun / HM)) * dtSun;
      tSunCurrent += dtSun;
    }

    vec3 attenuation = exp(-(push.rayleigh * 1e-5 * vec3(5.8, 13.5, 33.1) * (opticalDepth.x + sunOpticalDepth.x) +
                             push.mie * 1e-5 * 21.0 * (opticalDepth.y + sunOpticalDepth.y)));

    totalRayleigh += density.x * attenuation * dt;
    totalMie += density.y * attenuation * dt;

    tCurrent += dt;
  }

  float mu            = dot(rd, sunDir);
  vec3  rayleighColor = totalRayleigh * phaseRayleigh(mu) * push.rayleigh * 1e-5 * vec3(5.8, 13.5, 33.1);
  vec3  mieColor      = totalMie * phaseMie(mu, push.mieEccentricity) * push.mie * 1e-5 * 21.0;

  vec3 color = (rayleighColor + mieColor) * sunIntensity * push.sunColor.xyz;

  // Sun Disk
  // Calculate transmittance along the view ray (using the accumulated optical depth)
  vec3 extinction    = push.rayleigh * 1e-5 * vec3(5.8, 13.5, 33.1) * opticalDepth.x + push.mie * 1e-5 * 21.0 * opticalDepth.y;
  vec3 transmittance = exp(-extinction);

  // --- Night Sky (Stars + Moon) ---
  // Only visible when transmittance is high (atmosphere is transparent/dark)
  // and sun is down (intensity low or direction below horizon)

  // Calculate background (space)
  vec3 nightSky = vec3(0.0);

  // Stars
  nightSky += getStars(rd);

  // Moon
  nightSky += getMoon(rd, sunDir);

  // Apply atmospheric extinction to the background objects
  // This ensures they fade out near the horizon or during the day
  color += nightSky * transmittance;

  // Draw sun disk if looking at the sun
  // 0.999 corresponds to roughly 2.5 degrees, which is a bit large but good for games
  if (mu > 0.999)
  {
    float sunMask = smoothstep(0.999, 0.9995, mu);
    // Add sun radiance: SunColor * Intensity * Transmittance
    // We multiply by a factor to make it super bright
    color += push.sunColor.xyz * sunIntensity * transmittance * sunMask * 20.0;
  }

  // Tone mapping (simple Reinhard)
  color = color / (color + vec3(1.0));
  color = pow(color, vec3(1.0 / 2.2)); // Gamma correction

  outColor = vec4(color, 1.0);
}
