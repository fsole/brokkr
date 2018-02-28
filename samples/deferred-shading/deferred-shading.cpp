/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "render.h"
#include "window.h"
#include "mesh.h"
#include "maths.h"
#include "timer.h"
#include "../utility.h"
#include "transform-manager.h"
#include "packed-freelist.h"

using namespace bkk;
using namespace maths;
using namespace sample_utils;

static const char* gGeometryPassVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec3 aNormal;\n \
  layout (set = 0, binding = 0) uniform SCENE\n \
  {\n \
  mat4 view;\n \
  mat4 projection;\n \
  mat4 projectionInverse;\n \
  vec4 imageSize;\n \
  }scene;\n \
  layout(set = 1, binding = 0) uniform MODEL\n \
  {\n \
    mat4 transform;\n \
  }model;\n \
  layout(location = 0) out vec3 normalViewSpace;\n \
  void main(void)\n \
  {\n \
    mat4 modelView = scene.view * model.transform;\n \
    gl_Position = scene.projection * modelView * vec4(aPosition,1.0);\n \
    normalViewSpace = normalize((transpose( inverse( modelView) ) * vec4(aNormal,0.0)).xyz);\n \
  }\n"
};


static const char* gGeometryPassFragmentShaderSource = {
  "#version 440 core\n \
  layout(set = 2, binding = 0) uniform MATERIAL\n \
  {\n \
    vec3 albedo;\n \
    float metallic;\n\
    vec3 F0;\n \
    float roughness;\n \
  }material;\n \
  layout(location = 0) out vec4 RT0;\n \
  layout(location = 1) out vec4 RT1;\n \
  layout(location = 2) out vec4 RT2;\n \
  layout(location = 0) in vec3 normalViewSpace;\n \
  void main(void)\n \
  {\n \
    RT0 = vec4(material.albedo, material.roughness);\n \
    RT1 = vec4(normalize(normalViewSpace), gl_FragCoord.z );\n \
    RT2 = vec4(material.F0, material.metallic);\n \
  }\n"
};

static const char* gLightPassVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(set = 0, binding = 0) uniform SCENE\n \
  {\n \
    mat4 view;\n \
    mat4 projection;\n \
    mat4 projectionInverse;\n \
    vec4 imageSize;\n \
  }scene;\n \
  layout (set = 2, binding = 0) uniform LIGHT\n \
  {\n \
   vec4 position;\n \
   vec3 color;\n \
   float radius;\n \
  }light;\n \
  layout(location = 0) out vec3 lightPositionVS;\n\
  void main(void)\n \
  {\n \
    mat4 viewProjection = scene.projection * scene.view;\n \
    vec4 vertexPosition =  vec4( aPosition*light.radius+light.position.xyz, 1.0 );\n\
    gl_Position = viewProjection * vertexPosition;\n\
    lightPositionVS = (scene.view * light.position).xyz;\n\
  }\n"
};


