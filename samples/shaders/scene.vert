
#version 440 core
layout ( location = 0 ) in vec3 aPosition;
layout ( location = 1 ) in vec3 aNormal;

layout (set = 0, binding = 0) uniform SCENE
{
  mat4 view;
  mat4 projection;
  vec4 lightDirection;
  vec4 lightColor;
}scene;

layout (set = 1, binding = 1) uniform MODEL
{
  mat4 value;
}model;

out vec3 normalViewSpace;
out vec3 lightDirectionViewSpace;
void main(void)
{
  mat4 modelView = scene.view * model.value;
  gl_Position = scene.projection * modelView * vec4(aPosition,1.0);
  normalViewSpace = normalize( (modelView * vec4(aNormal,0.0)).xyz );
  lightDirectionViewSpace = normalize( (scene.view * normalize(scene.lightDirection)).xyz );;
}
