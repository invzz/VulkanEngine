#version 450

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sceneColor;

layout(push_constant) uniform PushConstants
{
  float exposure;
  float contrast;
  float saturation;
  float vignette;
  float bloomIntensity;
  float bloomThreshold;
  int   enableFXAA;
  int   enableBloom;
  float fxaaSpanMax;
  float fxaaReduceMul;
  float fxaaReduceMin;
  int   toneMappingMode; // 0: None, 1: ACES
}
push;

// FXAA implementation
// #define FXAA_SPAN_MAX   8.0
// #define FXAA_REDUCE_MUL (1.0 / 8.0)
// #define FXAA_REDUCE_MIN (1.0 / 128.0)

vec3 applyFXAA(vec2 texCoords)
{
  vec2 texSize   = textureSize(sceneColor, 0);
  vec2 inverseVP = 1.0 / texSize;

  vec3 rgbNW = texture(sceneColor, texCoords + (vec2(-1.0, -1.0) * inverseVP)).xyz;
  vec3 rgbNE = texture(sceneColor, texCoords + (vec2(1.0, -1.0) * inverseVP)).xyz;
  vec3 rgbSW = texture(sceneColor, texCoords + (vec2(-1.0, 1.0) * inverseVP)).xyz;
  vec3 rgbSE = texture(sceneColor, texCoords + (vec2(1.0, 1.0) * inverseVP)).xyz;
  vec3 rgbM  = texture(sceneColor, texCoords).xyz;

  vec3  luma   = vec3(0.299, 0.587, 0.114);
  float lumaNW = dot(rgbNW, luma);
  float lumaNE = dot(rgbNE, luma);
  float lumaSW = dot(rgbSW, luma);
  float lumaSE = dot(rgbSE, luma);
  float lumaM  = dot(rgbM, luma);

  float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
  float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

  vec2 dir;
  dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
  dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

  float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * push.fxaaReduceMul), push.fxaaReduceMin);

  float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

  dir = min(vec2(push.fxaaSpanMax, push.fxaaSpanMax), max(vec2(-push.fxaaSpanMax, -push.fxaaSpanMax), dir * rcpDirMin)) * inverseVP;

  vec3 rgbA = (1.0 / 2.0) * (texture(sceneColor, texCoords.xy + dir * (1.0 / 3.0 - 0.5)).xyz + texture(sceneColor, texCoords.xy + dir * (2.0 / 3.0 - 0.5)).xyz);

  vec3 rgbB = rgbA * (1.0 / 2.0) +
              (1.0 / 4.0) * (texture(sceneColor, texCoords.xy + dir * (0.0 / 3.0 - 0.5)).xyz + texture(sceneColor, texCoords.xy + dir * (3.0 / 3.0 - 0.5)).xyz);

  float lumaB = dot(rgbB, luma);

  if ((lumaB < lumaMin) || (lumaB > lumaMax)) return rgbA;
  return rgbB;
}

vec3 applyBloom(vec3 color, vec2 uv)
{
  vec3 bloomColor = vec3(0.0);

  // Sample lower mips
  // Weights can be adjusted
  bloomColor += textureLod(sceneColor, uv, 1.0).rgb * 1.0;
  bloomColor += textureLod(sceneColor, uv, 2.0).rgb * 0.8;
  bloomColor += textureLod(sceneColor, uv, 3.0).rgb * 0.6;
  bloomColor += textureLod(sceneColor, uv, 4.0).rgb * 0.4;
  bloomColor += textureLod(sceneColor, uv, 5.0).rgb * 0.2;

  bloomColor = max(bloomColor - vec3(push.bloomThreshold), vec3(0.0));

  return color + bloomColor * push.bloomIntensity;
}

void main()
{
  vec3 color;

  if (push.enableFXAA == 1)
  {
    color = applyFXAA(inUV);
  }
  else
  {
    color = texture(sceneColor, inUV).rgb;
  }

  // Bloom
  if (push.enableBloom == 1 && push.bloomIntensity > 0.0)
  {
    color = applyBloom(color, inUV);
  }

  // Exposure
  color *= push.exposure;

  // Contrast
  color = (color - 0.5) * push.contrast + 0.5;
  color = max(color, 0.0); // Prevent negative colors

  // Saturation
  float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
  color           = mix(vec3(luminance), color, push.saturation);

  vec3 mapped = color;

  if (push.toneMappingMode == 1) // ACES Filmic
  {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    mapped  = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
  }

  // Gamma correction
  const float gamma = 2.2;
  mapped            = pow(mapped, vec3(1.0 / gamma));

  // Vignette
  float dist = length(inUV - 0.5);
  float vig  = 1.0 - smoothstep(0.4, 1.5, dist * push.vignette);
  mapped *= vig;

  outColor = vec4(mapped, 1.0);
}