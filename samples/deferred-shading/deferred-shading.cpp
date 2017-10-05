
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
#include "timer.h"
#include "../utility.h"
#include "transform-manager.h"
#include "packed-freelist.h"
#include <array>

using namespace bkk;
using namespace maths;

static const char* gGeometryPassVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec3 aNormal;\n \
  layout (set = 0, binding = 0) uniform SCENE\n \
  {\n \
  mat4 view;\n \
  mat4 projection;\n \
  mat4 projectionInverse;\n \
  vec2 imageSize;\n \
  }scene;\n \
  layout(set = 1, binding = 1) uniform MODEL\n \
  {\n \
    mat4 value;\n \
  }model;\n \
  out vec3 normalViewSpace;\n \
  void main(void)\n \
  {\n \
    mat4 modelView = scene.view * model.value;\n \
    gl_Position = scene.projection * modelView * vec4(aPosition,1.0);\n \
    normalViewSpace = normalize((modelView * vec4(aNormal,0.0)).xyz);\n \
  }\n"
};


static const char* gGeometryPassFragmentShaderSource = {
  "#version 440 core\n \
  layout(set = 2, binding = 2) uniform MATERIAL\n \
  {\n \
    vec4 albedo;\n \
    vec3 F0;\n \
    float roughness;\n \
  }material;\n \
  layout(location = 0) out vec4 RT0;\n \
  layout(location = 1) out vec4 RT1;\n \
  layout(location = 2) out vec4 RT2;\n \
  in vec3 normalViewSpace;\n \
  in vec3 positionViewSpace;\n \
  void main(void)\n \
  {\n \
    RT0 = vec4(material.albedo.xyz, gl_FragCoord.z);\n \
    RT1 = vec4(normalize(normalViewSpace), material.roughness );\n \
    RT2 = vec4(material.F0, 1.0);\n \
  }\n"
};

static const char* gLightPassVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec3 aNormal;\n \
  layout(set = 0, binding = 0) uniform SCENE\n \
  {\n \
    mat4 view;\n \
    mat4 projection;\n \
    mat4 projectionInverse;\n \
    vec2 imageSize;\n \
  }scene;\n \
  layout (set = 2, binding = 0) uniform LIGHT\n \
  {\n \
   vec4 position;\n \
   vec3 color;\n \
   float radius;\n \
  }light;\n \
  void main(void)\n \
  {\n \
    mat4 viewProjection = scene.projection * scene.view;\n \
    gl_Position = viewProjection * vec4( aPosition*light.radius+light.position.xyz, 1.0 );\n\
  }\n"
};


static const char* gLightPassFragmentShaderSource = {
  "#version 440 core\n \
  layout(set = 0, binding = 0) uniform SCENE\n \
  {\n \
    mat4 view;\n \
    mat4 projection;\n \
    mat4 projectionInverse;\n \
    vec2 imageSize;\n \
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
  layout(location = 0) out vec4 result;\n \
  vec3 ViewSpacePositionFromDepth(vec2 uv, float depth)\n\
  {\n\
    vec3 clipSpacePosition = vec3(uv, depth) * 2.0 - vec3(1.0);\n\
    vec4 viewSpacePosition = scene.projectionInverse * vec4(clipSpacePosition,1.0);\n\
    return(viewSpacePosition.xyz / viewSpacePosition.w);\n\
  }\n\
  void main(void)\n \
  {\n \
    vec2 uv = gl_FragCoord.xy / scene.imageSize;\n\
    vec4 albedo = texture(RT0, uv);\n \
    float depth = albedo.w;\n\
    //float n = 0.1;\n\
    //float f = 100.0;\n\
    //float linearDepth = (2 * n) / (f + n - depth * (f - n));\n\
    vec3 GBufferPosition = ViewSpacePositionFromDepth( uv,depth );\n\
    vec3 lightPositionViewSpace = (scene.view * light.position).xyz;\n\
    vec3 lightVector = lightPositionViewSpace-GBufferPosition;\n\
    vec3 GBufferNormal = normalize( texture(RT1, uv).xyz );\n \
    float attenuation = clamp(  ( light.radius - length(lightVector) ) / light.radius, 0.0, 1.0);\n\
    float NdotL =  attenuation * max( 0.0, dot( GBufferNormal, -normalize(lightVector) ) );\n \
    result =  attenuation * ( NdotL * vec4(light.color,1.0) * vec4(albedo.xyz,1.0) );\n \
  }\n"
};

