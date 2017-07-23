
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
#include "utility.h"
#include "transform-manager.h"
#include "packed-freelist.h"

using namespace bkk;
using namespace maths;

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

    static mesh::vertex_attribute_t attributes[2];
    attributes[0] = { mesh::attribute_format_e::VEC3, 0, sizeof(Vertex) };
    attributes[1] = { mesh::attribute_format_e::VEC3, 3*sizeof(float), sizeof(Vertex) };

    mesh::mesh_t mesh;
    mesh::Create( &context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &mesh, &allocator_ );
    return mesh_.Add( mesh );
  }

  bkk::handle_t AddMesh( render::context_t& context, const char* url )
  {
    mesh::mesh_t mesh;
    mesh::CreateFromFile( &context, url, &mesh, &allocator_ );
    return mesh_.Add( mesh );
  }

  bkk::handle_t AddMaterial( render::context_t& context, const vec3& albedo, const vec3& F0, float roughness )
  {
    //Create uniform buffer and descriptor set
    material_t material = {};
    material.uniforms_.albedo_ = vec4(albedo,1.0);
    material.uniforms_.F0_ = F0;
    material.uniforms_.roughness_ = roughness;
    render::gpu_buffer_t ubo;
    render::GpuBufferCreate( &context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                              &material.uniforms_, sizeof(material_t::material_uniforms_t),
                              &allocator_, &ubo );

    material.ubo_ = ubo;
    material.descriptorSet_.descriptors_.resize(1);
    material.descriptorSet_.descriptors_[0].bufferDescriptor_ = material.ubo_.descriptor_;
    render::DescriptorSetCreate( &context, &descriptorPool_, &materialDescriptorSetLayout_, &material.descriptorSet_ );
    return material_.Add( material );
  }

  bkk::handle_t AddInstance( render::context_t& context, bkk::handle_t meshId, bkk::handle_t materialId, const maths::mat4& transform )
  {
    bkk::handle_t transformId = transformManager_.CreateTransform( transform );

    //Create uniform buffer and descriptor set
    render::gpu_buffer_t ubo;
    render::GpuBufferCreate( &context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                              nullptr, sizeof(mat4),
                              &allocator_, &ubo );

    instance_t instance = { meshId, materialId, transformId, ubo };
    instance.descriptorSet_.descriptors_.resize(1);
    instance.descriptorSet_.descriptors_[0].bufferDescriptor_ = instance.ubo_.descriptor_;
    render::DescriptorSetCreate( &context, &descriptorPool_, &instanceDescriptorSetLayout_, &instance.descriptorSet_ );
    return instance_.Add( instance );
  }

  void Initialize( render::context_t& context, const uvec2& size )
  {
    //Create allocator for uniform buffers and meshes
    render::AllocatorCreate( &context, 100*1024*1024, 0xFFFF, render::gpu_memory_type_e::HOST_VISIBLE_COHERENT, &allocator_ );

    //Create scene uniform buffer
    camera_.position_ = vec3(0.0f,2.5f,7.0f);
    camera_.Update();
    uniforms_.projectionMatrix_ = ComputePerspectiveProjectionMatrix( 1.5f,(f32)size.x / (f32)size.y, 0.1f,100.0f );
    uniforms_.viewMatrix_ = camera_.view_;
    uniforms_.lightDirection_ = vec4(0.0f,1.0f,1.0f,0.0f);
    uniforms_.lightColor_ = vec4(1.0f,1.0f,1.0f,1.0f);
    render::GpuBufferCreate( &context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                              (void*)&uniforms_, sizeof(scene_uniforms_t),
                              &allocator_, &ubo_ );

    //Create descriptorSets layouts
    descriptorSetLayout_.bindings_.push_back( { render::descriptor_type_e::UNIFORM_BUFFER, 0, render::descriptor_stage_e::VERTEX | render::descriptor_stage_e::FRAGMENT } );
    render::DescriptorSetLayoutCreate( &context, &descriptorSetLayout_ );

    instanceDescriptorSetLayout_.bindings_.push_back( { render::descriptor_type_e::UNIFORM_BUFFER, 1, render::descriptor_stage_e::VERTEX } );
    render::DescriptorSetLayoutCreate( &context, &instanceDescriptorSetLayout_ );

    materialDescriptorSetLayout_.bindings_.push_back( { render::descriptor_type_e::UNIFORM_BUFFER, 2, render::descriptor_stage_e::FRAGMENT } );
    render::DescriptorSetLayoutCreate( &context, &materialDescriptorSetLayout_ );

    //Create pipeline layout
    pipelineLayout_.descriptorSetLayout_.push_back( descriptorSetLayout_ );
    pipelineLayout_.descriptorSetLayout_.push_back( instanceDescriptorSetLayout_ );
    pipelineLayout_.descriptorSetLayout_.push_back( materialDescriptorSetLayout_ );
    render::PipelineLayoutCreate( &context, &pipelineLayout_ );

    //Create vertex format (position + normal)
    size_t vertexSize = 2 * sizeof(maths::vec3);
    mesh::vertex_attribute_t attributes[2];
    attributes[0] = { mesh::attribute_format_e::VEC3, 0, vertexSize  };
    attributes[1] = { mesh::attribute_format_e::VEC3, sizeof(maths::vec3), vertexSize  };
    mesh::VertexFormatCreate( &context, attributes, 2u, &vertexFormat_ );

    //Create pipeline
    vertexShader_   = LoadShader( &context , "shaders/scene.vert.spv" );
    fragmentShader_ = LoadShader( &context, "shaders/scene.frag.spv" );
    pipeline_.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipeline_.scissorRect_ = { {0,0}, {context.swapChain_.imageWidth_,context.swapChain_.imageHeight_} };
    pipeline_.blendState_.resize(1);
    pipeline_.blendState_[0].colorWriteMask = 0xF;
    pipeline_.blendState_[0].blendEnable = VK_FALSE;
    pipeline_.cullMode_ = VK_CULL_MODE_BACK_BIT;
    pipeline_.depthTestEnabled_ = true;
    pipeline_.depthWriteEnabled_ = true;
    pipeline_.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipeline_.vertexShader_ = vertexShader_;
    pipeline_.fragmentShader_ = fragmentShader_;
    render::GraphicsPipelineCreate( &context, context.swapChain_.renderPass_, &vertexFormat_, &pipelineLayout_, &pipeline_ );

    //Create descriptor pool
    descriptorPool_ = {};
    descriptorPool_.uniformBuffers_ = 100u;
    descriptorPool_.descriptorSets_ = 100u;
    render::DescriptorPoolCreate( &context, &descriptorPool_ );

    //Create global descriptor set (Scene uniforms)
    descriptorSet_.descriptors_.resize(1);
    descriptorSet_.descriptors_[0].bufferDescriptor_ = ubo_.descriptor_;
    render::DescriptorSetCreate( &context, &descriptorPool_, &descriptorSetLayout_, &descriptorSet_ );
  }

  void Resize( render::context_t& context, uint32_t width, uint32_t height )
  {
    uniforms_.projectionMatrix_ = ComputePerspectiveProjectionMatrix( 1.5f, (f32)width / (f32)height,0.1f,100.0f );
    render::ContextResize( &context, width, height );
    BuildCommandBuffers( context );
  }

  void Render( render::context_t& context )
  {
    //Update scene
    transformManager_.Update();
    uniforms_.viewMatrix_ = camera_.view_;
    render::GpuBufferUpdate( &context, &uniforms_, 0u, sizeof(scene_uniforms_t), &ubo_ );

    //Update modelview matrices
    std::vector<instance_t>& instance( instance_.GetData() );
    for( u32 i(0); i<instance.size(); ++i )
    {
      maths::mat4* modelTx = transformManager_.GetWorldMatrix( instance[i].transform_ );
      render::GpuBufferUpdate( &context, modelTx, 0, sizeof(mat4), &instance[i].ubo_ );
    }

    //Render
    render::PresentNextImage( &context );
  }

  void BuildCommandBuffers( render::context_t& context )
  {
    for( unsigned i(0); i<3; ++i )
    {
      VkCommandBuffer cmdBuffer = render::BeginPresentationCommandBuffer( &context, i, nullptr );
      vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle_);

      packed_freelist_iterator_t<instance_t> instanceIter( instance_ );
      while( !instanceIter.End() )
      {
        //Bind descriptor set
        std::vector<VkDescriptorSet> descriptorSets(3);
        descriptorSets[0] = descriptorSet_.handle_;
        descriptorSets[1] = instanceIter.Get().descriptorSet_.handle_;
        descriptorSets[2] = material_.Get( instanceIter.Get().material_ )->descriptorSet_.handle_;
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_.handle_, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        //Draw call
        mesh::mesh_t* mesh = mesh_.Get( instanceIter.Get().mesh_ );
        mesh::Draw(cmdBuffer, mesh );
        ++instanceIter;
      }

      render::EndPresentationCommandBuffer( &context, i );
    }
  }

  void Destroy( render::context_t& context )
  {
    //Destroy meshes
    packed_freelist_iterator_t<mesh::mesh_t> meshIter( mesh_ );
    while( !meshIter.End() )
    {
      mesh::Destroy( &context, &meshIter.Get(), &allocator_ );
      ++meshIter;
    }

    //Destroy material resources
    packed_freelist_iterator_t<material_t> materialIter( material_ );
    while( !materialIter.End() )
    {
      render::GpuBufferDestroy( &context, &materialIter.Get().ubo_, &allocator_ );
      render::DescriptorSetDestroy( &context, &materialIter.Get().descriptorSet_ );
      ++materialIter;
    }

    //Destroy instance resources
    packed_freelist_iterator_t<instance_t> instanceIter( instance_ );
    while( !instanceIter.End() )
    {
      render::GpuBufferDestroy( &context, &instanceIter.Get().ubo_, &allocator_ );
      render::DescriptorSetDestroy( &context, &instanceIter.Get().descriptorSet_ );
      ++instanceIter;
    }

    //Destroy global resources
    vkDestroyShaderModule(context.device_, vertexShader_, nullptr);
    vkDestroyShaderModule(context.device_, fragmentShader_, nullptr);
    render::GraphicsPipelineDestroy( &context, &pipeline_ );
    render::DescriptorSetDestroy( &context, &descriptorSet_ );
    render::DescriptorPoolDestroy( &context, &descriptorPool_ );
    render::PipelineLayoutDestroy( &context, &pipelineLayout_ );
    render::GpuBufferDestroy( &context, &ubo_, &allocator_ );

    render::AllocatorDestroy( &context, &allocator_ );
  }

