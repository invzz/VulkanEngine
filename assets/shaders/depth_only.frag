#version 460

// Depth-only prepass fragment shader.
// No color outputs: the render pass has only a depth attachment.
layout(early_fragment_tests) in;

void main() {
}