static const char* gVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec2 aTexCoord;\n \
  out vec2 uv;\n \
  void main(void)\n \
  {\n \
    gl_Position = vec4(aPosition,1.0);\n \
    uv = aTexCoord;\n \
  }\n"
};

static const char* gFragmentShaderSource = {
  "#version 440 core\n \
  in vec2 uv;\n  \
  layout (binding = 0) uniform sampler2D uTexture;\n \
  layout(location = 0) out vec4 color;\n \
  void main(void)\n \
  {\n \
    color = texture(uTexture, uv);\n \
  }\n"
};

struct scene_t
{
  struct light_t
  {
    struct light_uniforms_t
    {
      maths::vec4 position_;
      maths::vec3 color_;
      float radius_;
    };

    light_uniforms_t uniforms_;
    render::gpu_buffer_t ubo_;
    render::descriptor_set_t descriptorSet_;
  };

  struct material_t
  {
    struct material_uniforms_t
    {
      vec4 albedo_;
      vec3 F0_;
      float roughness_;
    };

    material_uniforms_t uniforms_;
    render::gpu_buffer_t ubo_;
    render::descriptor_set_t descriptorSet_;
  };

  struct instance_t
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
    vec2 imageSize_;
  };

  bkk::handle_t AddQuadMesh()
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
    mesh::create( *context_, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &mesh, &allocator_ );
    return mesh_.add( mesh );
  }

  bkk::handle_t AddMesh(const char* url )
  {
    mesh::mesh_t mesh;
    mesh::createFromFile( *context_, url, &mesh, &allocator_ );
    return mesh_.add( mesh );
  }

  bkk::handle_t AddMaterial( const vec3& albedo, const vec3& F0, float roughness )
  {
    //Create uniform buffer and descriptor set
    material_t material = {};
    material.uniforms_.albedo_ = vec4(albedo,1.0);
    material.uniforms_.F0_ = F0;
    material.uniforms_.roughness_ = roughness;
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate(  *context_, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                              &material.uniforms_, sizeof(material_t::material_uniforms_t),
                              &allocator_, &ubo );

    material.ubo_ = ubo;
    render::descriptor_t descriptor = render::getDescriptor(material.ubo_);
    render::descriptorSetCreate( *context_, descriptorPool_, materialDescriptorSetLayout_, &descriptor, &material.descriptorSet_ );
    return material_.add( material );
  }

  bkk::handle_t AddInstance( bkk::handle_t meshId, bkk::handle_t materialId, const maths::mat4& transform )
  {
    bkk::handle_t transformId = transformManager_.createTransform( transform );

    //Create uniform buffer and descriptor set
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate( *context_, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                             nullptr, sizeof(mat4),
                             &allocator_, &ubo );

    instance_t instance = { meshId, materialId, transformId, ubo };
    render::descriptor_t descriptor = render::getDescriptor(instance.ubo_);
    render::descriptorSetCreate(*context_, descriptorPool_, instanceDescriptorSetLayout_, &descriptor, &instance.descriptorSet_ );
    return instance_.add( instance );
  }

  bkk::handle_t AddLight(const maths::vec3& position,float radius, const maths::vec3& color )
  {
    light_t light;

    maths::mat4 transform;
    transform.setIdentity();
    transform.setScale(1.0, 1.0,1.0);
    transform.setTranslation(position);

    light.uniforms_.position_ = maths::vec4(position,1.0);
    light.uniforms_.color_ = color;    
    light.uniforms_.radius_ = radius;

    //Create uniform buffer and descriptor set
    
    render::gpuBufferCreate(*context_, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      &light.uniforms_, sizeof(light_t::light_uniforms_t),
      &allocator_, &light.ubo_);

    
    render::descriptor_t descriptor = render::getDescriptor(light.ubo_);
    render::descriptorSetCreate(*context_, descriptorPool_, lightDescriptorSetLayout_, &descriptor, &light.descriptorSet_);
    return light_.add(light);
  }

  void setLightPosition(bkk::handle_t light, const vec3& position)
  {
    light_.get(light)->uniforms_.position_ = vec4(position,1.0);
  }

  void InitializeOffscreenPasses(render::context_t& context, const uvec2& size)
  {
    ////Geometry pass

    //Semaphore to indicate geometr pass pass has completed
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(context.device_, &semaphoreCreateInfo, nullptr, &geometryRenderComplete_);


    //Create frame buffer attachments (Color and DepthStencil)
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &gBufferRT0_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &gBufferRT0_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &gBufferRT1_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &gBufferRT1_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &gBufferRT2_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &gBufferRT2_);
    render::depthStencilBufferCreate(context, size.x, size.y, &depthStencilBuffer_);


    //Create render pass
    geometryPass_ = {};
    render::render_pass_t::attachment_t attachments[4];
    attachments[0].format_ = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachments[0].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[1].format_ = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachments[1].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[2].format_ = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachments[2].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[2].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[3].format_ = depthStencilBuffer_.format_;
    attachments[3].initialLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[3].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[3].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].samples_ = VK_SAMPLE_COUNT_1_BIT;

    render::renderPassCreate(context, 4u, attachments, 0, 0, &geometryPass_);

    //Create frame buffer
    VkImageView fbAttachment[4] = { gBufferRT0_.imageView_, gBufferRT1_.imageView_, gBufferRT2_.imageView_, depthStencilBuffer_.imageView_ };
    render::frameBufferCreate(context, size.x, size.y, geometryPass_, fbAttachment, &geometryFrameBuffer_);
    
    //Create descriptorSets layouts
    render::descriptor_binding_t binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0, render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &globalsDescriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 1, render::descriptor_t::stage::VERTEX };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &instanceDescriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 2, render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &materialDescriptorSetLayout_);

    //Create pipeline layout
    render::descriptor_set_layout_t descriptorSetLayouts[3] = { globalsDescriptorSetLayout_, instanceDescriptorSetLayout_, materialDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, 3, descriptorSetLayouts, &gBufferPipelineLayout_);

    //Create vertex format (position + normal)
    uint32_t vertexSize = 2 * sizeof(maths::vec3);
    render::vertex_attribute_t attributes[2] = { { render::vertex_attribute_t::format::VEC3, 0, vertexSize }, { render::vertex_attribute_t::format::VEC3, sizeof(maths::vec3), vertexSize } };
    render::vertexFormatCreate(attributes, 2u, &vertexFormat_);

    //Create geometry pass pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gGeometryPassVertexShaderSource, &gBuffervertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gGeometryPassFragmentShaderSource, &gBufferfragmentShader_);
    bkk::render::graphics_pipeline_t::description_t pipelineDesc;
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
    render::graphicsPipelineCreate(context, geometryPass_.handle_, vertexFormat_, gBufferPipelineLayout_, pipelineDesc, &gBufferPipeline_);

    ////Light pass

    //Semaphore to indicate light pass has completed
    vkCreateSemaphore(context.device_, &semaphoreCreateInfo, nullptr, &renderComplete_);

    //Create frame buffer attachment
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &finalImage_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &finalImage_);

    //Create render pass
    lightPass_ = {};
    render::render_pass_t::attachment_t attachment;
    attachment.format_ = VK_FORMAT_B8G8R8A8_UNORM;
    attachment.initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachment.storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.samples_ = VK_SAMPLE_COUNT_1_BIT;
    render::renderPassCreate(context, 1u, &attachment, 0, 0, &lightPass_);

    //Create frame buffer
    render::frameBufferCreate(context, size.x, size.y, lightPass_, &finalImage_.imageView_, &lightFrameBuffer_);

    //Create descriptorSet layouts
    render::descriptor_binding_t bindings[4];
    bindings[0] = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage::FRAGMENT };
    bindings[1] = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 1, render::descriptor_t::stage::FRAGMENT };
    bindings[2] = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 2, render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, 3u, bindings, &lightPassTexturesDescriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0, render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &lightDescriptorSetLayout_);

    //Create pipeline layout
    render::descriptor_set_layout_t lightPassDescriptorSetLayouts[3] = { globalsDescriptorSetLayout_, lightPassTexturesDescriptorSetLayout_, lightDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, 3u, lightPassDescriptorSetLayouts, &lightPipelineLayout_);

    //Create light pass pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gLightPassVertexShaderSource, &lightVertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gLightPassFragmentShaderSource, &lightFragmentShader_);
    bkk::render::graphics_pipeline_t::description_t lightPipelineDesc;
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
    render::graphicsPipelineCreate(context, lightPass_.handle_, vertexFormat_, lightPipelineLayout_, lightPipelineDesc, &lightPipeline_);
  }

  void Initialize( render::context_t& context, const uvec2& size )
  {   
    context_ = &context;

    //Create allocator for uniform buffers and meshes
    render::gpuAllocatorCreate(context, 100 * 1024 * 1024, 0xFFFF, render::gpu_memory_type_e::HOST_VISIBLE_COHERENT, &allocator_);

    //Create descriptor pool
    render::descriptorPoolCreate(context, 100u, 100u, 100u, 0u, 0u, &descriptorPool_);

    //Initialize off-screen render passes
    InitializeOffscreenPasses(context, size);

    //Create scene uniform buffer
    camera_.position_ = vec3(0.0f, 2.5f, 8.0f);
    camera_.Update();
    uniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix(1.5f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
    computeInverse(uniforms_.projectionMatrix_, uniforms_.projectionInverseMatrix_);
    uniforms_.viewMatrix_ = camera_.view_;
    uniforms_.imageSize_ = vec2(size.x, size.y);
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      (void*)&uniforms_, sizeof(scene_uniforms_t),
      &allocator_, &ubo_);

    //Create global descriptor set (Scene uniforms)    
    render::descriptor_t descriptor = render::getDescriptor(ubo_);
    render::descriptorSetCreate(context, descriptorPool_, globalsDescriptorSetLayout_, &descriptor, &globalsDescriptorSet_);


    //Create descriptor sets for light pass
    render::descriptor_t descriptors[3];
    descriptors[0] = render::getDescriptor(gBufferRT0_);
    descriptors[1] = render::getDescriptor(gBufferRT1_);
    descriptors[2] = render::getDescriptor(gBufferRT2_);
    render::descriptorSetCreate(context, descriptorPool_, lightPassTexturesDescriptorSetLayout_, descriptors, &lightPassTexturesDescriptorSet_);


    //Initialize on-screen pass (Presents image genereated by offscreen pass) 
    fullScreenQuad_ = sample_utils::FullScreenQuad(context);
    mesh::createFromFile(context, "../resources/sphere.obj", &sphereMesh_ );

    //Descriptor set layout and pipeline layout
    render::descriptor_binding_t binding = { bkk::render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, bkk::render::descriptor_t::stage::FRAGMENT };
    bkk::render::descriptor_set_layout_t descriptorSetLayout;
    bkk::render::descriptorSetLayoutCreate(context, 1u, &binding, &descriptorSetLayout);
    bkk::render::pipelineLayoutCreate(context, 1u, &descriptorSetLayout, &pipelineLayout_);

    //Presentation descriptor sets
    descriptor = bkk::render::getDescriptor(finalImage_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[0]);
    descriptor = bkk::render::getDescriptor(gBufferRT0_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[1]);
    descriptor = bkk::render::getDescriptor(gBufferRT1_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[2]);
    descriptor = bkk::render::getDescriptor(gBufferRT2_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[3]);
    


    //Create presentation pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);
    render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled_ = false;
    pipelineDesc.depthWriteEnabled_ = false;
    pipelineDesc.vertexShader_ = vertexShader_;
    pipelineDesc.fragmentShader_ = fragmentShader_;
    bkk::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, fullScreenQuad_.vertexFormat_, pipelineLayout_, pipelineDesc, &pipeline_);
  }

  void Resize( uint32_t width, uint32_t height )
  {
    uniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix( 1.2f, (f32)width / (f32)height,0.1f,100.0f );
    render::swapchainResize( context_, width, height );
    BuildCommandBuffers();
  }

  void Render()
  {
    //Update scene
    transformManager_.update();
    uniforms_.viewMatrix_ = camera_.view_;
    render::gpuBufferUpdate(*context_, (void*)&uniforms_, 0u, sizeof(scene_uniforms_t), &ubo_ );

    //Update modelview matrices
    std::vector<instance_t>& instance( instance_.getData() );
    for( u32 i(0); i<instance.size(); ++i )
    {
      maths::mat4* modelTx = transformManager_.getWorldMatrix( instance[i].transform_ );
      render::gpuBufferUpdate(*context_, modelTx, 0, sizeof(mat4), &instance[i].ubo_ );
    }

    std::vector<light_t>& light(light_.getData());
    for (u32 i(0); i<light.size(); ++i)
    {
      maths::vec4 position = light[i].uniforms_.position_;
      render::gpuBufferUpdate(*context_, &position, 0, sizeof(vec4), &light[i].ubo_);
    }


    BuildCommandBuffers();
    render::commandBufferSubmit(*context_, geometryCommandBuffer_);
    render::commandBufferSubmit(*context_, lightCommandBuffer_);
    render::presentNextImage( context_, 1u, &renderComplete_);
  }

  void BuildCommandBuffers()
  {
    //Geometry command buffer
    if (geometryCommandBuffer_.handle_ == VK_NULL_HANDLE)
    {
      render::commandBufferCreate(*context_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, nullptr, nullptr, 1, &geometryRenderComplete_, render::command_buffer_t::GRAPHICS, &geometryCommandBuffer_);
    }
    
    VkClearValue clearValues[4];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    clearValues[3].depthStencil = { 1.0f,0 };

    render::commandBufferBegin(*context_, &geometryFrameBuffer_, 4u, clearValues, geometryCommandBuffer_);
    {
      bkk::render::graphicsPipelineBind(geometryCommandBuffer_.handle_, gBufferPipeline_);

      packed_freelist_iterator_t<instance_t> instanceIter = instance_.begin();
      while (instanceIter != instance_.end())
      {
        //Bind descriptor set
        std::vector<render::descriptor_set_t> descriptorSets(3);
        descriptorSets[0] = globalsDescriptorSet_;
        descriptorSets[1] = instanceIter.get().descriptorSet_;
        descriptorSets[2] = material_.get(instanceIter.get().material_)->descriptorSet_;
        bkk::render::descriptorSetBindForGraphics(geometryCommandBuffer_.handle_, gBufferPipelineLayout_, 0, descriptorSets.data(), (uint32_t)descriptorSets.size());

        //Draw call
        mesh::mesh_t* mesh = mesh_.get(instanceIter.get().mesh_);
        mesh::draw(geometryCommandBuffer_.handle_, *mesh);
        ++instanceIter;
      }
    }
    render::commandBufferEnd(*context_, geometryCommandBuffer_);


    //Light command buffer
    if (lightCommandBuffer_.handle_ == VK_NULL_HANDLE)
    {
      VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      render::commandBufferCreate(*context_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1u, &geometryRenderComplete_, &waitStage, 1u, &renderComplete_, render::command_buffer_t::GRAPHICS, &lightCommandBuffer_);
    }

    render::commandBufferBegin(*context_, &lightFrameBuffer_, 1u, clearValues, lightCommandBuffer_);
    {
      bkk::render::graphicsPipelineBind(lightCommandBuffer_.handle_, lightPipeline_);

      packed_freelist_iterator_t<light_t> lightIter = light_.begin();
      while (lightIter != light_.end())
      {
        //Bind descriptor set
        std::vector<render::descriptor_set_t> descriptorSets(3);
        descriptorSets[0] = globalsDescriptorSet_;
        descriptorSets[1] = lightPassTexturesDescriptorSet_;
        descriptorSets[2] = lightIter.get().descriptorSet_;
        bkk::render::descriptorSetBindForGraphics(lightCommandBuffer_.handle_,lightPipelineLayout_, 0u, descriptorSets.data(), (uint32_t)descriptorSets.size());

        //Draw call       
        mesh::draw(lightCommandBuffer_.handle_, sphereMesh_ );
        ++lightIter;
      }
    }
    render::commandBufferEnd(*context_, lightCommandBuffer_);

    //Presentation command buffers
    for (unsigned i(0); i<3; ++i)
    {
      VkCommandBuffer cmdBuffer = bkk::render::beginPresentationCommandBuffer(*context_, i, nullptr);
      bkk::render::graphicsPipelineBind(cmdBuffer, pipeline_);
      bkk::render::descriptorSetBindForGraphics(cmdBuffer, pipelineLayout_, 0u, &descriptorSet_[currentDescriptorSet_], 1u);
      bkk::mesh::draw(cmdBuffer, fullScreenQuad_);
      bkk::render::endPresentationCommandBuffer(*context_, i);
    }
  }


  void OnKeyEvent(window::key_e key, bool pressed, scene_t& scene)
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
      {
        currentDescriptorSet_ = 0;
        break;
      }
      case window::key_e::KEY_2:
      {
        currentDescriptorSet_ = 1;
        break;
      }
      case window::key_e::KEY_3:
      {
        currentDescriptorSet_ = 2;
        break;
      }
      case window::key_e::KEY_4:
      {
        currentDescriptorSet_ = 3;
        break;
      }
      default:
        break;
      }
    }
  }

  void OnMouseButton( bool pressed, uint32_t x, uint32_t y )
  {
    mouseButtonPressed_ = pressed;
    mousePosition_.x = (f32)x;
    mousePosition_.y = (f32)y;
  }

  void OnMouseMove(uint32_t x, uint32_t y)
  {
    if (mouseButtonPressed_)
    {
      f32 angleY = ((f32)x - mousePosition_.x) * 0.01f;
      f32 angleX = ((f32)y - mousePosition_.y) * 0.01f;
      mousePosition_.x = (f32)x;
      mousePosition_.y = (f32)y;
      camera_.Rotate(angleX, angleY);
    }
  }

  void Destroy()
  {
    //Destroy meshes
    packed_freelist_iterator_t<mesh::mesh_t> meshIter = mesh_.begin();
    while( meshIter != mesh_.end() )
    {
      mesh::destroy( *context_, &meshIter.get(), &allocator_ );
      ++meshIter;
    }

    //Destroy material resources
    packed_freelist_iterator_t<material_t> materialIter = material_.begin();
    while( materialIter != material_.end() )
    {
      render::gpuBufferDestroy(*context_, &materialIter.get().ubo_, &allocator_ );
      render::descriptorSetDestroy(*context_, &materialIter.get().descriptorSet_ );
      ++materialIter;
    }

    //Destroy instance resources
    packed_freelist_iterator_t<instance_t> instanceIter = instance_.begin();
    while( instanceIter != instance_.end() )
    {
      render::gpuBufferDestroy(*context_, &instanceIter.get().ubo_, &allocator_ );
      render::descriptorSetDestroy(*context_, &instanceIter.get().descriptorSet_ );
      ++instanceIter;
    }

    //Destroy global resources
    render::shaderDestroy(*context_, &gBuffervertexShader_);
    render::shaderDestroy(*context_, &gBufferfragmentShader_);
    render::graphicsPipelineDestroy(*context_, &gBufferPipeline_);
    render::descriptorSetDestroy(*context_, &globalsDescriptorSet_);
    render::descriptorPoolDestroy(*context_, &descriptorPool_ );
    render::pipelineLayoutDestroy(*context_, &gBufferPipelineLayout_);
    render::gpuBufferDestroy(*context_, &ubo_, &allocator_ );
    render::gpuAllocatorDestroy(*context_, &allocator_ );
    render::textureDestroy(*context_, &gBufferRT0_);
    render::textureDestroy(*context_, &gBufferRT1_);
    render::textureDestroy(*context_, &gBufferRT2_);
    render::depthStencilBufferDestroy(*context_, &depthStencilBuffer_);
    render::commandBufferDestroy(*context_, &geometryCommandBuffer_);
    render::graphicsPipelineDestroy(*context_, &pipeline_);
    render::descriptorSetDestroy(*context_, &descriptorSet_[0]);
    render::descriptorSetDestroy(*context_, &descriptorSet_[1]);
    render::descriptorSetDestroy(*context_, &descriptorSet_[2]);
    render::shaderDestroy(*context_, &vertexShader_);
    render::shaderDestroy(*context_, &fragmentShader_);
    render::pipelineLayoutDestroy(*context_, &pipelineLayout_);
    mesh::destroy(*context_, &fullScreenQuad_);
  }
  
