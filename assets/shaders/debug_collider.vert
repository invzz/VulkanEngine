#version 450

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 proj;
    mat4 view;
}
ubo;

layout(push_constant) uniform push_t {
    mat4 modelMatrix;
    vec4 color;
    vec4 shapeParams;  // xyz: halfExtents for box, x: radius for sphere/capsule, y: halfHeight for capsule
    int  shapeType;    // 0 = box, 1 = sphere, 2 = capsule
}
push;

const int   SHAPE_BOX       = 0;
const int   SHAPE_SPHERE    = 1;
const int   SHAPE_CAPSULE   = 2;
const int   CIRCLE_SEGMENTS = 24;
const float PI              = 3.14159265359;

vec3 boxVertex(int index) {
    int lineIndices[24] = int[](0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7);

    int  cornerIndex = lineIndices[index];
    vec3 h           = push.shapeParams.xyz;

    float sx = (cornerIndex == 0 || cornerIndex == 1 || cornerIndex == 4 || cornerIndex == 5) ? 1.0 : -1.0;
    float sy = (cornerIndex == 0 || cornerIndex == 3 || cornerIndex == 4 || cornerIndex == 7) ? 1.0 : -1.0;
    float sz = (cornerIndex < 4) ? -1.0 : 1.0;

    return vec3(h.x * sx, h.y * sy, h.z * sz);
}

vec3 sphereVertex(int index) {
    int vertsPerCircle = CIRCLE_SEGMENTS * 2;
    int circle         = index / vertsPerCircle;
    int local          = index % vertsPerCircle;
    int segment        = local / 2;
    int endpoint       = local % 2;
    int vertexIndex    = (segment + endpoint) % CIRCLE_SEGMENTS;

    float angle = (2.0 * PI * float(vertexIndex)) / float(CIRCLE_SEGMENTS);
    float c     = cos(angle);
    float s     = sin(angle);
    float r     = push.shapeParams.x;

    if (circle == 0) {
        return vec3(r * c, r * s, 0.0);
    }
    if (circle == 1) {
        return vec3(r * c, 0.0, r * s);
    }
    return vec3(0.0, r * c, r * s);
}

vec3 capsuleVertex(int index) {
    int   circleVerts = CIRCLE_SEGMENTS * 2;
    float r           = push.shapeParams.x;
    float halfHeight  = max(push.shapeParams.y, r);

    if (index < circleVerts) {
        int   local       = index;
        int   segment     = local / 2;
        int   endpoint    = local % 2;
        int   vertexIndex = (segment + endpoint) % CIRCLE_SEGMENTS;
        float angle       = (2.0 * PI * float(vertexIndex)) / float(CIRCLE_SEGMENTS);
        return vec3(r * cos(angle), halfHeight, r * sin(angle));
    }

    if (index < (circleVerts * 2)) {
        int   local       = index - circleVerts;
        int   segment     = local / 2;
        int   endpoint    = local % 2;
        int   vertexIndex = (segment + endpoint) % CIRCLE_SEGMENTS;
        float angle       = (2.0 * PI * float(vertexIndex)) / float(CIRCLE_SEGMENTS);
        return vec3(r * cos(angle), -halfHeight, r * sin(angle));
    }

    int lineIndex = index - (circleVerts * 2);
    int pair      = lineIndex / 2;
    int endpoint  = lineIndex % 2;

    vec3 anchors[4] = vec3[](vec3(r, 0.0, 0.0), vec3(-r, 0.0, 0.0), vec3(0.0, 0.0, r), vec3(0.0, 0.0, -r));
    vec3 a          = anchors[pair];
    return vec3(a.x, endpoint == 0 ? halfHeight : -halfHeight, a.z);
}

void main() {
    vec3 localPos;
    if (push.shapeType == SHAPE_SPHERE) {
        localPos = sphereVertex(gl_VertexIndex);
    } else if (push.shapeType == SHAPE_CAPSULE) {
        localPos = capsuleVertex(gl_VertexIndex);
    } else {
        localPos = boxVertex(gl_VertexIndex);
    }

    vec4 worldPos = push.modelMatrix * vec4(localPos, 1.0);
    gl_Position   = ubo.proj * ubo.view * worldPos;
    fragColor     = push.color.rgb;
}
