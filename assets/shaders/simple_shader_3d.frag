#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

// push constants are a way to pass a small amount of data to shaders
layout(push_constant) uniform push_t
{
    mat4 transform;
    vec3 color;
}
push;

// RGBA Red
void main()
{
    outColor = vec4(fragColor, 1.0);
}