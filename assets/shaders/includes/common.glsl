#ifndef COMMON_GLSL
#define COMMON_GLSL

// Small shared helpers used across multiple shaders.

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec3 saturate(vec3 v) {
    return clamp(v, vec3(0.0), vec3(1.0));
}

// Builds an orthonormal basis (T, B) around a unit normal N.
void buildOrthonormalBasis(in vec3 N, out vec3 T, out vec3 B) {
    // Choose an "up" vector that is not parallel to N.
    vec3 up = (abs(N.y) > 0.99) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);

    T = normalize(cross(up, N));
    B = cross(N, T);
}

#endif // COMMON_GLSL
