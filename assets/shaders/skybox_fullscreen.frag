#version 450

layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube skyboxSampler;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 debugParams; // x = debugCubemapFaces (1/0)
}
push;

// Determine which cubemap face a direction hits (same ordering as Vulkan cubemap array layers):
// { +X, -X, +Y, -Y, +Z, -Z }
int cubemapFaceIndex(vec3 d) {
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return (d.x >= 0.0) ? 0 : 1;
    if (a.y >= a.x && a.y >= a.z) return (d.y >= 0.0) ? 2 : 3;
    return (d.z >= 0.0) ? 4 : 5;
}

// Returns per-face UV in [-1, 1] for debug visualization.
vec2 cubemapFaceUV(vec3 d, int face) {
    vec3 a = abs(d);
    if (face == 0) return vec2(-d.z, -d.y) / max(a.x, 1e-8); // +X
    if (face == 1) return vec2(d.z, -d.y) / max(a.x, 1e-8); // -X
    if (face == 2) return vec2(d.x,  d.z) / max(a.y, 1e-8); // +Y
    if (face == 3) return vec2(d.x, -d.z) / max(a.y, 1e-8); // -Y
    if (face == 4) return vec2(d.x, -d.y) / max(a.z, 1e-8); // +Z
    return             vec2(-d.x, -d.y) / max(a.z, 1e-8);     // -Z
}

vec3 faceDebugColor(int face) {
    // Distinct, easy-to-spot colors.
    if (face == 0) return vec3(1.0, 0.2, 0.2); // +X
    if (face == 1) return vec3(0.2, 1.0, 0.2); // -X
    if (face == 2) return vec3(0.2, 0.4, 1.0); // +Y
    if (face == 3) return vec3(1.0, 1.0, 0.2); // -Y
    if (face == 4) return vec3(1.0, 0.2, 1.0); // +Z
    return               vec3(0.2, 1.0, 1.0);   // -Z
}

float gridLine(float t) {
    // Thin line every 1/8th.
    float f = abs(fract(t * 8.0) - 0.5);
    return 1.0 - smoothstep(0.48, 0.5, f);
}

void main() {
    // Unproject a point on the far plane for this pixel.
    // viewProjection is expected to have translation removed.
    mat4 invVP = inverse(push.viewProjection);

    vec4 world = invVP * vec4(vNdc, 1.0, 1.0);
    vec3 dir = (abs(world.w) > 1e-6) ? (world.xyz / world.w) : world.xyz;
    dir = normalize(dir);

    // Flip Y axis to match the renderer's cubemap sampling convention.
    vec3 sampleDir = vec3(dir.x, -dir.y, dir.z);

    if (push.debugParams.x > 0.5) {
        int face = cubemapFaceIndex(sampleDir);
        vec2 uv = cubemapFaceUV(sampleDir, face);
        vec2 uv01 = uv * 0.5 + 0.5;

        vec3 base = faceDebugColor(face);

        // Show orientation within the face.
        vec3 grad = vec3(uv01, 0.6);
        vec3 color = base * (0.35 + 0.65 * grad);

        // Add a simple UV grid + center cross.
        float g = max(gridLine(uv01.x), gridLine(uv01.y));
        float cx = 1.0 - smoothstep(0.0, 0.01, abs(uv01.x - 0.5));
        float cy = 1.0 - smoothstep(0.0, 0.01, abs(uv01.y - 0.5));
        float overlay = clamp(g * 0.35 + max(cx, cy) * 0.35, 0.0, 0.7);
        color = mix(color, vec3(0.0), overlay);

        outColor = vec4(color, 1.0);
        return;
    }

    vec3 color = texture(skyboxSampler, sampleDir).rgb;
    outColor = vec4(color, 1.0);
}
