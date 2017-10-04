
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
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
  vec4 lightDirection;\n \
  vec4 lightColor;\n \
  }scene;\n \
  layout(set = 1, binding = 1) uniform MODEL\n \
  {\n \
    mat4 value;\n \
  }model;\n \
  out vec3 normalViewSpace;\n \
  out vec3 lightDirectionViewSpace;\n \
  void main(void)\n \
  {\n \
    mat4 modelView = scene.view * model.value;\n \
    gl_Position = scene.projection * modelView * vec4(aPosition,1.0);\n \
    normalViewSpace = normalize((modelView * vec4(aNormal,0.0)).xyz);\n \
    lightDirectionViewSpace = normalize((scene.view * normalize(scene.lightDirection)).xyz);\n \
  }\n"
};


static const char* gGeometrPassFragmentShaderSource = {
  "#version 440 core\n \
  layout (set = 0, binding = 0) uniform SCENE\n \
  {\n \
    mat4 view;\n \
    mat4 projection;\n \
    vec4 lightDirection;\n \
    vec4 lightColor;\n \
  }scene;\n \
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
  in vec3 lightDirectionViewSpace;\n \
  void main(void)\n \
  {\n \
    float diffuse = max(0.0, dot(normalViewSpace, lightDirectionViewSpace));\n \
    RT0 = material.albedo;\n \
    RT1 = vec4(normalize(normalViewSpace), material.roughness );\n \
    RT2 = vec4(material.F0, 1.0);\n \
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
    vec4 lightDirection_;
    vec4 lightColor_;
    vec4 shCoeff[9];
  };

  bkk::handle_t AddQuadMesh( render::context_t& context )
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
    mesh::create( context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &mesh, &allocator_ );
    return mesh_.add( mesh );
  }

  bkk::handle_t AddMesh( render::context_t& context, const char* url )
  {
    mesh::mesh_t mesh;
    mesh::createFromFile( context, url, &mesh, &allocator_ );
    return mesh_.add( mesh );
  }

  bkk::handle_t AddMaterial( render::context_t& context, const vec3& albedo, const vec3& F0, float roughness )
  {
    //Create uniform buffer and descriptor set
    material_t material = {};
    material.uniforms_.albedo_ = vec4(albedo,1.0);
    material.uniforms_.F0_ = F0;
    material.uniforms_.roughness_ = roughness;
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate(  context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                              &material.uniforms_, sizeof(material_t::material_uniforms_t),
                              &allocator_, &ubo );

    material.ubo_ = ubo;
    render::descriptor_t descriptor = render::getDescriptor(material.ubo_);
    render::descriptorSetCreate( context, descriptorPool_, materialDescriptorSetLayout_, &descriptor, &material.descriptorSet_ );
    return material_.add( material );
  }

  bkk::handle_t AddInstance( render::context_t& context, bkk::handle_t meshId, bkk::handle_t materialId, const maths::mat4& transform )
  {
    bkk::handle_t transformId = transformManager_.createTransform( transform );

    //Create uniform buffer and descriptor set
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate( context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                             nullptr, sizeof(mat4),
                             &allocator_, &ubo );

    instance_t instance = { meshId, materialId, transformId, ubo };
    render::descriptor_t descriptor = render::getDescriptor(instance.ubo_);
    render::descriptorSetCreate( context, descriptorPool_, instanceDescriptorSetLayout_, &descriptor, &instance.descriptorSet_ );
    return instance_.add( instance );
  }


  void InitializeOffscreenPass(render::context_t& context, const uvec2& size)
  {
    //Semaphore to indicate offscreen pass has completed
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(context.device_, &semaphoreCreateInfo, nullptr, &offscreenRenderComplete);


    //Create frame buffer attachments (Color and DepthStencil)
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &gBufferRT0_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &gBufferRT0_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &gBufferRT1_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &gBufferRT1_);
    render::texture2DCreate(context, size.x, size.y, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, render::texture_sampler_t(), &gBufferRT2_);
    bkk::render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &gBufferRT2_);
    render::depthStencilBufferCreate(context, size.x, size.y, &depthStencilBuffer_);

    //Create render pass
    geometryPass_ = {};
    render::render_pass_t::attachment_t attachments[4];
    attachments[0].format_ = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[0].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[1].format_ = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[1].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[2].format_ = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[2].initialLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[2].finallLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[2].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].samples_ = VK_SAMPLE_COUNT_1_BIT;

    attachments[3].format_ = depthStencilBuffer_.format_;
    attachments[3].initialLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[3].finallLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[3].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[3].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].samples_ = VK_SAMPLE_COUNT_1_BIT;

    render::renderPassCreate(context, 4u, &attachments[0], 0, 0, &geometryPass_);

    //Create frame buffer
    VkImageView fbAttachment[4] = { gBufferRT0_.imageView_, gBufferRT1_.imageView_, gBufferRT2_.imageView_, depthStencilBuffer_.imageView_ };
    render::frameBufferCreate(context, size.x, size.y, geometryPass_, &fbAttachment[0], &frameBuffer_);

    

    //Create descriptorSets layouts
    render::descriptor_binding_t binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0, render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &descriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 1, render::descriptor_t::stage::VERTEX };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &instanceDescriptorSetLayout_);

    binding = { render::descriptor_t::type::UNIFORM_BUFFER, 2, render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, 1u, &binding, &materialDescriptorSetLayout_);

    //Create pipeline layout
    render::descriptor_set_layout_t descriptorSetLayouts[3] = { descriptorSetLayout_, instanceDescriptorSetLayout_, materialDescriptorSetLayout_ };
    render::pipelineLayoutCreate(context, 3, &descriptorSetLayouts[0], &gBufferPipelineLayout_);

    //Create vertex format (position + normal)
    uint32_t vertexSize = 2 * sizeof(maths::vec3);
    render::vertex_attribute_t attributes[2] = { { render::vertex_attribute_t::format::VEC3, 0, vertexSize }, { render::vertex_attribute_t::format::VEC3, sizeof(maths::vec3), vertexSize } };
    render::vertexFormatCreate(attributes, 2u, &vertexFormat_);

    //Create off-screen pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gGeometryPassVertexShaderSource, &gBuffervertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gGeometrPassFragmentShaderSource, &gBufferfragmentShader_);
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
  }

  void Initialize( render::context_t& context, const uvec2& size )
  {   
    context_ = &context;

    //Create allocator for uniform buffers and meshes
    render::gpuAllocatorCreate(context, 100 * 1024 * 1024, 0xFFFF, render::gpu_memory_type_e::HOST_VISIBLE_COHERENT, &allocator_);

    //Create descriptor pool
    render::descriptorPoolCreate(context, 100u, 100u, 100u, 0u, 0u, &descriptorPool_);

    //Initialize off-screen render pass
    InitializeOffscreenPass(context, size);

    //Create scene uniform buffer
    camera_.position_ = vec3(0.0f, 2.5f, 8.0f);
    camera_.Update();
    uniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix(1.2f, (f32)size.x / (f32)size.y, 0.1f, 100.0f);
    uniforms_.viewMatrix_ = camera_.view_;
    uniforms_.lightDirection_ = vec4(0.0f, 1.0f, 1.0f, 0.0f);
    uniforms_.lightColor_ = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      (void*)&uniforms_, sizeof(scene_uniforms_t),
      &allocator_, &ubo_);

    //Create global descriptor set (Scene uniforms)    
    render::descriptor_t descriptor = render::getDescriptor(ubo_);
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, &descriptor, &gBufferDescriptorSet_);


    //Initialize on-screen pass (Presents image genereated by offscreen pass) 
    fullScreenQuad_ = sample_utils::FullScreenQuad(context);
    
    //Descriptor set layout and pipeline layout
    render::descriptor_binding_t binding = { bkk::render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, bkk::render::descriptor_t::stage::FRAGMENT };
    bkk::render::descriptor_set_layout_t descriptorSetLayout;
    bkk::render::descriptorSetLayoutCreate(context, 1u, &binding, &descriptorSetLayout);
    bkk::render::pipelineLayoutCreate(context, 1u, &descriptorSetLayout, &pipelineLayout_);

    //Presentation descriptor sets
    descriptor = bkk::render::getDescriptor(gBufferRT0_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[0]);
    descriptor = bkk::render::getDescriptor(gBufferRT1_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[1]);
    descriptor = bkk::render::getDescriptor(gBufferRT2_);
    bkk::render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout, &descriptor, &descriptorSet_[2]);
        

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

    BuildCommandBuffers();
    render::commandBufferSubmit(*context_, commandBuffer_);
    render::presentNextImage( context_, 1, &offscreenRenderComplete);
  }

  void BuildCommandBuffers()
  {
    //Offscreen command buffer
    if (commandBuffer_.handle_ == VK_NULL_HANDLE)
    {
      render::commandBufferCreate(*context_, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0, nullptr, nullptr, 1, &offscreenRenderComplete, render::command_buffer_t::GRAPHICS, &commandBuffer_);
    }
    
    VkClearValue clearValues[4];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[3].depthStencil = { 1.0f,0 };

    render::commandBufferBegin(*context_, &frameBuffer_, 4, clearValues, commandBuffer_);
    {
      bkk::render::graphicsPipelineBind(commandBuffer_.handle_, gBufferPipeline_);

      packed_freelist_iterator_t<instance_t> instanceIter = instance_.begin();
      while (instanceIter != instance_.end())
      {
        //Bind descriptor set
        std::vector<render::descriptor_set_t> descriptorSets(3);
        descriptorSets[0] = gBufferDescriptorSet_;
        descriptorSets[1] = instanceIter.get().descriptorSet_;
        descriptorSets[2] = material_.get(instanceIter.get().material_)->descriptorSet_;
        bkk::render::descriptorSetBindForGraphics(commandBuffer_.handle_, gBufferPipelineLayout_, 0, descriptorSets.data(), (uint32_t)descriptorSets.size());

        //Draw call
        mesh::mesh_t* mesh = mesh_.get(instanceIter.get().mesh_);
        mesh::draw(commandBuffer_.handle_, *mesh);
        ++instanceIter;
      }
    }
    render::commandBufferEnd(*context_, commandBuffer_);


    //Presentation command buffers
    for (unsigned i(0); i<3; ++i)
    {
      VkCommandBuffer cmdBuffer = bkk::render::beginPresentationCommandBuffer(*context_, i, nullptr);
      bkk::render::graphicsPipelineBind(cmdBuffer, pipeline_);
      bkk::render::descriptorSetBindForGraphics(cmdBuffer, pipelineLayout_, 0, &descriptorSet_[currentDescriptorSet_], 1u);
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
    render::descriptorSetDestroy(*context_, &gBufferDescriptorSet_ );
    render::descriptorPoolDestroy(*context_, &descriptorPool_ );
    render::pipelineLayoutDestroy(*context_, &gBufferPipelineLayout_);
    render::gpuBufferDestroy(*context_, &ubo_, &allocator_ );
    render::gpuAllocatorDestroy(*context_, &allocator_ );
    render::textureDestroy(*context_, &gBufferRT0_);
    render::textureDestroy(*context_, &gBufferRT1_);
    render::textureDestroy(*context_, &gBufferRT2_);
    render::depthStencilBufferDestroy(*context_, &depthStencilBuffer_);
    render::commandBufferDestroy(*context_, &commandBuffer_);
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

  render::descriptor_set_layout_t descriptorSetLayout_;
  render::descriptor_set_layout_t materialDescriptorSetLayout_;
  render::descriptor_set_layout_t instanceDescriptorSetLayout_;
  render::descriptor_set_t gBufferDescriptorSet_;
  
  render::gpu_memory_allocator_t allocator_;
  render::gpu_buffer_t ubo_;
  
  render::vertex_format_t vertexFormat_;
  render::pipeline_layout_t gBufferPipelineLayout_;
  render::graphics_pipeline_t gBufferPipeline_;
  
  render::shader_t gBuffervertexShader_;
  render::shader_t gBufferfragmentShader_;
  scene_uniforms_t uniforms_;  

  packed_freelist_t<material_t> material_;
  packed_freelist_t<mesh::mesh_t> mesh_;
  packed_freelist_t<instance_t> instance_;

  VkSemaphore offscreenRenderComplete;
  render::command_buffer_t commandBuffer_;
  render::render_pass_t geometryPass_;
  render::texture_t gBufferRT0_;
  render::texture_t gBufferRT1_;
  render::texture_t gBufferRT2_;

  render::depth_stencil_buffer_t depthStencilBuffer_;
  render::frame_buffer_t frameBuffer_;

  render::graphics_pipeline_t pipeline_;
  render::pipeline_layout_t pipelineLayout_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;

  uint32_t currentDescriptorSet_ = 0u;
  render::descriptor_set_t descriptorSet_[3];
  mesh::mesh_t fullScreenQuad_;

  sample_utils::free_camera_t camera_;
  maths::vec2 mousePosition_ = vec2(0.0f, 0.0f);
  bool mouseButtonPressed_ = false;  
};

int main()
{
  //Create a window
  window::window_t window;
  window::create( "Scene", 400u, 400u, &window );

  //Initialize context
  render::context_t context;
  render::contextCreate( "Scene", "", window, 3, &context );

  //Initialize scene
  scene_t scene;
  scene.Initialize( context, maths::uvec2(400u,400u) );

  //Add some materials
  bkk::handle_t material0 = scene.AddMaterial( context, vec3(1.0f,0.0f,0.0f), vec3(0.1f,0.1f,0.1f), 1.0f);
  bkk::handle_t material1 = scene.AddMaterial( context, vec3(0.0f,1.0f,0.0f), vec3(0.4f,0.4f,0.4f), 1.0f);
  bkk::handle_t material2 = scene.AddMaterial( context, vec3(1.0f,0.0f,1.0f), vec3(0.8f,0.8f,0.8f), 1.0f);
  bkk::handle_t material3 = scene.AddMaterial( context, vec3(1.0f,1.0f,0.0f), vec3(0.2f,0.2f,0.2f), 1.0f);
  bkk::handle_t material4 = scene.AddMaterial( context, vec3(0.0f,1.0f,1.0f), vec3(0.5f,0.5f,0.5f), 1.0f);

  //Add some meshes
  bkk::handle_t bunny = scene.AddMesh( context, "../resources/bunny.ply" );
  bkk::handle_t quad = scene.AddQuadMesh( context );

  //Add instances
  scene.AddInstance( context, bunny, material0, maths::computeTransform( maths::vec3(-3.0f, 0.0f, -1.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material1, maths::computeTransform( maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material2, maths::computeTransform( maths::vec3(4.0f, 0.0f, -4.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material3, maths::computeTransform( maths::vec3(-1.5f, 0.0f, 3.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material4, maths::computeTransform( maths::vec3(2.5f, 0.0f, 3.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, quad,  material0, maths::computeTransform( maths::vec3(0.0f, 0.35f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::QUAT_UNIT));
    
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

    //Render next frame
    scene.Render();
  }

  render::contextFlush( context );
  scene.Destroy();
  render::contextDestroy( &context );
  window::destroy( &window );
  return 0;
}