static const char* gLightPassFragmentShaderSource = {
  "#version 440 core\n \
  layout(set = 0, binding = 0) uniform SCENE\n \
  {\n \
    mat4 view;\n \
    mat4 projection;\n \
    mat4 projectionInverse;\n \
    vec4 imageSize;\n \
  }scene;\n \
  layout (set = 2, binding = 0) uniform LIGHT\n \
  {\n \
   vec4 position;\n \
   vec3 color;\n \
   float radius;\n \
  }light;\n \
  layout(set = 1, binding = 0) uniform sampler2D RT0;\n \
  layout(set = 1, binding = 1) uniform sampler2D RT1;\n \
  layout(set = 1, binding = 2) uniform sampler2D RT2;\n \
  layout(location = 0) in vec3 lightPositionVS;\n\
  const float PI = 3.14159265359;\n\
  layout(location = 0) out vec4 result;\n \
  vec3 ViewSpacePositionFromDepth(vec2 uv, float depth)\n\
  {\n\
    vec3 clipSpacePosition = vec3(uv* 2.0 - 1.0, depth);\n\
    vec4 viewSpacePosition = scene.projectionInverse * vec4(clipSpacePosition,1.0);\n\
    return(viewSpacePosition.xyz / viewSpacePosition.w);\n\
  }\n\
  vec3 fresnelSchlick(float cosTheta, vec3 F0)\n\
  {\n\
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);\n\
  }\n\
  float DistributionGGX(vec3 N, vec3 H, float roughness)\n\
  {\n\
    float a = roughness*roughness;\n\
    float a2 = a*a;\n\
    float NdotH = max(dot(N, H), 0.0);\n\
    float NdotH2 = NdotH*NdotH;\n\
    float nom = a2;\n\
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);\n\
    denom = PI * denom * denom;\n\
    return nom / denom;\n\
  }\n\
  float GeometrySchlickGGX(float NdotV, float roughness)\n\
  {\n\
    float r = (roughness + 1.0);\n\
    float k = (r*r) / 8.0;\n\
    float nom = NdotV;\n\
    float denom = NdotV * (1.0 - k) + k;\n\
    return nom / denom;\n\
  }\n\
  float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)\n\
  {\n\
    float NdotV = max(dot(N, V), 0.0);\n\
    float NdotL = max(dot(N, L), 0.0);\n\
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);\n\
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);\n\
    return ggx1 * ggx2;\n\
  }\n\
  void main(void)\n \
  {\n \
    vec2 uv = gl_FragCoord.xy * scene.imageSize.zw;\n\
    vec4 RT0Value = texture(RT0, uv);\n \
    vec3 albedo = RT0Value.xyz;\n\
    float roughness = RT0Value.w;\n\
    vec4 RT1Value = texture(RT1, uv);\n \
    vec3 N = normalize(RT1Value.xyz); \n \
    float depth = RT1Value.w;\n\
    vec4 RT2Value = texture(RT2, uv);\n \
    vec3 positionVS = ViewSpacePositionFromDepth( uv,depth );\n\
    vec3 L = normalize( lightPositionVS-positionVS );\n\
    vec3 F0 = RT2Value.xyz;\n \
    float metallic = RT2Value.w;\n\
    vec3 V = -normalize(positionVS);\n\
    vec3 H = normalize(V + L);\n\
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);\n \
    float NDF = DistributionGGX(N, H, roughness);\n\
    float G = GeometrySmith(N, V, L, roughness);\n\
    vec3 kS = F;\n\
    vec3 kD = vec3(1.0) - kS;\n\
    kD *= 1.0 - metallic;\n\
    vec3 nominator = NDF * G * F;\n\
    float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;\n\
    vec3 specular = nominator / denominator;\n\
    float lightDistance    = length(lightPositionVS - positionVS);\n\
    float attenuation = 1.0 - clamp( lightDistance / light.radius, 0.0, 1.0);\n\
    attenuation *= attenuation;\n\
    float NdotL =  max( 0.0, dot( N, L ) );\n \
    vec3 color = (kD * albedo / PI + specular) * (light.color*attenuation) * NdotL;\n\
    color = color / (color + vec3(1.0));\n\
    color = pow(color, vec3(1.0 / 2.2));\n\
    result = vec4(color,1.0);\n\
  }\n"
};

static const char* gPresentationVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec2 aTexCoord;\n \
  layout(location = 0) out vec2 uv;\n \
  void main(void)\n \
  {\n \
    gl_Position = vec4(aPosition,1.0);\n \
    uv = aTexCoord;\n \
  }\n"
};

static const char* gPresentationFragmentShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec2 uv;\n  \
  layout (set = 0, binding = 0) uniform sampler2D uTexture;\n \
  layout(location = 0) out vec4 color;\n \
  void main(void)\n \
  {\n \
    color = texture(uTexture, uv);\n \
  }\n"
};

struct deferred_shading_sample_t : public application_t
{
  struct light_t
  {
    struct uniforms_t
    {
      maths::vec4 position_;
      maths::vec3 color_;
      float radius_;
    };

    uniforms_t uniforms_;
    render::gpu_buffer_t ubo_;
    render::descriptor_set_t descriptorSet_;
  };

