#version 440 core

in vec3 normalViewSpace;
in vec3 lightViewSpace;
layout (location = 0) out vec4 color;

void main(void)
{
  float diffuse = max( dot( normalize(lightViewSpace), normalize(normalViewSpace) ), 0.0);
  color = vec4(vec3(diffuse), 1.0);
}