public:
  sample_utils::free_camera_t camera_;

private:
  bkk::transform::transform_manager_t transformManager_;
  render::descriptor_set_layout_t descriptorSetLayout_;
  render::descriptor_set_layout_t materialDescriptorSetLayout_;
  render::descriptor_set_layout_t instanceDescriptorSetLayout_;
  render::descriptor_set_t descriptorSet_;
  render::gpu_buffer_t ubo_;

  mesh::vertex_format_t vertexFormat_;
  render::pipeline_layout_t pipelineLayout_;
  render::graphics_pipeline_t pipeline_;
  render::descriptor_pool_t descriptorPool_;
  VkShaderModule vertexShader_;
  VkShaderModule fragmentShader_;
  scene_uniforms_t uniforms_;

  packed_freelist_t<material_t> material_;
  packed_freelist_t<mesh::mesh_t> mesh_;
  packed_freelist_t<instance_t> instance_;

  render::gpu_memory_allocator_t allocator_;
};

void OnKeyEvent( window::key_e key, bool pressed, scene_t& scene )
{
  if( pressed )
  {
    switch( key )
    {
      case window::key_e::KEY_UP:
      case 'w':
      {
        scene.camera_.Move( 0.0f, -0.5f );
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        scene.camera_.Move( 0.0f, 0.5f );
        break;
      }
      case window::key_e::KEY_LEFT:
      case 'a':
      {
        scene.camera_.Move( 0.5f, 0.0f );
        break;
      }
      case window::key_e::KEY_RIGHT:
      case 'd':
      {
        scene.camera_.Move( -0.5f, 0.0f );
        break;
      }
      default:
        break;
    }
  }
}