  struct material_t
  {
    struct uniforms_t
    {
      vec3 albedo_;
      float metallic_;
      vec3 F0_;
      float roughness_;
    };

    uniforms_t uniforms_;
    render::gpu_buffer_t ubo_;
    render::descriptor_set_t descriptorSet_;
  };

  struct object_t
  {
    bkk::handle_t mesh_;
    bkk::handle_t material_;
    bkk::handle_t transform_;
    render::gpu_buffer_t ubo_;
    render::descriptor_set_t descriptorSet_;
  };

  struct scene_uniforms_t
  {
    mat4 viewMatrix_;
    mat4 projectionMatrix_;
    mat4 projectionInverseMatrix_;
    vec4 imageSize_;
  };

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
    render::descriptorPoolCreate(context, 1000u, 1000u, 1000u, 0u, 0u, &descriptorPool_);

    //Create vertex format (position + normal)
    uint32_t vertexSize = 2 * sizeof(maths::vec3);
    render::vertex_attribute_t attributes[2] = { { render::vertex_attribute_t::format::VEC3, 0, vertexSize },{ render::vertex_attribute_t::format::VEC3, sizeof(maths::vec3), vertexSize } };
    render::vertexFormatCreate(attributes, 2u, &vertexFormat_);

    //Load full-screen quad and sphere meshes
    fullScreenQuad_ = sample_utils::fullScreenQuad(context);
    mesh::createFromFile(context, "../resources/sphere.obj", mesh::EXPORT_POSITION_ONLY, nullptr, 0u, &sphereMesh_);

    //Create globals uniform buffer    
    sceneUniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix(1.2f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
    computeInverse(sceneUniforms_.projectionMatrix_, sceneUniforms_.projectionInverseMatrix_);
    sceneUniforms_.viewMatrix_ = camera_.view_;
    sceneUniforms_.imageSize_ = vec4((f32)size.x, (f32)size.y, 1.0f / (f32)size.x, 1.0f / (f32)size.y);
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER, (void*)&sceneUniforms_, sizeof(scene_uniforms_t), &allocator_, &globalsUbo_);

