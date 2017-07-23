#version 440 core

layout (set = 0, binding = 0) uniform SCENE
{
  mat4 view;
  mat4 projection;
  vec4 lightDirection;
  vec4 lightColor;
}scene;

layout (set = 2, binding = 2) uniform MATERIAL
{
  vec4 albedo;
  vec3 F0;
  float roughness;
}material;

layout (location = 0) out vec4 color;

in vec3 normalViewSpace;
in vec3 lightDirectionViewSpace;

void main(void)
{  
  float diffuse = max(0.0, dot( normalViewSpace, lightDirectionViewSpace ));
  color = vec4( diffuse*scene.lightColor.rgb, 1.0) * material.albedo;
}
