#version 450

// vec2 is a built-in type representing a 2D vector
// the in keyword indicates that this variable is an input to the vertex shader
layout(location = 0) in vec2 position;

// color will be passed from the vertex shader to the fragment shader
layout(location = 1) in vec3 color;

// fragColor is an output variable that will be passed to the fragment shader
layout(location = 0) out vec3 fragColor;

void main()
{
    // gl_Position is a built-in output variable that holds the position of the
    // vertex. Here we set it to one of the positions defined above.

    // the gl_VertexIndex is a built-in variable that holds the index of the
    // vertex being processed.

    // the vec4 constructor takes four arguments: x, y, z, w

    // we set z to 0.0 and w to 1.0 for 2D rendering
    // final position is in clip space

    // gl_VertexIndex ranges from 0 to 2 for the three vertices of the triangle

    // we dont need gl_VertexIndex because we are using the position attribute directly
    gl_Position = vec4(position, 0.0, 1.0);
    fragColor   = color;
}