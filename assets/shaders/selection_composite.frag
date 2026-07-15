#version 450
// Screen-space selection outline composite.
//
// Samples the tonemapped post-fx color and the selection mask, then draws a
// Blender-style solid orange rim where the mask boundary is (a pixel is on the
// edge when it or any 4-neighbour differs in mask coverage). The rim is blended
// ON TOP of the scene color so it always sits above all geometry — it is not
// tonemapped or depth-occluded.

layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(set = 0, binding = 1) uniform sampler2D selectionMask;

layout(location = 0) out vec4 outColor;

void main() {
    vec2  uv      = vec2(gl_FragCoord.xy);
    vec2  texel   = 1.0 / textureSize(sceneColor, 0);
    vec3  baseCol = texture(sceneColor, uv * texel).rgb;

    float c = texture(selectionMask, uv * texel).r;
    float l = texture(selectionMask, (uv + vec2(-1.0, 0.0)) * texel).r;
    float r = texture(selectionMask, (uv + vec2( 1.0, 0.0)) * texel).r;
    float d = texture(selectionMask, (uv + vec2( 0.0, 1.0)) * texel).r;
    float u = texture(selectionMask, (uv + vec2( 0.0,-1.0)) * texel).r;

    // Edge: current coverage differs from any neighbour -> boundary pixel.
    float edge = abs(c - l) + abs(c - r) + abs(c - d) + abs(c - u);
    edge = clamp(edge, 0.0, 1.0);

    // Blender selection orange. alpha stays 1.0 (opaque overlay pixel).
    vec3  outlineColor = vec3(1.0, 0.55, 0.0);
    vec3  finalCol     = mix(baseCol, outlineColor, edge);

    outColor = vec4(finalCol, 1.0);
}
