#version 450

layout(location = 0) in vec3 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube skyboxSampler;

void main()
{
  // Flip Y axis to correct vertical orientation
  vec3 texCoord = vec3(fragTexCoord.x, -fragTexCoord.y, fragTexCoord.z);
  vec3 color    = texture(skyboxSampler, texCoord).rgb;

  outColor = vec4(color, 1.0);
}