private:
  
  render::context_t* context_;
  bkk::transform_manager_t transformManager_;
  render::descriptor_pool_t descriptorPool_;

  
  render::descriptor_set_layout_t globalsDescriptorSetLayout_;
  render::descriptor_set_t globalsDescriptorSet_;

  render::descriptor_set_layout_t materialDescriptorSetLayout_;
  render::descriptor_set_layout_t instanceDescriptorSetLayout_;
  render::descriptor_set_layout_t lightDescriptorSetLayout_;

    
  render::gpu_memory_allocator_t allocator_;
  render::gpu_buffer_t ubo_;
  
  render::vertex_format_t vertexFormat_;
  render::pipeline_layout_t gBufferPipelineLayout_;
  render::graphics_pipeline_t gBufferPipeline_;
  
  render::shader_t gBuffervertexShader_;
  render::shader_t gBufferfragmentShader_;
  scene_uniforms_t uniforms_;  

  VkSemaphore geometryRenderComplete_;
  render::command_buffer_t geometryCommandBuffer_;
  render::render_pass_t geometryPass_;
  render::texture_t gBufferRT0_;
  render::texture_t gBufferRT1_;
  render::texture_t gBufferRT2_;
  render::depth_stencil_buffer_t depthStencilBuffer_;
  render::frame_buffer_t geometryFrameBuffer_;

  VkSemaphore renderComplete_;
  render::command_buffer_t lightCommandBuffer_;
  render::render_pass_t lightPass_;
  render::descriptor_set_layout_t lightPassGlobalDescriptorSetLayout_;
  render::descriptor_set_layout_t lightPassTexturesDescriptorSetLayout_;  
  render::descriptor_set_t lightPassTexturesDescriptorSet_;
  

  render::pipeline_layout_t lightPipelineLayout_;
  render::graphics_pipeline_t lightPipeline_;
  render::shader_t lightVertexShader_;
  render::shader_t lightFragmentShader_;
  render::texture_t finalImage_;
  render::frame_buffer_t lightFrameBuffer_;
  mesh::mesh_t sphereMesh_;

  packed_freelist_t<material_t> material_;
  packed_freelist_t<mesh::mesh_t> mesh_;
  packed_freelist_t<instance_t> instance_;
  packed_freelist_t<light_t> light_;

  

  render::graphics_pipeline_t pipeline_;
  render::pipeline_layout_t pipelineLayout_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;

  uint32_t currentDescriptorSet_ = 0u;
  render::descriptor_set_t descriptorSet_[4];
  mesh::mesh_t fullScreenQuad_;

  sample_utils::free_camera_t camera_;
  maths::vec2 mousePosition_ = vec2(0.0f, 0.0f);
  bool mouseButtonPressed_ = false;  
};