    //Create global descriptor set (Scene uniforms)   
    render::descriptor_binding_t binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0, render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &globalsDescriptorSetLayout_);
    render::descriptor_t descriptor = render::getDescriptor(globalsUbo_);
    render::descriptorSetCreate(context, descriptorPool_, globalsDescriptorSetLayout_, &descriptor, &globalsDescriptorSet_);

    //Create render targets 
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &gBufferRT0_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &gBufferRT0_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &gBufferRT1_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &gBufferRT1_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &gBufferRT2_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &gBufferRT2_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), &finalImage_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &finalImage_);
    render::depthStencilBufferCreate(context, size.x, size.y, &depthStencilBuffer_);

    //Presentation descriptor set layout and pipeline layout
    binding = { bkk::render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, bkk::render::descriptor_t::stage::FRAGMENT };
    bkk::render::descriptorSetLayoutCreate(context, &binding, 1u, &presentationDescriptorSetLayout_);
    bkk::render::pipelineLayoutCreate(context, &presentationDescriptorSetLayout_, 1u, &presentationPipelineLayout_);

    //Presentation descriptor sets
    descriptor = bkk::render::getDescriptor(finalImage_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[0]);
    descriptor = bkk::render::getDescriptor(gBufferRT0_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[1]);
    descriptor = bkk::render::getDescriptor(gBufferRT1_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[2]);
    descriptor = bkk::render::getDescriptor(gBufferRT2_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, presentationDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_[3]);

    //Create presentation pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gPresentationVertexShaderSource, &presentationVertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gPresentationFragmentShaderSource, &presentationFragmentShader_);
    render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled_ = false;
    pipelineDesc.depthWriteEnabled_ = false;
    pipelineDesc.vertexShader_ = presentationVertexShader_;
    pipelineDesc.fragmentShader_ = presentationFragmentShader_;
    bkk::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, fullScreenQuad_.vertexFormat_, presentationPipelineLayout_, pipelineDesc, &presentationPipeline_);

    initializeOffscreenPass(context, size);
    buildPresentationCommandBuffers();
  }

  bkk::handle_t addQuadMesh()
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
    attributes[0] = { render::vertex_attribute_t::format::VEC3, 0, sizeof(Vertex) };
    attributes[1] = { render::vertex_attribute_t::format::VEC3, offsetof(Vertex, normal), sizeof(Vertex) };

    mesh::mesh_t mesh;
    mesh::create( getRenderContext(), indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &allocator_, &mesh );
    return mesh_.add( mesh );
  }

  bkk::handle_t addMesh(const char* url )
  {
    mesh::mesh_t mesh;
    mesh::createFromFile( getRenderContext(), url, mesh::EXPORT_NORMALS, &allocator_, 0u, &mesh);
    return mesh_.add( mesh );
  }

  bkk::handle_t addMaterial( const vec3& albedo, float metallic, const vec3& F0, float roughness )
  {
    render::context_t& context = getRenderContext();

    //Create uniform buffer and descriptor set
    material_t material = {};
    material.uniforms_.albedo_ = albedo;
    material.uniforms_.metallic_ = metallic;
    material.uniforms_.F0_ = F0;
    material.uniforms_.roughness_ = roughness;
    render::gpuBufferCreate(  context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                              &material.uniforms_, sizeof(material_t::uniforms_t),
                              &allocator_, &material.ubo_);

    render::descriptor_t descriptor = render::getDescriptor(material.ubo_);
    render::descriptorSetCreate( context, descriptorPool_, materialDescriptorSetLayout_, &descriptor, &material.descriptorSet_ );
    return material_.add( material );
  }

  bkk::handle_t addObject( bkk::handle_t meshId, bkk::handle_t materialId, const maths::mat4& transform )
  {
    render::context_t& context = getRenderContext();

    bkk::handle_t transformId = transformManager_.createTransform( transform );

    //Create uniform buffer and descriptor set
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate( context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                             nullptr, sizeof(mat4),
                             &allocator_, &ubo );

    object_t object = { meshId, materialId, transformId, ubo };
    render::descriptor_t descriptor = render::getDescriptor(object.ubo_);
    render::descriptorSetCreate(context, descriptorPool_, objectDescriptorSetLayout_, &descriptor, &object.descriptorSet_ );
    return object_.add( object );
  }

  bkk::handle_t addLight(const maths::vec3& position,float radius, const maths::vec3& color )
  {
    render::context_t& context = getRenderContext();

    light_t light;

    light.uniforms_.position_ = maths::vec4(position,1.0);
    light.uniforms_.color_ = color;    
    light.uniforms_.radius_ = radius;

    //Create uniform buffer and descriptor set
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      &light.uniforms_, sizeof(light_t::uniforms_t),
      &allocator_, &light.ubo_);

    
    render::descriptor_t descriptor = render::getDescriptor(light.ubo_);
    render::descriptorSetCreate(context, descriptorPool_, lightDescriptorSetLayout_, &descriptor, &light.descriptorSet_);
    return light_.add(light);
  }

  void onResize( uint32_t width, uint32_t height )
  {
    sceneUniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix( 1.2f, (f32)width / (f32)height,0.1f,100.0f );
    buildPresentationCommandBuffers();
  }

  void render()
  {
    render::context_t& context = getRenderContext();
        
    //Update scene
    animateLights();
    transformManager_.update();
    sceneUniforms_.viewMatrix_ = camera_.view_;
    render::gpuBufferUpdate(context, (void*)&sceneUniforms_, 0u, sizeof(scene_uniforms_t), &globalsUbo_);

    //Update modelview matrices
    std::vector<object_t>& object( object_.getData() );
    for (u32 i(0); i < object.size(); ++i)
    {
      render::gpuBufferUpdate(context, transformManager_.getWorldMatrix(object[i].transform_), 0, sizeof(mat4), &object[i].ubo_ );
    }

    //Update lights position
    std::vector<light_t>& light(light_.getData());
    for (u32 i(0); i<light.size(); ++i)
    {
      render::gpuBufferUpdate(context, &light[i].uniforms_.position_, 0, sizeof(vec4), &light[i].ubo_);
    }
    
    buildAndSubmitCommandBuffer();        
    render::presentFrame( &context, &renderComplete_, 1u);
  }

  virtual void onKeyEvent(window::key_e key, bool pressed)
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

  void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos, bool buttonPressed)
  {
    if (buttonPressed)
    {
      camera_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
    }
  }

  void onQuit()
  {
   
    render::context_t& context = getRenderContext();

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
      render::gpuBufferDestroy(context, &allocator_, &materialIter.get().ubo_ );
      render::descriptorSetDestroy(context, &materialIter.get().descriptorSet_ );
      ++materialIter;
    }

    //Destroy object resources
    packed_freelist_iterator_t<object_t> objectIter = object_.begin();
    while( objectIter != object_.end() )
    {
      render::gpuBufferDestroy(context, &allocator_, &objectIter.get().ubo_ );
      render::descriptorSetDestroy(context, &objectIter.get().descriptorSet_ );
      ++objectIter;
    }

    //Destroy lights resources
    packed_freelist_iterator_t<light_t> lightIter = light_.begin();
    while (lightIter != light_.end())
    {
      render::gpuBufferDestroy(context, &allocator_, &lightIter.get().ubo_);
      render::descriptorSetDestroy(context, &lightIter.get().descriptorSet_);
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
    vkDestroySemaphore(context.device_, renderComplete_, nullptr);
  }
  
