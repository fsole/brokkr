/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/gui.h"

#include "core/render.h"
#include "core/window.h"
#include "core/mesh.h"
#include "core/maths.h"
#include "core/timer.h"
#include "core/transform-manager.h"
#include "core/packed-freelist.h"


using namespace bkk;
using namespace bkk::core;
using namespace bkk::core::maths;

static const char* gGeometryPassVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec3 aNormal;

  layout (set = 0, binding = 0) uniform SCENE
  {
    mat4 view;
    mat4 projection;
    mat4 projectionInverse;
    vec4 imageSize;
  }scene;

  layout(set = 1, binding = 0) uniform MODEL
  {
    mat4 transform;
  }model;

  layout(location = 0) out vec3 normalViewSpace;

  void main(void)
  {
    mat4 modelView = scene.view * model.transform;
    gl_Position = scene.projection * modelView * vec4(aPosition,1.0);
    normalViewSpace = normalize((transpose( inverse( modelView) ) * vec4(aNormal,0.0)).xyz);
  }
)";


static const char* gGeometryPassFragmentShaderSource = R"(
  #version 440 core

  layout(set = 2, binding = 0) uniform MATERIAL
  {
    vec3 albedo;
    float metallic;
    vec3 F0;
    float roughness;
  }material;

  layout(location = 0) out vec4 RT0;
  layout(location = 1) out vec4 RT1;
  layout(location = 2) out vec4 RT2;

  layout(location = 0) in vec3 normalViewSpace;

  void main(void)
  {
    RT0 = vec4(material.albedo, material.roughness);
    RT1 = vec4(normalize(normalViewSpace), gl_FragCoord.z );
    RT2 = vec4(material.F0, material.metallic);
  }
)";

static const char* gLightPassVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;

  layout(set = 0, binding = 0) uniform SCENE
  {
    mat4 view;
    mat4 projection;
    mat4 projectionInverse;
    vec4 imageSize;
  }scene;

  layout (set = 2, binding = 0) uniform LIGHT
  {
    vec4 position;
    vec3 color;
    float radius;
  }light;

  layout(location = 0) out vec3 lightPositionVS;

  void main(void)
  {
    mat4 viewProjection = scene.projection * scene.view;
    vec4 vertexPosition =  vec4( aPosition*light.radius+light.position.xyz, 1.0 );
    gl_Position = viewProjection * vertexPosition;
    lightPositionVS = (scene.view * light.position).xyz;
  }
)";


static const char* gLightPassFragmentShaderSource = R"(
  #version 440 core

  layout(set = 0, binding = 0) uniform SCENE
  {
    mat4 view;
    mat4 projection;
    mat4 projectionInverse;
    vec4 imageSize;
  }scene;

  layout (set = 2, binding = 0) uniform LIGHT
  {
    vec4 position;
    vec3 color;
    float radius;
  }light;

  layout(set = 1, binding = 0) uniform sampler2D RT0;
  layout(set = 1, binding = 1) uniform sampler2D RT1;
  layout(set = 1, binding = 2) uniform sampler2D RT2;

  layout(location = 0) in vec3 lightPositionVS;
  
  layout(location = 0) out vec4 result;

  const float PI = 3.14159265359;
  vec3 ViewSpacePositionFromDepth(vec2 uv, float depth)
  {
    vec3 clipSpacePosition = vec3(uv* 2.0 - 1.0, depth);
    vec4 viewSpacePosition = scene.projectionInverse * vec4(clipSpacePosition,1.0);
    return(viewSpacePosition.xyz / viewSpacePosition.w);
  }

  vec3 fresnelSchlick(float cosTheta, vec3 F0)
  {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
  }

  float DistributionGGX(vec3 N, vec3 H, float roughness)
  {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
  }

  float GeometrySchlickGGX(float NdotV, float roughness)
  {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
  }

  float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
  {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
  }

  void main(void)
  {
    vec2 uv = gl_FragCoord.xy * scene.imageSize.zw;
    vec4 RT0Value = texture(RT0, uv);
    vec3 albedo = RT0Value.xyz;
    float roughness = RT0Value.w;
    vec4 RT1Value = texture(RT1, uv);
    vec3 N = normalize(RT1Value.xyz); 
    float depth = RT1Value.w;
    vec4 RT2Value = texture(RT2, uv);
    vec3 positionVS = ViewSpacePositionFromDepth( uv,depth );
    vec3 L = normalize( lightPositionVS-positionVS );
    vec3 F0 = RT2Value.xyz;
    float metallic = RT2Value.w;
    vec3 V = -normalize(positionVS);
    vec3 H = normalize(V + L);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    vec3 nominator = NDF * G * F;
    float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = nominator / denominator;
    float lightDistance    = length(lightPositionVS - positionVS);
    float attenuation = 1.0 - clamp( lightDistance / light.radius, 0.0, 1.0);
    attenuation *= attenuation;
    float NdotL =  max( 0.0, dot( N, L ) );
    result = vec4( (kD * albedo / PI + specular) * (light.color*attenuation) * NdotL, 1.0 );
  }
)";

