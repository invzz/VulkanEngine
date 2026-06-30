#version 450

layout(push_constant) uniform PushConstants {
    vec3 aabbMin;
    vec3 aabbMax;
    vec3 color;
} pc;

layout(location = 0) out vec3 vColor;

void main() {
    int e = gl_VertexIndex / 2;
    int v = gl_VertexIndex % 2;
    vec3 p;
    
    if (e == 0) p = mix(vec3(pc.aabbMin.x, pc.aabbMin.y, pc.aabbMin.z), vec3(pc.aabbMax.x, pc.aabbMin.y, pc.aabbMin.z), float(v));
    else if (e == 1) p = mix(vec3(pc.aabbMin.x, pc.aabbMax.y, pc.aabbMin.z), vec3(pc.aabbMax.x, pc.aabbMax.y, pc.aabbMin.z), float(v));
    else if (e == 2) p = mix(vec3(pc.aabbMin.x, pc.aabbMin.y, pc.aabbMax.z), vec3(pc.aabbMax.x, pc.aabbMax.y, pc.aabbMax.z), float(v));
    else if (e == 3) p = mix(vec3(pc.aabbMin.x, pc.aabbMin.y, pc.aabbMin.z), vec3(pc.aabbMin.x, pc.aabbMax.y, pc.aabbMin.z), float(v));
    else if (e == 4) p = mix(vec3(pc.aabbMax.x, pc.aabbMin.y, pc.aabbMin.z), vec3(pc.aabbMax.x, pc.aabbMax.y, pc.aabbMin.z), float(v));
    else p = mix(vec3(pc.aabbMin.x, pc.aabbMin.y, pc.aabbMax.z), vec3(pc.aabbMin.x, pc.aabbMax.y, pc.aabbMax.z), float(v));
    
    gl_Position = vec4(p, 1.0);
    vColor = pc.color;
}