void AnimateLights(f32 timeDelta, std::vector< bkk::handle_t >& lights, scene_t& scene )
{
  static const vec3 light_path[] =
  {
    vec3(-3.0f,  0.0f, 5.0f),
    vec3(-3.0f,  1.0f, -5.0f),
    vec3(3.0f,   0.0f, -5.0f),
    vec3(3.0f,   1.0f, 5.0f),
    vec3(-3.0f,  0.0f, 5.0f)
  };

  static f32 totalTime = 0.0f;
  totalTime += timeDelta*0.001f;

  for (u32 i(0); i<lights.size(); ++i)
  {
    float t = totalTime + i* 5.0f / lights.size();
    int baseFrame = (int)t;
    float f = t - baseFrame;

    vec3 p0 = light_path[(baseFrame + 0) % 5];
    vec3 p1 = light_path[(baseFrame + 1) % 5];
    vec3 p2 = light_path[(baseFrame + 2) % 5];
    vec3 p3 = light_path[(baseFrame + 3) % 5];

    vec3 position = maths::cubicInterpolation(p0, p1, p2, p3, f);
    scene.setLightPosition(lights[i], position);
  }
}


int main()
{
  //Create a window
  window::window_t window;
  window::create( "Scene", 800u, 600u, &window );

  //Initialize context
  render::context_t context;
  render::contextCreate( "Scene", "", window, 3, &context );

  //Initialize scene
  scene_t scene;
  scene.Initialize( context, maths::uvec2(800u,600u) );

  //Add some materials
  bkk::handle_t material0 = scene.AddMaterial( vec3(1.0f,1.0f,1.0f), vec3(0.1f,0.1f,0.1f), 1.0f);
  bkk::handle_t material1 = scene.AddMaterial( vec3(1.0f,1.0f,1.0f), vec3(0.4f,0.4f,0.4f), 1.0f);
  bkk::handle_t material2 = scene.AddMaterial( vec3(1.0f,1.0f,1.0f), vec3(0.8f,0.8f,0.8f), 1.0f);
  bkk::handle_t material3 = scene.AddMaterial( vec3(1.0f,1.0f,1.0f), vec3(0.2f,0.2f,0.2f), 1.0f);
  bkk::handle_t material4 = scene.AddMaterial( vec3(1.0f,1.0f,1.0f), vec3(0.5f,0.5f,0.5f), 1.0f);

  //Add some meshes
  bkk::handle_t bunny = scene.AddMesh( "../resources/bunny.ply" );
  bkk::handle_t sphere = scene.AddMesh("../resources/sphere.obj");
  bkk::handle_t quad = scene.AddQuadMesh();

  //Add instances
  scene.AddInstance( bunny, material0, maths::computeTransform( maths::vec3(-3.0f, 0.0f, -1.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( bunny, material1, maths::computeTransform( maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( bunny, material2, maths::computeTransform( maths::vec3(4.0f, 0.0f, -4.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( bunny, material3, maths::computeTransform( maths::vec3(-1.5f, 0.0f, 3.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( bunny, material4, maths::computeTransform( maths::vec3(2.5f, 0.0f, 3.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( quad,  material0, maths::computeTransform( maths::vec3(0.0f, 0.35f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::QUAT_UNIT));
  //scene.AddInstance( context, sphere, material2, maths::computeTransform(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(2.0f, 2.0f, 2.0f), maths::QUAT_UNIT));
    
  //Add lights
  std::vector < bkk::handle_t > lights;
  lights.push_back( scene.AddLight(vec3(0.0f,0.0f,0.0f),   5.0f, vec3(1.0f,0.0f,0.0f) ) );
  lights.push_back( scene.AddLight(vec3(0.0f, 0.0f, 0.0f), 5.0f, vec3(0.0f, 1.0f, 0.0f)) );
  lights.push_back( scene.AddLight(vec3(0.0f, 0.0f, 0.0f), 5.0f, vec3(0.0f, 0.0f, 1.0f)) );
  lights.push_back(scene.AddLight(vec3(0.0f, 0.0f, 0.0f), 5.0f, vec3(1.0f, 0.0f, 0.0f)));
  lights.push_back(scene.AddLight(vec3(0.0f, 0.0f, 0.0f), 5.0f, vec3(0.0f, 1.0f, 0.0f)));
  lights.push_back(scene.AddLight(vec3(0.0f, 0.0f, 0.0f), 5.0f, vec3(0.0f, 0.0f, 1.0f)));
  
  auto timePrev = bkk::time::getCurrent();
  auto currentTime = timePrev;

  bool quit = false;
  while( !quit )
  {
    window::event_t* event = nullptr;
    while( (event = window::getNextEvent( &window )) )
    {
      switch( event->type_)
      {
        case window::EVENT_QUIT:
        {
          quit = true;
          break;
        }
        case window::EVENT_RESIZE:
        {
          window::event_resize_t* resizeEvent = (window::event_resize_t*)event;
          scene.Resize( resizeEvent->width_, resizeEvent->height_ );
          break;
        }
        case window::EVENT_KEY:
        {
          window::event_key_t* keyEvent = (window::event_key_t*)event;
          scene.OnKeyEvent( keyEvent->keyCode_, keyEvent->pressed_, scene );
          break;
        }
        case window::EVENT_MOUSE_BUTTON:
        {
          window::event_mouse_button_t* buttonEvent = (window::event_mouse_button_t*)event;
          scene.OnMouseButton(buttonEvent->pressed_, buttonEvent->x_, buttonEvent->y_);
          break;
        }
        case window::EVENT_MOUSE_MOVE:
        {
          window::event_mouse_move_t* moveEvent = (window::event_mouse_move_t*)event;
          scene.OnMouseMove( moveEvent->x_, moveEvent->y_);
          break;
        }
        default:
          break;
      }
    }
    currentTime = bkk::time::getCurrent();
    f32 delta = bkk::time::getDifference(timePrev, currentTime);// *0.001f;
    AnimateLights(delta, lights, scene);
    timePrev = currentTime;

    
    //Render next frame
    scene.Render();
  }


  render::contextFlush( context );
  scene.Destroy();
  render::contextDestroy( &context );
  window::destroy( &window );
  return 0;
}