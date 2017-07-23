
#version 440 core
layout ( location = 0 ) in vec3 aPosition;
layout ( location = 1 ) in vec3 aNormal;
layout ( location = 2 ) in vec2 aTexCoord;

layout (binding = 1) uniform UNIFORMS
{
  mat4 modelViewProjection;
}uniforms;

out vec2 uv;
void main(void)
{
  gl_Position = uniforms.modelViewProjection * vec4(aPosition,1.0);
  uv = aTexCoord;
}
