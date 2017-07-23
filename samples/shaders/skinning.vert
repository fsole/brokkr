
#version 440 core
layout ( location = 0 ) in vec3 aPosition;
layout ( location = 1 ) in vec3 aNormal;
layout ( location = 2 ) in vec2 aTexCoord;
layout ( location = 3 ) in vec4 aBonesWeight;
layout ( location = 4 ) in vec4 aBonesId;


layout (binding = 1) uniform UNIFORMS
{
  mat4 modelView;
  mat4 modelViewProjection;
}uniforms;

layout (binding = 2) uniform BONESTX
{
  mat4 bones[64];
}bonesTx;


out vec3 normalViewSpace;
out vec3 lightViewSpace;

void main(void)
{

  mat4 transform = bonesTx.bones[int(aBonesId[0])] * aBonesWeight[0] +
                   bonesTx.bones[int(aBonesId[1])] * aBonesWeight[1] +
                   bonesTx.bones[int(aBonesId[2])] * aBonesWeight[2] +
                   bonesTx.bones[int(aBonesId[3])] * aBonesWeight[3];
	
  

  gl_Position = uniforms.modelViewProjection * transform * vec4(aPosition,1.0);

  mat4 normalTransform = mat4(inverse(transpose( uniforms.modelView * transform)) );
  normalViewSpace = normalize( (normalTransform * vec4(aNormal,0.0)).xyz);

  vec3 lightPositionModelSpace = vec3(-0.5,0.5,1.0);  
  lightViewSpace = normalize( (uniforms.modelView * vec4(normalize(lightPositionModelSpace),0.0)).xyz);
}
