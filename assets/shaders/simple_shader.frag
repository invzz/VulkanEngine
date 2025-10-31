#version 450

layout(location = 0) out vec4 outColor;

// push constants are a way to pass a small amount of data to shaders
layout(push_constant) uniform PushConstants
{
    vec2 offset;
    vec3 color;
}
pushConstants;

// RGBA Red
void main()
{
    outColor = vec4(pushConstants.color, 1.0);
}