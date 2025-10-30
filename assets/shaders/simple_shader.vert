#version 450

// vec2 is a built-in type representing a 2D vector
vec2 position[3] = vec2[](vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.0, 0.5));

void main()
{
    // gl_Position is a built-in output variable that holds the position of the
    // vertex. Here we set it to one of the positions defined above.
    // the gl_VertexIndex is a built-in variable that holds the index of the
    // vertex being processed.

    // the vec4 constructor takes four arguments: x, y, z, w
    // we set z to 0.0 and w to 1.0 for 2D rendering
    // final position is in clip space
    gl_Position = vec4(position[gl_VertexIndex], 0.0, 1.0);
}