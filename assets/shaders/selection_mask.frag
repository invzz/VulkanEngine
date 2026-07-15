#version 450
// Selection mask fill: writes 1.0 wherever the selected geometry covers a pixel.
// Rendered with depth test disabled so the full projected silhouette is captured
// (the screen-space composite pass turns this into a Blender-style rim).
layout(location = 0) out float outMask;
void main() {
    outMask = 1.0;
}
