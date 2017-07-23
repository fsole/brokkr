#version 440 core
in vec2 uv;

layout (binding = 0) uniform sampler2D uTexture;
layout (location = 0) out vec4 color;

void main(void)
{
  vec4 texColor = texture(uTexture, uv);
  if( texColor.a == 0.0 )
    discard;

  color = texColor;
}