int main()
{
  //Create a window
  window::window_t window;
  window::Initialize( "Scene", 400u, 400u, &window );

  //Initialize context
  render::context_t context;
  render::ContextCreate( "Scene", "", &window, 3, &context );

  //Initialize scene
  scene_t scene;
  scene.Initialize( context, maths::uvec2(400u,400u) );

  //Add some materials
  bkk::handle_t material0 = scene.AddMaterial( context, vec3(1.0f,0.0f,0.0f), vec3(0.0f,1.0f,0.0f), 1.0f);
  bkk::handle_t material1 = scene.AddMaterial( context, vec3(0.0f,1.0f,0.0f), vec3(0.0f,1.0f,0.0f), 1.0f);
  bkk::handle_t material2 = scene.AddMaterial( context, vec3(1.0f,0.0f,1.0f), vec3(0.0f,1.0f,0.0f), 1.0f);
  bkk::handle_t material3 = scene.AddMaterial( context, vec3(1.0f,1.0f,0.0f), vec3(0.0f,1.0f,0.0f), 1.0f);
  bkk::handle_t material4 = scene.AddMaterial( context, vec3(0.0f,1.0f,1.0f), vec3(0.0f,1.0f,0.0f), 1.0f);

  //Add some meshes
  bkk::handle_t bunny = scene.AddMesh( context, "./resources/bunny.ply" );
  bkk::handle_t quad = scene.AddQuadMesh( context );

  //Add instances
  scene.AddInstance( context, bunny, material0, maths::ComputeTransform( maths::vec3(-3.0f, 0.0f, -1.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material1, maths::ComputeTransform( maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material2, maths::ComputeTransform( maths::vec3(4.0f, 0.0f, -4.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material3, maths::ComputeTransform( maths::vec3(-1.5f, 0.0f, 3.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material4, maths::ComputeTransform( maths::vec3(2.5f, 0.0f, 3.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, quad,  material0, maths::ComputeTransform( maths::vec3(0.0f, 0.35f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::QUAT_UNIT));
  scene.BuildCommandBuffers( context );

  maths::vec2 mousePosition = vec2(0.0f,0.0f);
  bool mouseButtonPressed = false;
  bool quit = false;
  while( !quit )
  {
    window::event_t* event = nullptr;
    while( (event = window::GetNextEvent( &window )) )
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
          scene.Resize( context, resizeEvent->width_, resizeEvent->height_ );
          break;
        }
        case window::EVENT_KEY:
        {
          window::event_key_t* keyEvent = (window::event_key_t*)event;
          OnKeyEvent( keyEvent->keyCode_, keyEvent->pressed_, scene );
          break;
        }
        case window::EVENT_MOUSE_BUTTON:
        {
          window::event_mouse_button_t* buttonEvent = (window::event_mouse_button_t*)event;
          mouseButtonPressed = buttonEvent->pressed_;
          mousePosition.x = (f32)buttonEvent->x_;
          mousePosition.y = (f32)buttonEvent->y_;
          break;
        }
        case window::EVENT_MOUSE_MOVE:
        {
          window::event_mouse_move_t* moveEvent = (window::event_mouse_move_t*)event;
          if( mouseButtonPressed )
          {
            f32 angleY = (moveEvent->x_ - mousePosition.x) * 0.01f;
            f32 angleX = (moveEvent->y_ - mousePosition.y) * 0.01f;
            mousePosition.x = (f32)moveEvent->x_;
            mousePosition.y = (f32)moveEvent->y_;
            scene.camera_.Rotate( angleX,angleY );
          }
          break;
        }
        default:
          break;
      }
    }
    scene.Render( context );
  }

  render::Flush( &context );
  scene.Destroy( context );
  render::ContextDestroy( &context );
  window::Close( &window );
  return 0;
}