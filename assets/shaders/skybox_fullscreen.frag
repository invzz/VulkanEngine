#version 450

layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube skyboxSampler;
layout(set = 0, binding = 1) uniform sampler2D skyLUTSampler;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 debugParams;   // x = debugCubemapFaces, y = proceduralSky, z = useSkyLUT, w = captureToCubemap
    vec4 sunDirection;  // xyz = direction to sun, w = unused
    vec4 sunColor;      // rgb = sun color, w = sun angular radius (radians, default 0.015)
    vec4 skyParams;     // x = timeOfDay (0-24), y = intensity, zw = unused
    int  faceIndex;     // cube face when debugParams.w > 0.5 (capture mode)
} push;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
int cubemapFaceIndex(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z)
        return (d.x >= 0.0) ? 0 : 1;
    if (a.y >= a.x && a.y >= a.z)
        return (d.y >= 0.0) ? 2 : 3;
    return (d.z >= 0.0) ? 4 : 5;
}
vec2 cubemapFaceUV(vec3 d, int face) {
    vec3 a = abs(d);
    if (face == 0)
        return vec2(-d.z, -d.y) / max(a.x, 1e-8);
    if (face == 1)
        return vec2(d.z, -d.y) / max(a.x, 1e-8);
    if (face == 2)
        return vec2(d.x, d.z) / max(a.y, 1e-8);
    if (face == 3)
        return vec2(d.x, -d.z) / max(a.y, 1e-8);
    if (face == 4)
        return vec2(d.x, -d.y) / max(a.z, 1e-8);
    return vec2(-d.x, -d.y) / max(a.z, 1e-8);
}
vec3 faceDebugColor(int face) {
    if (face == 0)
        return vec3(1.0, 0.2, 0.2);
    if (face == 1)
        return vec3(0.2, 1.0, 0.2);
    if (face == 2)
        return vec3(0.2, 0.4, 1.0);
    if (face == 3)
        return vec3(1.0, 1.0, 0.2);
    if (face == 4)
        return vec3(1.0, 0.2, 1.0);
    return vec3(0.2, 1.0, 1.0);
}
float gridLine(float t) {
    float f = abs(fract(t * 8.0) - 0.5);
    return 1.0 - smoothstep(0.48, 0.5, f);
}

// Direction (Y-up world) for a point on cube face `face` at UV in [-1,1].
// Mirrors the capture view math in ProceduralSkyCapture (face order +Z,-Z,+Y,-Y,+X,-X).
vec3 cubeDir(int face, vec2 uv) {
    if (face == 0)  return normalize(vec3( uv.x, -uv.y,  1.0));   // +Z
    if (face == 1)  return normalize(vec3( uv.x, -uv.y, -1.0));   // -Z
    if (face == 2)  return normalize(vec3( uv.x,  1.0,  uv.y));   // +Y
    if (face == 3)  return normalize(vec3( uv.x, -1.0, -uv.y));   // -Y
    if (face == 4)  return normalize(vec3( 1.0, -uv.y, -uv.x));   // +X
    return normalize(vec3(-1.0, -uv.y,  uv.x));                 // -X
}

float sunDisc(vec3 dir, vec3 sunDir, float angularRadius);
vec3  nightStars(vec3 dir, float sunElevation);
vec3  sunColorFromElevation(float elevation);

float rayleighPhase(float cosTheta) {
    return (3.0 / (16.0 * 3.14159265)) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta, float g) {
    float numerator   = 1.0 - (g * g);
    float denominator = pow(max(1.0 + (g * g) - (2.0 * g * cosTheta), 1e-4), 1.5);
    return (1.0 / (4.0 * 3.14159265)) * (numerator / denominator);
}

