#version 440 core
in vec2 uv;
layout (location = 0) out vec4 color;
void main(void)
{
  color = vec4(uv,0.0,1.0);
}
