#version 450

// vec2 is a built-in type representing a 2D vector
// the in keyword indicates that this variable is an input to the vertex shader
layout(location = 0) in vec3 position;

// color will be passed from the vertex shader to the fragment shader
layout(location = 1) in vec3 color;

// fragColor is an output variable that will be passed to the fragment shader
layout(location = 0) out vec3 fragColor;

// push constants are a way to pass a small amount of data to shaders
layout(push_constant) uniform push_t
{
    mat4 transform;
    vec3 color;
}
push;

void main()
{
    gl_Position = push.transform * vec4(position, 1.0);
    fragColor   = color;
}