private:
  ///Helper methods
  void initializeOffscreenPass(render::context_t& context, const uvec2& size)
  {
    //Semaphore to indicate rendering has completed
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(context.device_, &semaphoreCreateInfo, nullptr, &renderComplete_);
    
    //Create offscreen render pass (GBuffer + light subpasses)
    renderPass_ = {};
    render::render_pass_t::attachment_t attachments[5];
    attachments[0].format_ = gBufferRT0_.format_;
    attachments[0].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finallLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[1].format_ = gBufferRT1_.format_;
    attachments[1].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].finallLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[2].format_ = gBufferRT2_.format_;
    attachments[2].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[2].finallLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[2].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[3].format_ = finalImage_.format_;
    attachments[3].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[3].finallLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[3].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[4].format_ = depthStencilBuffer_.format_;
    attachments[4].initialLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[4].finallLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[4].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].samples_ = VK_SAMPLE_COUNT_1_BIT;

    render::render_pass_t::subpass_t subpasses[2];
    subpasses[0].colorAttachmentIndex_.push_back(0);
    subpasses[0].colorAttachmentIndex_.push_back(1);
    subpasses[0].colorAttachmentIndex_.push_back(2);
    subpasses[0].depthStencilAttachmentIndex_ = 4;

    subpasses[1].inputAttachmentIndex_.push_back(0);
    subpasses[1].inputAttachmentIndex_.push_back(1);
    subpasses[1].inputAttachmentIndex_.push_back(2);
    subpasses[1].colorAttachmentIndex_.push_back(3);

    //Dependency chain for layout transitions
    render::render_pass_t::subpass_dependency_t dependencies[4];
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[2].srcSubpass = 0;
    dependencies[2].dstSubpass = 1;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    dependencies[3].srcSubpass = 1;
    dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[3].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[3].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[3].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[3].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    render::renderPassCreate(context, attachments, 5u, subpasses, 2u, dependencies, 4u, &renderPass_);

    //Create frame buffer
    VkImageView fbAttachment[5] = { gBufferRT0_.imageView_, gBufferRT1_.imageView_, gBufferRT2_.imageView_, finalImage_.imageView_, depthStencilBuffer_.imageView_ };
    render::frameBufferCreate(context, size.x, size.y, renderPass_, fbAttachment, &frameBuffer_);

    //Create descriptorSets layouts
    render::descriptor_binding_t binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0u, render::descriptor_t::stage::VERTEX };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &objectDescriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0u, render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &materialDescriptorSetLayout_);

    //Create gBuffer pipeline layout
    render::descriptor_set_layout_t descriptorSetLayouts[3] = { globalsDescriptorSetLayout_, objectDescriptorSetLayout_, materialDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, descriptorSetLayouts, 3u, &gBufferPipelineLayout_);

    //Create geometry pass pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gGeometryPassVertexShaderSource, &gBuffervertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gGeometryPassFragmentShaderSource, &gBufferfragmentShader_);
    bkk::render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(3);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.blendState_[1].colorWriteMask = 0xF;
    pipelineDesc.blendState_[1].blendEnable = VK_FALSE;
    pipelineDesc.blendState_[2].colorWriteMask = 0xF;
    pipelineDesc.blendState_[2].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled_ = true;
    pipelineDesc.depthWriteEnabled_ = true;
    pipelineDesc.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDesc.vertexShader_ = gBuffervertexShader_;
    pipelineDesc.fragmentShader_ = gBufferfragmentShader_;
    render::graphicsPipelineCreate(context, renderPass_.handle_, 0u, vertexFormat_, gBufferPipelineLayout_, pipelineDesc, &gBufferPipeline_);

    //Create light pass descriptorSet layouts
    render::descriptor_binding_t bindings[3];
    bindings[0] = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage::FRAGMENT };
    bindings[1] = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 1, render::descriptor_t::stage::FRAGMENT };
    bindings[2] = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 2, render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, bindings, 3u, &lightPassTexturesDescriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0, render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &lightDescriptorSetLayout_);

    //Create descriptor sets for light pass (GBuffer textures)
    render::descriptor_t descriptors[3];
    descriptors[0] = render::getDescriptor(gBufferRT0_);
    descriptors[1] = render::getDescriptor(gBufferRT1_);
    descriptors[2] = render::getDescriptor(gBufferRT2_);
    render::descriptorSetCreate(context, descriptorPool_, lightPassTexturesDescriptorSetLayout_, descriptors, &lightPassTexturesDescriptorSet_);

    //Create light pass pipeline layout
    render::descriptor_set_layout_t lightPassDescriptorSetLayouts[3] = { globalsDescriptorSetLayout_, lightPassTexturesDescriptorSetLayout_, lightDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, lightPassDescriptorSetLayouts, 3u, &lightPipelineLayout_);

    //Create light pass pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gLightPassVertexShaderSource, &lightVertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gLightPassFragmentShaderSource, &lightFragmentShader_);
    bkk::render::graphics_pipeline_t::description_t lightPipelineDesc = {};
    lightPipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    lightPipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    lightPipelineDesc.blendState_.resize(1);
    lightPipelineDesc.blendState_[0].colorWriteMask = 0xF;
    lightPipelineDesc.blendState_[0].blendEnable = VK_TRUE;
    lightPipelineDesc.blendState_[0].colorBlendOp = VK_BLEND_OP_ADD;
    lightPipelineDesc.blendState_[0].alphaBlendOp = VK_BLEND_OP_ADD;
    lightPipelineDesc.blendState_[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.blendState_[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.blendState_[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.blendState_[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    lightPipelineDesc.cullMode_ = VK_CULL_MODE_FRONT_BIT;
    lightPipelineDesc.depthTestEnabled_ = false;
    lightPipelineDesc.depthWriteEnabled_ = false;
    lightPipelineDesc.vertexShader_ = lightVertexShader_;
    lightPipelineDesc.fragmentShader_ = lightFragmentShader_;
    render::graphicsPipelineCreate(context, renderPass_.handle_, 1u, sphereMesh_.vertexFormat_, lightPipelineLayout_, lightPipelineDesc, &lightPipeline_);
  }

  void buildAndSubmitCommandBuffer()
  {
    render::context_t& context = getRenderContext();

    if (commandBuffer_.handle_ == VK_NULL_HANDLE)
    {
      render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, &renderComplete_, 1u, render::command_buffer_t::GRAPHICS, &commandBuffer_);
    }

    VkClearValue clearValues[5];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[4].depthStencil = { 1.0f,0 };

    render::commandBufferBegin(context, &frameBuffer_, clearValues, 5u, commandBuffer_);
    {

      //GBuffer pass
      bkk::render::graphicsPipelineBind(commandBuffer_.handle_, gBufferPipeline_);
      render::descriptor_set_t descriptorSets[3];
      descriptorSets[0] = globalsDescriptorSet_;
      packed_freelist_iterator_t<object_t> objectIter = object_.begin();
      while (objectIter != object_.end())
      {
        descriptorSets[1] = objectIter.get().descriptorSet_;
        descriptorSets[2] = material_.get(objectIter.get().material_)->descriptorSet_;
        bkk::render::descriptorSetBindForGraphics(commandBuffer_.handle_, gBufferPipelineLayout_, 0, descriptorSets, 3u);
        mesh::mesh_t* mesh = mesh_.get(objectIter.get().mesh_);
        mesh::draw(commandBuffer_.handle_, *mesh);
        ++objectIter;
      }

      bkk::render::commandBufferNextSubpass(commandBuffer_);

      //Light pass      
      bkk::render::graphicsPipelineBind(commandBuffer_.handle_, lightPipeline_);
      descriptorSets[1] = lightPassTexturesDescriptorSet_;
      packed_freelist_iterator_t<light_t> lightIter = light_.begin();
      while (lightIter != light_.end())
      {
        descriptorSets[2] = lightIter.get().descriptorSet_;
        bkk::render::descriptorSetBindForGraphics(commandBuffer_.handle_, lightPipelineLayout_, 0u, descriptorSets, 3u);
        mesh::draw(commandBuffer_.handle_, sphereMesh_);
        ++lightIter;
      }
    }

    render::commandBufferEnd(commandBuffer_);
    render::commandBufferSubmit(context, commandBuffer_);
  }

  void buildPresentationCommandBuffers()
  {
    render::context_t& context = getRenderContext();

    const VkCommandBuffer* commandBuffers;
    uint32_t count = bkk::render::getPresentationCommandBuffers(context, &commandBuffers);
    for (uint32_t i(0); i<count; ++i)
    {
      bkk::render::beginPresentationCommandBuffer(context, i, nullptr);
      bkk::render::graphicsPipelineBind(commandBuffers[i], presentationPipeline_);
      bkk::render::descriptorSetBindForGraphics(commandBuffers[i], presentationPipelineLayout_, 0u, &presentationDescriptorSet_[currentPresentationDescriptorSet_], 1u);
      bkk::mesh::draw(commandBuffers[i], fullScreenQuad_);
      bkk::render::endPresentationCommandBuffer(context, i);
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
    std::vector<light_t>& lights = light_.getData();
    for (u32 i(0); i < lights.size(); ++i)
    {
      float t = totalTime + i* 5.0f / lights.size();
      int baseFrame = (int)t;
      float f = t - baseFrame;

      vec3 p0 = light_path[(baseFrame + 0) % 5];
      vec3 p1 = light_path[(baseFrame + 1) % 5];
      vec3 p2 = light_path[(baseFrame + 2) % 5];
      vec3 p3 = light_path[(baseFrame + 3) % 5];

      lights[i].uniforms_.position_ = vec4(maths::cubicInterpolation(p0, p1, p2, p3, f), 1.0);
    }
  }

private:
  ///Member variables
  bkk::transform_manager_t transformManager_;
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
  
  free_camera_t camera_;
  bool bAnimateLights_;
};


int main()
{
  deferred_shading_sample_t scene;
  
  //Materials  
  bkk::handle_t wall = scene.addMaterial(vec3(0.5f, 0.5f, 0.5f), 0.0f, vec3(0.004f, 0.004f, 0.004f),0.7f);
  bkk::handle_t redWall = scene.addMaterial( vec3(0.5f,0.0f,0.0f), 0.0f, vec3(0.04f, 0.04f, 0.04f), 0.7f);
  bkk::handle_t greenWall = scene.addMaterial( vec3(0.0f,0.5f,0.0f), 0.0f, vec3(0.004f, 0.004f, 0.004f), 0.7f);
  bkk::handle_t gold = scene.addMaterial(vec3(0.0f, 0.0f, 0.0f), 1.0f, vec3(1.000f, 0.766f, 0.336f), 0.3f);
  bkk::handle_t silver = scene.addMaterial(vec3(0.0f, 0.0f, 0.0f), 1.0f, vec3(0.972f, 0.960f, 0.915f), 0.3f);
  bkk::handle_t copper = scene.addMaterial(vec3(0.0f, 0.0f, 0.0f), 1.0f, vec3(1.0f, 0.437f, 0.338f), 0.3f);
  bkk::handle_t redPlastic = scene.addMaterial(vec3(1.0f, 0.0f, 0.0f), 0.0f, vec3(0.05f, 0.05f, 0.05f), 0.4f);
  bkk::handle_t bluePlastic = scene.addMaterial(vec3(0.0f, 0.0f, 1.0f), 0.0f, vec3(0.05f, 0.05f, 0.05f), 0.4f);

  //Meshes
  bkk::handle_t bunny = scene.addMesh( "../resources/bunny.ply" );
  bkk::handle_t dragon = scene.addMesh("../resources/dragon.obj");
  bkk::handle_t buddha = scene.addMesh("../resources/buddha.obj");
  bkk::handle_t armadillo = scene.addMesh("../resources/armadillo.obj");
  bkk::handle_t teapot = scene.addMesh("../resources/teapot.obj");
  bkk::handle_t quad = scene.addQuadMesh();


  //Objects
  scene.addObject(quad, wall, maths::computeTransform( maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::QUAT_UNIT));
  scene.addObject(quad, redWall, maths::computeTransform(maths::vec3(-5.0f, 4.0f, 0.0f), maths::vec3(4.0f, 5.0f, 5.0f), maths::quaternionFromAxisAngle(vec3(0, 0, 1), maths::degreeToRadian(90.0f))));
  scene.addObject(quad, greenWall, maths::computeTransform(maths::vec3(5.0f, 4.0f, 0.0f), maths::vec3(4.0f, 5.0f, 5.0f), maths::quaternionFromAxisAngle(vec3(0, 0, 1), maths::degreeToRadian(-90.0f))));
  scene.addObject(quad, wall, maths::computeTransform(maths::vec3(0.0f, 4.0f, -5.0f), maths::vec3(5.0f, 5.0f, 4.0f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(-90.0f))));
  scene.addObject(quad, wall, maths::computeTransform(maths::vec3(0.0f, 8.0f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(180.0f))));
  scene.addObject(bunny, bluePlastic, maths::computeTransform(maths::vec3(0.0f, -0.4f, 0.5f), maths::vec3(12.0f, 12.0f, 12.0f), maths::QUAT_UNIT));
  scene.addObject(dragon, silver, maths::computeTransform(maths::vec3(2.5f, 1.2f, 4.0f), maths::vec3(1.7f, 1.7f, 1.7f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(90.0f))*maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(-10.0f))));
  scene.addObject(buddha, copper, maths::computeTransform(maths::vec3(-3.0f, 1.4f, 4.0f), maths::vec3(1.5f, 1.5f, 1.5f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(90.0f))*maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(-30.0f))));
  scene.addObject(armadillo, redPlastic, maths::computeTransform(maths::vec3(-3.0f, 1.1f, -1.5f), maths::vec3(1.0f, 1.0f, 1.0f), maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(180.0f))));
  scene.addObject(teapot, gold, maths::computeTransform(maths::vec3(2.0f, 0.0f, -3.0f), maths::vec3(0.5f, 0.5f, 0.5f), maths::QUAT_UNIT));

  //Lights
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.5f,0.0f,0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 0.5f, 0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 0.0f, 0.5f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.5f, 0.0f, 0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 0.5f, 0.0f));
  scene.addLight(vec3(0.0f, 0.0f, 0.0f), 10.0f, vec3(0.0f, 0.0f, 0.5f));
  
  scene.loop();
  return 0;
}