vec3 proceduralSkyFromLUT(vec3 dir, vec3 sunDir) {
    float viewAngle = clamp(dir.y, 0.0, 1.0);
    float sunAngle  = clamp(sunDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec2  lutUV     = vec2(sunAngle, viewAngle);

    vec4  lutData  = texture(skyLUTSampler, lutUV);
    float cosTheta = clamp(dot(dir, sunDir), -1.0, 1.0);

    vec3  rayleighScattering = lutData.rgb;
    float mieInt             = lutData.a;

    // --- Mie chromatic reconstruction ---
    // Mie is physically achromatic: its colour is the DIRECT sun colour
    // (the reddened disc you see at golden hour), NOT the ratio of the
    // scattered blue sky. The LUT's mieInt already carries the sun-reaches
    // point extinction, so we tint with the sun colour HUE only (normalised
    // to peak 1.0) to avoid double-applying extinction. At sunset sunColor
    // is red-dominated, so the halo goes warm; at noon it is ~white.
    vec3 sunCol = push.sunColor.rgb;
    float peak  = max(max(sunCol.r, sunCol.g), sunCol.b);
    vec3 mieColor = (peak > 1e-4) ? (sunCol / peak) : vec3(1.0);

    vec3 finalRayleigh = rayleighScattering * rayleighPhase(cosTheta);
    vec3 finalMie      = vec3(mieInt) * mieColor * miePhase(cosTheta, 0.76);

    vec3 sky = finalRayleigh + finalMie;

    float disc = sunDisc(dir, sunDir, push.sunColor.w > 0.0 ? push.sunColor.w : 0.015);
    sky += disc * sunCol * 2.0;   // disc keeps full chromatic sun colour
    sky += nightStars(dir, sunDir.y);

    sky *= push.skyParams.y > 0.0 ? push.skyParams.y : 1.0;
    return sky;
}

// ---------------------------------------------------------------------------
// Procedural sky (Rayleigh + Mie + Sun disc + Stars)
// ---------------------------------------------------------------------------

// Compute sun elevation and azimuth from time of day (0-24).
// Returns sun direction in world space (Y-up).
vec3 sunDirectionFromTime(float t) {
    // Sun rises at 6, peaks at 12, sets at 18
    float elevation = sin((t - 6.0) / 24.0 * 6.2831853);
    float azimuth   = (t - 6.0) / 24.0 * 6.2831853;
    // Y-up world: azimuth rotates around Y, elevation tilts from XZ plane
    float cosElev = sqrt(max(1.0 - elevation * elevation, 0.0));
    return normalize(vec3(
        cosElev * cos(azimuth),
        elevation,
        cosElev * sin(azimuth)));
}

// Sun color: white at noon, dramatic orange/red at dawn/dusk
vec3 sunColorFromElevation(float elevation) {
    // Use a sharper transition near horizon
    float t = clamp(elevation, 0.0, 1.0);
    // Horizon glow: deep orange-red when sun is at/below horizon
    // Golden hour: warm yellow when sun is 0-15 deg above
    // Noon: white
    vec3 horizon    = vec3(1.0, 0.3, 0.05);   // Deep red-orange at horizon
    vec3 goldenHour = vec3(1.0, 0.7, 0.3);    // Warm yellow (golden hour)
    vec3 noon       = vec3(1.0, 0.98, 0.92);  // White at zenith

    vec3 color;
    if (t < 0.05) {
        // Below horizon: deep red sun (sun is setting/rising)
        color = horizon;
    } else if (t < 0.2) {
        // Golden hour: orange-red -> warm yellow
        color = mix(horizon, goldenHour, (t - 0.05) / 0.15);
    } else {
        // Golden hour -> noon: warm yellow -> white
        color = mix(goldenHour, noon, (t - 0.2) / 0.8);
    }
    return color;
}

// Sky dome color: blue zenith, warm horizon during day, dark blue/purple at night
vec3 rayleighSky(vec3 dir, vec3 sunDir, float sunElevation) {
    float sunAngle = dot(dir, sunDir);
    float rayleigh = 1.0 - abs(sunAngle);

    // Horizon color: depends on sun elevation
    vec3 horizonColor;
    if (sunElevation < -0.05) {
        // Night: deep dark blue/purple
        horizonColor = vec3(0.02, 0.02, 0.06);
    } else if (sunElevation < 0.05) {
        // Twilight: warm orange/pink
        horizonColor = mix(vec3(0.6, 0.3, 0.15), vec3(0.05, 0.05, 0.15), sunElevation * 10.0 + 0.5);
    } else {
        // Day: light blue/white
        horizonColor = mix(vec3(0.85, 0.8, 0.7), vec3(0.9, 0.85, 0.8), sunElevation);
    }

    // Zenith color
    vec3 zenithColor;
    if (sunElevation < -0.05) {
        // Night: dark blue
        zenithColor = vec3(0.01, 0.01, 0.04);
    } else if (sunElevation < 0.1) {
        // Twilight: purple/dark blue
        zenithColor = mix(vec3(0.15, 0.1, 0.3), vec3(0.02, 0.02, 0.08), sunElevation * 10.0 + 0.5);
    } else {
        // Day: blue
        zenithColor = mix(vec3(0.1, 0.25, 0.75), vec3(0.2, 0.35, 0.85), sunElevation);
    }

    // Interpolate horizon to zenith based on height
    float horizonFade = smoothstep(0.0, 0.25, abs(dir.y));
    vec3  skyColor    = mix(horizonColor, zenithColor, horizonFade);

    // Add subtle sun glow in sky (not just the disc)
    float sunGlow = pow(max(dot(dir, sunDir), 0.0), 32.0) * 0.15;
    skyColor += sunGlow * sunColorFromElevation(sunElevation);

    return skyColor;
}

// Mie scattering: bright glow around the sun
float mieGlow(vec3 dir, vec3 sunDir) {
    float cosAngle = dot(dir, sunDir);
    float g        = -0.98;  // forward scattering
    float mie      = (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosAngle, 1.5);
    return mie * 0.04;
}

// Sun disc
float sunDisc(vec3 dir, vec3 sunDir, float angularRadius) {
    float cosAngle = dot(dir, sunDir);
    float angle    = acos(clamp(cosAngle, -1.0, 1.0));
    return 1.0 - smoothstep(angularRadius * 0.7, angularRadius * 1.1, angle);
}

// Night stars: procedural star field
vec3 nightStars(vec3 dir, float sunElevation) {
    if (sunElevation > 0.1)
        return vec3(0.0);  // Not visible during day

    float starField = 0.0;
    float twinkle   = 0.8 + 0.2 * sin(dir.x * 123.456 + dir.y * 789.012);

    // Layer 1: bright stars
    vec3  p     = dir * 200.0;
    float star1 = step(0.997, fract(sin(dot(floor(p), vec3(12.9898, 78.233, 45.164))) * 43758.5453));
    starField += star1 * twinkle;

    // Layer 2: dimmer stars
    vec3  p2    = dir * 500.0;
    float star2 = step(0.9985, fract(sin(dot(floor(p2), vec3(26.841, 18.382, 42.175))) * 13757.357));
    starField += star2 * twinkle * 0.5;

    // Only visible at night (sun well below horizon)
    float visibility = smoothstep(0.1, -0.3, sunElevation);
    return starField * vec3(0.8, 0.85, 1.0) * visibility;
}

vec3 proceduralSky(vec3 dir) {
    // Use push constant sun position if provided, else compute from time
    vec3  sunDir;
    float sunElevation;
    if (dot(push.sunDirection.xyz, push.sunDirection.xyz) > 0.001) {
        sunDir       = normalize(push.sunDirection.xyz);
        sunElevation = sunDir.y;
    } else {
        sunDir       = sunDirectionFromTime(push.skyParams.x);
        sunElevation = sunDir.y;
    }

    // Rayleigh sky base
    vec3 sky = rayleighSky(dir, sunDir, sunElevation);

    // Mie glow near sun
    float mie    = mieGlow(dir, sunDir);
    vec3  sunCol = sunColorFromElevation(sunElevation);
    sky += mie * sunCol * 0.6;

    // Sun disc
    float disc = sunDisc(dir, sunDir, push.sunColor.w > 0.0 ? push.sunColor.w : 0.015);
    sky += disc * sunCol * 2.0;

    // Night stars
    sky += nightStars(dir, sunElevation);

    // Intensity (already has night darkening applied in C++)
    sky *= push.skyParams.y > 0.0 ? push.skyParams.y : 1.0;

    return sky;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() {
    mat4 invVP = inverse(push.viewProjection);
    vec4 world = invVP * vec4(vNdc, 1.0, 1.0);
    vec3 dir   = (abs(world.w) > 1e-6) ? (world.xyz / world.w) : world.xyz;
    dir        = normalize(dir);

    vec3 sampleDir = vec3(dir.x, -dir.y, dir.z);

    // --- Capture to cubemap mode (debugParams.w > 0.5) ---
    // Used to bake the procedural sky into a cubemap for IBL. The dome is
    // authored Y-up, so we sample the cube face direction directly (no Y flip).
    // We use the analytic proceduralSky(dir) rather than the LUT variant because
    // the capture pipeline has no descriptor set bound (no skyLUTSampler), so
    // sampling an unbound LUT descriptor would fault the GPU. The analytic
    // integral is the same sky, computed directly -- correct for a one-time bake.
    if (push.debugParams.w > 0.5) {
        // Fullscreen triangle NDC -> [-1,1] UV.
        vec2  uv   = vec2(vNdc.x, -vNdc.y);
        vec3  dir   = cubeDir(push.faceIndex, uv);
        vec3  color = proceduralSky(dir);
        outColor = vec4(color, 1.0);
        return;
    }

    // Procedural sky mode (push.debugParams.y > 0.5)
    if (push.debugParams.y > 0.5) {
        vec3 color;
        if (push.debugParams.z > 0.5) {
            vec3 sunDir = normalize(push.sunDirection.xyz);
            color       = proceduralSkyFromLUT(sampleDir, sunDir);
        } else {
            color = proceduralSky(sampleDir);
        }
        outColor = vec4(color, 1.0);
        return;
    }

    // Debug cubemap faces mode
    if (push.debugParams.x > 0.5) {
        int   face    = cubemapFaceIndex(sampleDir);
        vec2  uv      = cubemapFaceUV(sampleDir, face);
        vec2  uv01    = uv * 0.5 + 0.5;
        vec3  base    = faceDebugColor(face);
        vec3  grad    = vec3(uv01, 0.6);
        vec3  color   = base * (0.35 + 0.65 * grad);
        float g       = max(gridLine(uv01.x), gridLine(uv01.y));
        float cx      = 1.0 - smoothstep(0.0, 0.01, abs(uv01.x - 0.5));
        float cy      = 1.0 - smoothstep(0.0, 0.01, abs(uv01.y - 0.5));
        float overlay = clamp(g * 0.35 + max(cx, cy) * 0.35, 0.0, 0.7);
        color         = mix(color, vec3(0.0), overlay);
        outColor      = vec4(color, 1.0);
        return;
    }

    // Default: sample cubemap
    vec3 color = texture(skyboxSampler, sampleDir).rgb;
    outColor   = vec4(color, 1.0);
}