static const char* gPresentationVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;

  layout(location = 0) out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition,1.0);
    uv = aTexCoord;
  }
)";

static const char* gPresentationFragmentShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec2 uv;

  layout (set = 0, binding = 0) uniform sampler2D uTexture;

  layout(location = 0) out vec4 color;

  void main(void)
  {
    color = texture(uTexture, uv);
    color.rgb = pow(color.rgb, vec3(1.0 / 2.2));
  }
)";

class deferred_shading_sample_t : public framework::application_t
{
public:
  
  deferred_shading_sample_t()
  :application_t("Deferred shading", 1200u, 800u, 3u),
   currentPresentationDescriptorSet_(0u),
   camera_( vec3(0.0f, 2.5f, 8.5f), vec2(0.0f,0.0f), 1.0f, 0.01f),
   bAnimateLights_( true )
  {
    render::context_t& context = getRenderContext();
    uvec2 size = getWindowSize();

    //Create allocator for uniform buffers and meshes
    render::gpuAllocatorCreate(context, 100 * 1024 * 1024, 0xFFFF, render::gpu_memory_type_e::HOST_VISIBLE_COHERENT, &allocator_);

    //Create descriptor pool
    render::descriptorPoolCreate(context, 1000u,
      render::combined_image_sampler_count(1000u),
      render::uniform_buffer_count(1000u),
      render::storage_buffer_count(0u),
      render::storage_image_count(0u),
      &descriptorPool_);

    //Create vertex format (position + normal)
    uint32_t vertexSize = 2 * sizeof(maths::vec3);
    render::vertex_attribute_t attributes[2] = { { render::vertex_attribute_t::format_e::VEC3, 0, vertexSize, false },{ render::vertex_attribute_t::format_e::VEC3, sizeof(maths::vec3), vertexSize, false } };
    render::vertexFormatCreate(attributes, 2u, &vertexFormat_);

    //Load full-screen quad and sphere meshes
    fullScreenQuad_ = mesh::fullScreenQuad(context);
    mesh::createFromFile(context, "../resources/sphere.obj", mesh::EXPORT_POSITION_ONLY, nullptr, 0u, &sphereMesh_);

    //Create globals uniform buffer    
    sceneUniforms_.projectionMatrix = perspectiveProjectionMatrix(1.2f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
    invertMatrix(sceneUniforms_.projectionMatrix, sceneUniforms_.projectionInverseMatrix);
    sceneUniforms_.viewMatrix = camera_.getViewMatrix();
    sceneUniforms_.imageSize = vec4((f32)size.x, (f32)size.y, 1.0f / (f32)size.x, 1.0f / (f32)size.y);
    render::gpuBufferCreate(context, render::gpu_buffer_t::UNIFORM_BUFFER, (void*)&sceneUniforms_, sizeof(scene_uniforms_t), &allocator_, &globalsUbo_);

    //Create global descriptor set (Scene uniforms)   
    render::descriptor_binding_t binding = { render::descriptor_t::type_e::UNIFORM_BUFFER, 0, render::descriptor_t::stage_e::VERTEX | render::descriptor_t::stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &globalsDescriptorSetLayout_);
    render::descriptor_t descriptor = render::getDescriptor(globalsUbo_);
    render::descriptorSetCreate(context, descriptorPool_, globalsDescriptorSetLayout_, &descriptor, &globalsDescriptorSet_);

    //Create render targets 
    render::texture2DCreate(context, size.x, size.y, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &gBufferRT0_);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &gBufferRT0_);
    render::texture2DCreate(context, size.x, size.y, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &gBufferRT1_);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &gBufferRT1_);
    render::texture2DCreate(context, size.x, size.y, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &gBufferRT2_);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &gBufferRT2_);
    render::texture2DCreate(context, size.x, size.y, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &finalImage_);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &finalImage_);
    render::depthStencilBufferCreate(context, size.x, size.y, &depthStencilBuffer_);

    //Presentation descriptor set layout and pipeline layout
    binding = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &presentationDescriptorSetLayout_);
    render::pipelineLayoutCreate(context, &presentationDescriptorSetLayout_, 1u, nullptr, 0u, &presentationPipelineLayout_);

    //Presentation descriptor sets
    descriptor = render::getDescriptor(finalImage_);
    render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[0]);
    descriptor = render::getDescriptor(gBufferRT0_);
    render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[1]);
    descriptor = render::getDescriptor(gBufferRT1_);
    render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[2]);
    descriptor = render::getDescriptor(gBufferRT2_);
    render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[3]);

    //Create presentation pipeline
    render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, gPresentationVertexShaderSource, &presentationVertexShader_);
    render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, gPresentationFragmentShaderSource, &presentationFragmentShader_);
    render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
    pipelineDesc.scissorRect = { { 0,0 },{ context.swapChain.imageWidth,context.swapChain.imageHeight } };
    pipelineDesc.blendState.resize(1);
    pipelineDesc.blendState[0].colorWriteMask = 0xF;
    pipelineDesc.blendState[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled = false;
    pipelineDesc.depthWriteEnabled = false;
    pipelineDesc.vertexShader = presentationVertexShader_;
    pipelineDesc.fragmentShader = presentationFragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain.renderPass, 0u, fullScreenQuad_.vertexFormat, presentationPipelineLayout_, pipelineDesc, &presentationPipeline_);

    initializeOffscreenPass(context, size);    
  }

  core::handle_t addQuadMesh()
  {
    struct Vertex
    {
      float position[3];
      float normal[3];
    };

    static const Vertex vertices[] = { { { -1.0f, 0.0f, +1.0f }, { 0.0f, 1.0f, 0.0f } },
                                      { { +1.0f, 0.0f, +1.0f }, { 0.0f, 1.0f, 0.0f } },
                                      { { -1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
                                      { { +1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } }
                                      };

    static const uint32_t indices[] = {0,1,2,1,3,2};

    static render::vertex_attribute_t attributes[2];
    attributes[0] = { render::vertex_attribute_t::format_e::VEC3, 0, sizeof(Vertex), false };
    attributes[1] = { render::vertex_attribute_t::format_e::VEC3, offsetof(Vertex, normal), sizeof(Vertex), false };

    mesh::mesh_t mesh;
    mesh::create( getRenderContext(), indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &allocator_, &mesh );
    return mesh_.add( mesh );
  }

  core::handle_t addMesh(const char* url )
  {
    mesh::mesh_t mesh;
    mesh::createFromFile( getRenderContext(), url, mesh::EXPORT_NORMALS, &allocator_, 0u, &mesh);
    return mesh_.add( mesh );
  }

  core::handle_t addMaterial( const vec3& albedo, float metallic, const vec3& F0, float roughness )
  {
    render::context_t& context = getRenderContext();

    //Create uniform buffer and descriptor set
    material_t material = {};
    material.uniforms.albedo = albedo;
    material.uniforms.metallic = metallic;
    material.uniforms.F0 = F0;
    material.uniforms.roughness = roughness;
    render::gpuBufferCreate(  context, render::gpu_buffer_t::UNIFORM_BUFFER,
                              &material.uniforms, sizeof(material_t::uniforms_t),
                              &allocator_, &material.ubo);

    render::descriptor_t descriptor = render::getDescriptor(material.ubo);
    render::descriptorSetCreate( context, descriptorPool_, materialDescriptorSetLayout_, &descriptor, &material.descriptorSet );
    return material_.add( material );
  }

  core::handle_t addObject( core::handle_t meshId, core::handle_t materialId, const maths::mat4& transform )
  {
    render::context_t& context = getRenderContext();

    core::handle_t transformId = transformManager_.createTransform( transform );

    //Create uniform buffer and descriptor set
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate( context, render::gpu_buffer_t::UNIFORM_BUFFER,
                             nullptr, sizeof(mat4),
                             &allocator_, &ubo );

    object_t object = { meshId, materialId, transformId, ubo };
    render::descriptor_t descriptor = render::getDescriptor(object.ubo);
    render::descriptorSetCreate(context, descriptorPool_, objectDescriptorSetLayout_, &descriptor, &object.descriptorSet );
    return object_.add( object );
  }

  core::handle_t addLight(const maths::vec3& position,float radius, const maths::vec3& color )
  {
    render::context_t& context = getRenderContext();

    light_t light;

    light.uniforms.position = maths::vec4(position,1.0);
    light.uniforms.color = color;    
    light.uniforms.radius = radius;

    //Create uniform buffer and descriptor set
    render::gpuBufferCreate(context, render::gpu_buffer_t::UNIFORM_BUFFER,
      &light.uniforms, sizeof(light_t::uniforms_t),
      &allocator_, &light.ubo);

    
    render::descriptor_t descriptor = render::getDescriptor(light.ubo);
    render::descriptorSetCreate(context, descriptorPool_, lightDescriptorSetLayout_, &descriptor, &light.descriptorSet);
    return light_.add(light);
  }

  void onResize( uint32_t width, uint32_t height )
  {
    sceneUniforms_.projectionMatrix = perspectiveProjectionMatrix( 1.2f, (f32)width / (f32)height,0.1f,100.0f );
    buildPresentationCommandBuffers();
  }

  void render()
  {
    render::context_t& context = getRenderContext();
        
    //Update scene
    animateLights();
    transformManager_.update();
    sceneUniforms_.viewMatrix = camera_.getViewMatrix();
    render::gpuBufferUpdate(context, (void*)&sceneUniforms_, 0u, sizeof(scene_uniforms_t), &globalsUbo_);

    //Update modelview matrices
    object_t* objects;
    uint32_t objectCount = object_.getData(&objects);    
    for (u32 i(0); i < objectCount; ++i)
    {
      render::gpuBufferUpdate(context, transformManager_.getWorldMatrix(objects[i].transform), 0, sizeof(mat4), &objects[i].ubo );
    }

    //Update lights position
    light_t* lights;
    uint32_t lightCount = light_.getData(&lights);
    for (u32 i(0); i<lightCount; ++i)
    {
      render::gpuBufferUpdate(context, &lights[i].uniforms.position, 0, sizeof(vec4), &lights[i].ubo );
    }
    
    buildPresentationCommandBuffers();
    buildAndSubmitCommandBuffer();        
    render::presentFrame( &context, &renderComplete_, 1u);
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    if (pressed)
    {
      switch (key)
      {
      case window::key_e::KEY_UP:
      case 'w':
      {
        camera_.Move(0.0f, -0.5f);
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        camera_.Move(0.0f, 0.5f);
        break;
      }
      case window::key_e::KEY_LEFT:
      case 'a':
      {
        camera_.Move(-0.5f, 0.0f);
        break;
      }
      case window::key_e::KEY_RIGHT:
      case 'd':
      {
        camera_.Move(0.5f, 0.0f);
        break;
      }
      case window::key_e::KEY_1:
      case window::key_e::KEY_2:
      case window::key_e::KEY_3:
      case window::key_e::KEY_4:
      {
        currentPresentationDescriptorSet_ = key - window::key_e::KEY_1;
        render::contextFlush(getRenderContext());
        buildPresentationCommandBuffers();
        break;
      }
      case window::key_e::KEY_P:
      {
        bAnimateLights_ = !bAnimateLights_;
        break;
      }
      default:
        break;
      }
    }
  }

  void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos)
  {
    if (getMousePressedButton() == window::MOUSE_RIGHT)
    {
      camera_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
    }
  }

  void onQuit()
  {   
    render::context_t& context = getRenderContext();
    render::contextFlush(context);

    //Destroy meshes
    packed_freelist_iterator_t<mesh::mesh_t> meshIter = mesh_.begin();
    while( meshIter != mesh_.end() )
    {
      mesh::destroy( context, &meshIter.get(), &allocator_ );
      ++meshIter;
    }

    //Destroy material resources
    packed_freelist_iterator_t<material_t> materialIter = material_.begin();
    while( materialIter != material_.end() )
    {
      render::gpuBufferDestroy(context, &allocator_, &materialIter.get().ubo );
      render::descriptorSetDestroy(context, &materialIter.get().descriptorSet );
      ++materialIter;
    }

    //Destroy object resources
    packed_freelist_iterator_t<object_t> objectIter = object_.begin();
    while( objectIter != object_.end() )
    {
      render::gpuBufferDestroy(context, &allocator_, &objectIter.get().ubo );
      render::descriptorSetDestroy(context, &objectIter.get().descriptorSet );
      ++objectIter;
    }

    //Destroy lights resources
    packed_freelist_iterator_t<light_t> lightIter = light_.begin();
    while (lightIter != light_.end())
    {
      render::gpuBufferDestroy(context, &allocator_, &lightIter.get().ubo);
      render::descriptorSetDestroy(context, &lightIter.get().descriptorSet);
      ++lightIter;
    }

    
    render::shaderDestroy(context, &gBuffervertexShader_);
    render::shaderDestroy(context, &gBufferfragmentShader_);
    render::shaderDestroy(context, &lightVertexShader_);
    render::shaderDestroy(context, &lightFragmentShader_);
    render::shaderDestroy(context, &presentationVertexShader_);
    render::shaderDestroy(context, &presentationFragmentShader_);
    
    render::graphicsPipelineDestroy(context, &gBufferPipeline_);
    render::graphicsPipelineDestroy(context, &lightPipeline_);
    render::graphicsPipelineDestroy(context, &presentationPipeline_);

    render::pipelineLayoutDestroy(context, &presentationPipelineLayout_);
    render::pipelineLayoutDestroy(context, &gBufferPipelineLayout_);
    render::pipelineLayoutDestroy(context, &lightPipelineLayout_);
    
    render::descriptorSetDestroy(context, &globalsDescriptorSet_);
    render::descriptorSetDestroy(context, &lightPassTexturesDescriptorSet_);
    render::descriptorSetDestroy(context, &presentationDescriptorSet_[0]);
    render::descriptorSetDestroy(context, &presentationDescriptorSet_[1]);
    render::descriptorSetDestroy(context, &presentationDescriptorSet_[2]);

    render::descriptorSetLayoutDestroy(context, &globalsDescriptorSetLayout_);
    render::descriptorSetLayoutDestroy(context, &materialDescriptorSetLayout_);
    render::descriptorSetLayoutDestroy(context, &objectDescriptorSetLayout_);
    render::descriptorSetLayoutDestroy(context, &lightDescriptorSetLayout_);
    render::descriptorSetLayoutDestroy(context, &lightPassTexturesDescriptorSetLayout_);
    render::descriptorSetLayoutDestroy(context, &presentationDescriptorSetLayout_);

    render::textureDestroy(context, &gBufferRT0_);
    render::textureDestroy(context, &gBufferRT1_);
    render::textureDestroy(context, &gBufferRT2_);
    render::textureDestroy(context, &finalImage_);
    render::depthStencilBufferDestroy(context, &depthStencilBuffer_);
    
    mesh::destroy(context, &fullScreenQuad_);
    mesh::destroy(context, &sphereMesh_);

    render::frameBufferDestroy(context, &frameBuffer_);
    render::commandBufferDestroy(context, &commandBuffer_);
    render::renderPassDestroy(context, &renderPass_);
    render::vertexFormatDestroy(&vertexFormat_);
    render::gpuBufferDestroy(context, &allocator_, &globalsUbo_);
    render::gpuAllocatorDestroy(context, &allocator_);
    render::descriptorPoolDestroy(context, &descriptorPool_);
    render::semaphoreDestroy( context, renderComplete_);
  }
  
private:
  ///Helper methods
  void initializeOffscreenPass(render::context_t& context, const uvec2& size)
  {
    //Semaphore to indicate rendering has completed
    renderComplete_ = render::semaphoreCreate(context);
    
    //Create offscreen render pass (GBuffer + light subpasses)
    renderPass_ = {};
    render::render_pass_t::attachment_t attachments[5];
    attachments[0].format = gBufferRT0_.format;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;

    attachments[1].format = gBufferRT1_.format;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;

    attachments[2].format = gBufferRT2_.format;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;

    attachments[3].format = finalImage_.format;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;

    attachments[4].format = depthStencilBuffer_.format;
    attachments[4].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].samples = VK_SAMPLE_COUNT_1_BIT;

    render::render_pass_t::subpass_t subpasses[2];
    subpasses[0].colorAttachmentIndex.push_back(0);
    subpasses[0].colorAttachmentIndex.push_back(1);
    subpasses[0].colorAttachmentIndex.push_back(2);
    subpasses[0].depthStencilAttachmentIndex = 4;

    subpasses[1].inputAttachmentIndex.push_back(0);
    subpasses[1].inputAttachmentIndex.push_back(1);
    subpasses[1].inputAttachmentIndex.push_back(2);
    subpasses[1].colorAttachmentIndex.push_back(3);

    //Dependency chain for layout transitions
    render::render_pass_t::subpass_dependency_t dependency;    
    dependency.srcSubpass = 0;
    dependency.dstSubpass = 1;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;    

    render::renderPassCreate(context, attachments, 5u, subpasses, 2u, &dependency, 1u, &renderPass_);

    //Create frame buffer
    VkImageView fbAttachment[5] = { gBufferRT0_.imageView, gBufferRT1_.imageView, gBufferRT2_.imageView, finalImage_.imageView, depthStencilBuffer_.imageView };
    render::frameBufferCreate(context, size.x, size.y, renderPass_, fbAttachment, &frameBuffer_);

    //Create descriptorSets layouts
    render::descriptor_binding_t binding = { render::descriptor_t::type_e::UNIFORM_BUFFER, 0u, render::descriptor_t::stage_e::VERTEX };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &objectDescriptorSetLayout_);

    binding = { render::descriptor_t::type_e::UNIFORM_BUFFER, 0u, render::descriptor_t::stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &materialDescriptorSetLayout_);

    //Create gBuffer pipeline layout
    render::descriptor_set_layout_t descriptorSetLayouts[3] = { globalsDescriptorSetLayout_, objectDescriptorSetLayout_, materialDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, descriptorSetLayouts, 3u, nullptr, 0u, &gBufferPipelineLayout_);

    //Create geometry pass pipeline
    render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, gGeometryPassVertexShaderSource, &gBuffervertexShader_);
    render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, gGeometryPassFragmentShaderSource, &gBufferfragmentShader_);
    render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
    pipelineDesc.scissorRect = { { 0,0 },{ context.swapChain.imageWidth,context.swapChain.imageHeight } };
    pipelineDesc.blendState.resize(3);
    pipelineDesc.blendState[0].colorWriteMask = 0xF;
    pipelineDesc.blendState[0].blendEnable = VK_FALSE;
    pipelineDesc.blendState[1].colorWriteMask = 0xF;
    pipelineDesc.blendState[1].blendEnable = VK_FALSE;
    pipelineDesc.blendState[2].colorWriteMask = 0xF;
    pipelineDesc.blendState[2].blendEnable = VK_FALSE;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled = true;
    pipelineDesc.depthWriteEnabled = true;
    pipelineDesc.depthTestFunction = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDesc.vertexShader = gBuffervertexShader_;
    pipelineDesc.fragmentShader = gBufferfragmentShader_;
    render::graphicsPipelineCreate(context, renderPass_.handle, 0u, vertexFormat_, gBufferPipelineLayout_, pipelineDesc, &gBufferPipeline_);

    //Create light pass descriptorSet layouts
    render::descriptor_binding_t bindings[3];
    bindings[0] = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage_e::FRAGMENT };
    bindings[1] = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 1, render::descriptor_t::stage_e::FRAGMENT };
    bindings[2] = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 2, render::descriptor_t::stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate(context, bindings, 3u, &lightPassTexturesDescriptorSetLayout_);

    binding = { render::descriptor_t::type_e::UNIFORM_BUFFER, 0, render::descriptor_t::stage_e::VERTEX | render::descriptor_t::stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &lightDescriptorSetLayout_);

    //Create descriptor sets for light pass (GBuffer textures)
    render::descriptor_t descriptors[3];
    descriptors[0] = render::getDescriptor(gBufferRT0_);
    descriptors[1] = render::getDescriptor(gBufferRT1_);
    descriptors[2] = render::getDescriptor(gBufferRT2_);
    render::descriptorSetCreate(context, descriptorPool_, lightPassTexturesDescriptorSetLayout_, descriptors, &lightPassTexturesDescriptorSet_);

    //Create light pass pipeline layout
    render::descriptor_set_layout_t lightPassDescriptorSetLayouts[3] = { globalsDescriptorSetLayout_, lightPassTexturesDescriptorSetLayout_, lightDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, lightPassDescriptorSetLayouts, 3u, nullptr, 0u, &lightPipelineLayout_);

    //Create light pass pipeline
    render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, gLightPassVertexShaderSource, &lightVertexShader_);
    render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, gLightPassFragmentShaderSource, &lightFragmentShader_);
    render::graphics_pipeline_t::description_t lightPipelineDesc = {};
    lightPipelineDesc.viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
    lightPipelineDesc.scissorRect = { { 0,0 },{ context.swapChain.imageWidth,context.swapChain.imageHeight } };
    lightPipelineDesc.blendState.resize(1);
    lightPipelineDesc.blendState[0].colorWriteMask = 0xF;
    lightPipelineDesc.blendState[0].blendEnable = VK_TRUE;
    lightPipelineDesc.blendState[0].colorBlendOp = VK_BLEND_OP_ADD;
    lightPipelineDesc.blendState[0].alphaBlendOp = VK_BLEND_OP_ADD;
    lightPipelineDesc.blendState[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.blendState[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.blendState[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.blendState[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.cullMode = VK_CULL_MODE_FRONT_BIT;
    lightPipelineDesc.depthTestEnabled = false;
    lightPipelineDesc.depthWriteEnabled = false;
    lightPipelineDesc.vertexShader = lightVertexShader_;
    lightPipelineDesc.fragmentShader = lightFragmentShader_;
    render::graphicsPipelineCreate(context, renderPass_.handle, 1u, sphereMesh_.vertexFormat, lightPipelineLayout_, lightPipelineDesc, &lightPipeline_);
  }

  void buildAndSubmitCommandBuffer()
  {
    render::context_t& context = getRenderContext();

    if (commandBuffer_.handle == VK_NULL_HANDLE)
    {
      render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, &renderComplete_, 1u, render::command_buffer_t::GRAPHICS, &commandBuffer_);
    }

    VkClearValue clearValues[5];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[4].depthStencil = { 1.0f,0 };

    render::commandBufferBegin(context, commandBuffer_);
    {
      render::commandBufferRenderPassBegin(context, &frameBuffer_, clearValues, 5u, commandBuffer_);

      //GBuffer pass
      render::graphicsPipelineBind(commandBuffer_, gBufferPipeline_);
      render::descriptor_set_t descriptorSets[3];
      descriptorSets[0] = globalsDescriptorSet_;
      packed_freelist_iterator_t<object_t> objectIter = object_.begin();
      while (objectIter != object_.end())
      {
        descriptorSets[1] = objectIter.get().descriptorSet;
        descriptorSets[2] = material_.get(objectIter.get().material)->descriptorSet;
        render::descriptorSetBind(commandBuffer_, gBufferPipelineLayout_, 0, descriptorSets, 3u);
        mesh::mesh_t* mesh = mesh_.get(objectIter.get().mesh);
        mesh::draw(commandBuffer_, *mesh);
        ++objectIter;
      }

      render::commandBufferNextSubpass(commandBuffer_);
            
      //Light pass      
      render::graphicsPipelineBind(commandBuffer_, lightPipeline_);
      descriptorSets[1] = lightPassTexturesDescriptorSet_;
      packed_freelist_iterator_t<light_t> lightIter = light_.begin();
      while (lightIter != light_.end())
      {
        descriptorSets[2] = lightIter.get().descriptorSet;
        render::descriptorSetBind(commandBuffer_, lightPipelineLayout_, 0u, descriptorSets, 3u);
        mesh::draw(commandBuffer_, sphereMesh_);
        ++lightIter;
      }

      render::commandBufferRenderPassEnd(commandBuffer_);
    }
    
    render::commandBufferEnd(commandBuffer_);
    render::commandBufferSubmit(context, commandBuffer_);
  }

  void buildPresentationCommandBuffers()
  {
    render::context_t& context = getRenderContext();

    const render::command_buffer_t* commandBuffers;
    uint32_t count = render::getPresentationCommandBuffers(context, &commandBuffers);
    for (uint32_t i(0); i<count; ++i)
    {
      render::beginPresentationCommandBuffer(context, i, nullptr);
      render::graphicsPipelineBind(commandBuffers[i], presentationPipeline_);
      render::descriptorSetBind(commandBuffers[i], presentationPipelineLayout_, 0u, &presentationDescriptorSet_[currentPresentationDescriptorSet_], 1u);
      mesh::draw(commandBuffers[i], fullScreenQuad_);

      framework::gui::draw(context, commandBuffers[i]);

      render::endPresentationCommandBuffer(context, i);
    }
  }

  void animateLights()
  {
    if (!bAnimateLights_)
      return;

    static const vec3 light_path[] =
    {
      vec3(-3.0f,  3.0f, 4.0f),
      vec3(-3.0f,  3.0f, -3.0f),
      vec3(3.0f,   3.0f, -3.0f),
      vec3(3.0f,   3.0f, 4.0f),
      vec3(-3.0f,  3.0f, 4.0f)
    };

    static f32 totalTime = 0.0f;
    totalTime += getTimeDelta()*0.001f;
    light_t* lights;
    uint32_t lightCount = light_.getData(&lights);
    for (u32 i(0); i<lightCount; ++i)
    {
      float t = totalTime + i* 5.0f / lightCount;
      int baseFrame = (int)t;
      float f = t - baseFrame;

      vec3 p0 = light_path[(baseFrame + 0) % 5];
      vec3 p1 = light_path[(baseFrame + 1) % 5];
      vec3 p2 = light_path[(baseFrame + 2) % 5];
      vec3 p3 = light_path[(baseFrame + 3) % 5];

      lights[i].uniforms.position = vec4(maths::cubicInterpolation(p0, p1, p2, p3, f), 1.0);
    }
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");
    ImGui::Checkbox("Animate Lights", &bAnimateLights_);
    ImGui::End();
  }

private:

  struct light_t
  {
    struct uniforms_t
    {
      maths::vec4 position;
      maths::vec3 color;
      float radius;
    }uniforms;

    render::gpu_buffer_t ubo;
    render::descriptor_set_t descriptorSet;
  };

  struct material_t
  {
    struct uniforms_t
    {
      vec3 albedo;
      float metallic;
      vec3 F0;
      float roughness;
    }uniforms;

    render::gpu_buffer_t ubo;
    render::descriptor_set_t descriptorSet;
  };

  struct object_t
  {
    core::handle_t mesh;
    core::handle_t material;
    core::handle_t transform;
    render::gpu_buffer_t ubo;
    render::descriptor_set_t descriptorSet;
  };

  struct scene_uniforms_t
  {
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 projectionInverseMatrix;
    vec4 imageSize;
  };

  ///Member variables
  transform_manager_t transformManager_;
  render::gpu_memory_allocator_t allocator_;

  packed_freelist_t<object_t> object_;
  packed_freelist_t<material_t> material_;
  packed_freelist_t<mesh::mesh_t> mesh_;
  packed_freelist_t<light_t> light_;

  render::descriptor_pool_t descriptorPool_;  
  render::descriptor_set_layout_t globalsDescriptorSetLayout_;
  render::descriptor_set_layout_t materialDescriptorSetLayout_;
  render::descriptor_set_layout_t objectDescriptorSetLayout_;
  render::descriptor_set_layout_t lightDescriptorSetLayout_;
  render::descriptor_set_layout_t lightPassTexturesDescriptorSetLayout_;
  render::descriptor_set_layout_t presentationDescriptorSetLayout_;
  
  uint32_t currentPresentationDescriptorSet_;
  render::descriptor_set_t presentationDescriptorSet_[4];
  render::descriptor_set_t globalsDescriptorSet_;
  render::descriptor_set_t lightPassTexturesDescriptorSet_;

  render::vertex_format_t vertexFormat_;
  
  render::pipeline_layout_t gBufferPipelineLayout_;
  render::graphics_pipeline_t gBufferPipeline_;
  render::pipeline_layout_t lightPipelineLayout_;
  render::graphics_pipeline_t lightPipeline_;

  render::pipeline_layout_t presentationPipelineLayout_;
  render::graphics_pipeline_t presentationPipeline_;
    
  VkSemaphore renderComplete_;
  render::command_buffer_t commandBuffer_;
  render::render_pass_t renderPass_;

  scene_uniforms_t sceneUniforms_;  
  render::gpu_buffer_t globalsUbo_;

  render::frame_buffer_t frameBuffer_;
  render::texture_t gBufferRT0_;  //Albedo + roughness
  render::texture_t gBufferRT1_;  //Normal + Depth
  render::texture_t gBufferRT2_;  //F0 + metallic
  render::texture_t finalImage_;
  render::depth_stencil_buffer_t depthStencilBuffer_;
  
  render::shader_t gBuffervertexShader_;
  render::shader_t gBufferfragmentShader_;
  render::shader_t lightVertexShader_;
  render::shader_t lightFragmentShader_;
  render::shader_t presentationVertexShader_;
  render::shader_t presentationFragmentShader_;

  mesh::mesh_t sphereMesh_;
  mesh::mesh_t fullScreenQuad_;
  
  framework::free_camera_t camera_;
  bool bAnimateLights_;
};


int main()
{
  deferred_shading_sample_t scene;
  
  //Materials  
  core::handle_t wall = scene.addMaterial(vec3(0.5f, 0.5f, 0.5f), 0.0f, vec3(0.004f, 0.004f, 0.004f),0.7f);
  core::handle_t redWall = scene.addMaterial( vec3(0.5f,0.0f,0.0f), 0.0f, vec3(0.04f, 0.04f, 0.04f), 0.7f);
  core::handle_t greenWall = scene.addMaterial( vec3(0.0f,0.5f,0.0f), 0.0f, vec3(0.004f, 0.004f, 0.004f), 0.7f);
  core::handle_t gold = scene.addMaterial(vec3(0.0f, 0.0f, 0.0f), 1.0f, vec3(1.000f, 0.766f, 0.336f), 0.3f);
  core::handle_t silver = scene.addMaterial(vec3(0.0f, 0.0f, 0.0f), 1.0f, vec3(0.972f, 0.960f, 0.915f), 0.3f);
  core::handle_t copper = scene.addMaterial(vec3(0.0f, 0.0f, 0.0f), 1.0f, vec3(1.0f, 0.437f, 0.338f), 0.3f);
  core::handle_t redPlastic = scene.addMaterial(vec3(1.0f, 0.0f, 0.0f), 0.0f, vec3(0.05f, 0.05f, 0.05f), 0.4f);
  core::handle_t bluePlastic = scene.addMaterial(vec3(0.0f, 0.0f, 1.0f), 0.0f, vec3(0.05f, 0.05f, 0.05f), 0.4f);

  //Meshes
  core::handle_t bunny = scene.addMesh( "../resources/bunny.ply" );
  core::handle_t dragon = scene.addMesh("../resources/dragon.obj");
  core::handle_t buddha = scene.addMesh("../resources/buddha.obj");
  core::handle_t armadillo = scene.addMesh("../resources/armadillo.obj");
  core::handle_t teapot = scene.addMesh("../resources/teapot.obj");
  core::handle_t quad = scene.addQuadMesh();


  //Objects
  scene.addObject(quad, wall, maths::createTransform( maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::QUAT_UNIT));
  scene.addObject(quad, redWall, maths::createTransform(maths::vec3(-5.0f, 4.0f, 0.0f), maths::vec3(4.0f, 5.0f, 5.0f), maths::quaternionFromAxisAngle(vec3(0, 0, 1), maths::degreeToRadian(90.0f))));
  scene.addObject(quad, greenWall, maths::createTransform(maths::vec3(5.0f, 4.0f, 0.0f), maths::vec3(4.0f, 5.0f, 5.0f), maths::quaternionFromAxisAngle(vec3(0, 0, 1), maths::degreeToRadian(-90.0f))));
  scene.addObject(quad, wall, maths::createTransform(maths::vec3(0.0f, 4.0f, -5.0f), maths::vec3(5.0f, 5.0f, 4.0f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(-90.0f))));
  scene.addObject(quad, wall, maths::createTransform(maths::vec3(0.0f, 8.0f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(180.0f))));
  scene.addObject(bunny, bluePlastic, maths::createTransform(maths::vec3(0.0f, -0.4f, 0.5f), maths::vec3(12.0f, 12.0f, 12.0f), maths::QUAT_UNIT));
  scene.addObject(dragon, silver, maths::createTransform(maths::vec3(2.5f, 1.2f, 4.0f), maths::vec3(1.7f, 1.7f, 1.7f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(90.0f))*maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(-10.0f))));
  scene.addObject(buddha, copper, maths::createTransform(maths::vec3(-3.0f, 1.4f, 4.0f), maths::vec3(1.5f, 1.5f, 1.5f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(90.0f))*maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(-30.0f))));
  scene.addObject(armadillo, redPlastic, maths::createTransform(maths::vec3(-3.0f, 1.1f, -1.5f), maths::vec3(1.0f, 1.0f, 1.0f), maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(180.0f))));
  scene.addObject(teapot, gold, maths::createTransform(maths::vec3(2.0f, 0.0f, -3.0f), maths::vec3(0.5f, 0.5f, 0.5f), maths::QUAT_UNIT));

  //Lights
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(1.5f,0.0f,0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 1.5f, 0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 0.0f, 1.5f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(1.5f, 0.0f, 0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 1.5f, 0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 0.0f, 1.5f));
  
  scene.loop();
  return 0;
}