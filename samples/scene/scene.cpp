
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
#include "../utility.h"
#include "transform-manager.h"
#include "packed-freelist.h"

using namespace bkk;
using namespace maths;

static const char* gVertexShaderSource = {
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


static const char* gFragmentShaderSource = {
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
  layout(location = 0) out vec4 color;\n \
  in vec3 normalViewSpace;\n \
  in vec3 lightDirectionViewSpace;\n \
  void main(void)\n \
  {\n \
    float diffuse = max(0.0, dot(normalViewSpace, lightDirectionViewSpace));\n \
    color = vec4(diffuse*scene.lightColor.rgb, 1.0) * material.albedo;\n \
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
    attributes[0] = { render::attribute_format_e::VEC3, 0, sizeof(Vertex) };
    attributes[1] = { render::attribute_format_e::VEC3, offsetof(Vertex, normal), sizeof(Vertex) };

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
    render::gpuBufferCreate(  context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                              &material.uniforms_, sizeof(material_t::material_uniforms_t),
                              &allocator_, &ubo );

    material.ubo_ = ubo;
    material.descriptorSet_.descriptors_.resize(1);
    material.descriptorSet_.descriptors_[0].bufferDescriptor_ = material.ubo_.descriptor_;
    render::descriptorSetCreate( context, descriptorPool_, materialDescriptorSetLayout_, &material.descriptorSet_ );
    return material_.add( material );
  }

  bkk::handle_t AddInstance( render::context_t& context, bkk::handle_t meshId, bkk::handle_t materialId, const maths::mat4& transform )
  {
    bkk::handle_t transformId = transformManager_.createTransform( transform );

    //Create uniform buffer and descriptor set
    render::gpu_buffer_t ubo;
    render::gpuBufferCreate( context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                             nullptr, sizeof(mat4),
                             &allocator_, &ubo );

    instance_t instance = { meshId, materialId, transformId, ubo };
    instance.descriptorSet_.descriptors_.resize(1);
    instance.descriptorSet_.descriptors_[0].bufferDescriptor_ = instance.ubo_.descriptor_;
    render::descriptorSetCreate( context, descriptorPool_, instanceDescriptorSetLayout_, &instance.descriptorSet_ );
    return instance_.add( instance );
  }

  void Initialize( render::context_t& context, const uvec2& size )
  {
    //Create allocator for uniform buffers and meshes
    render::gpuAllocatorCreate( context, 100*1024*1024, 0xFFFF, render::gpu_memory_type_e::HOST_VISIBLE_COHERENT, &allocator_ );

    //Create scene uniform buffer
    camera_.position_ = vec3(0.0f,2.5f,7.0f);
    camera_.Update();
    uniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix( 1.5f,(f32)size.x / (f32)size.y, 0.1f,100.0f );
    uniforms_.viewMatrix_ = camera_.view_;
    uniforms_.lightDirection_ = vec4(0.0f,1.0f,1.0f,0.0f);
    uniforms_.lightColor_ = vec4(1.0f,1.0f,1.0f,1.0f);
    render::gpuBufferCreate( context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                             (void*)&uniforms_, sizeof(scene_uniforms_t),
                             &allocator_, &ubo_ );

    //Create descriptorSets layouts
    render::descriptor_binding_t binding = { render::descriptor_type_e::UNIFORM_BUFFER, 0, render::descriptor_stage_e::VERTEX | render::descriptor_stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate( context, 1u, &binding, &descriptorSetLayout_ );

    binding = { render::descriptor_type_e::UNIFORM_BUFFER, 1, render::descriptor_stage_e::VERTEX };
    render::descriptorSetLayoutCreate( context, 1u, &binding, &instanceDescriptorSetLayout_ );

    binding = { render::descriptor_type_e::UNIFORM_BUFFER, 2, render::descriptor_stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate( context, 1u, &binding, &materialDescriptorSetLayout_ );

    //Create pipeline layout
    pipelineLayout_.descriptorSetLayout_.push_back( descriptorSetLayout_ );
    pipelineLayout_.descriptorSetLayout_.push_back( instanceDescriptorSetLayout_ );
    pipelineLayout_.descriptorSetLayout_.push_back( materialDescriptorSetLayout_ );
    render::pipelineLayoutCreate( context, &pipelineLayout_ );

    //Create vertex format (position + normal)
    uint32_t vertexSize = 2 * sizeof(maths::vec3);
    render::vertex_attribute_t attributes[2];
    attributes[0] = { render::attribute_format_e::VEC3, 0, vertexSize  };
    attributes[1] = { render::attribute_format_e::VEC3, sizeof(maths::vec3), vertexSize  };
    render::vertexFormatCreate( attributes, 2u, &vertexFormat_ );

    //Create pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);
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
    render::graphicsPipelineCreate( context, context.swapChain_.renderPass_, vertexFormat_, pipelineLayout_, &pipeline_ );

    //Create descriptor pool
    descriptorPool_ = {};
    descriptorPool_.uniformBuffers_ = 100u;
    descriptorPool_.descriptorSets_ = 100u;
    render::descriptorPoolCreate( context, &descriptorPool_ );

    //Create global descriptor set (Scene uniforms)
    descriptorSet_.descriptors_.resize(1);
    descriptorSet_.descriptors_[0].bufferDescriptor_ = ubo_.descriptor_;
    render::descriptorSetCreate( context, descriptorPool_, descriptorSetLayout_, &descriptorSet_ );
  }

  void Resize( render::context_t& context, uint32_t width, uint32_t height )
  {
    uniforms_.projectionMatrix_ = computePerspectiveProjectionMatrix( 1.5f, (f32)width / (f32)height,0.1f,100.0f );
    render::swapchainResize( &context, width, height );
    BuildCommandBuffers( context );
  }

  void Render( render::context_t& context )
  {
    //Update scene
    transformManager_.update();
    uniforms_.viewMatrix_ = camera_.view_;
    render::gpuBufferUpdate( context, (void*)&uniforms_, 0u, sizeof(scene_uniforms_t), &ubo_ );

    //Update modelview matrices
    std::vector<instance_t>& instance( instance_.getData() );
    for( u32 i(0); i<instance.size(); ++i )
    {
      maths::mat4* modelTx = transformManager_.getWorldMatrix( instance[i].transform_ );
      render::gpuBufferUpdate( context, modelTx, 0, sizeof(mat4), &instance[i].ubo_ );
    }

    //Render
    render::presentNextImage( &context );
  }

  void BuildCommandBuffers( render::context_t& context )
  {
    for( unsigned i(0); i<3; ++i )
    {
      VkCommandBuffer cmdBuffer = render::beginPresentationCommandBuffer( context, i, nullptr );
      bkk::render::graphicsPipelineBind(cmdBuffer, pipeline_);

      packed_freelist_iterator_t<instance_t> instanceIter( instance_ );
      while( !instanceIter.end() )
      {
        //Bind descriptor set
        std::vector<render::descriptor_set_t> descriptorSets(3);
        descriptorSets[0] = descriptorSet_;
        descriptorSets[1] = instanceIter.get().descriptorSet_;
        descriptorSets[2] = material_.get( instanceIter.get().material_ )->descriptorSet_;
        bkk::render::descriptorSetBindForGraphics(cmdBuffer, pipelineLayout_, 0, descriptorSets.data(), (uint32_t)descriptorSets.size());

        //Draw call
        mesh::mesh_t* mesh = mesh_.get( instanceIter.get().mesh_ );
        mesh::draw(cmdBuffer, *mesh );
        ++instanceIter;
      }

      render::endPresentationCommandBuffer( context, i );
    }
  }

  void Destroy( render::context_t& context )
  {
    //Destroy meshes
    packed_freelist_iterator_t<mesh::mesh_t> meshIter( mesh_ );
    while( !meshIter.end() )
    {
      mesh::destroy( context, &meshIter.get(), &allocator_ );
      ++meshIter;
    }

    //Destroy material resources
    packed_freelist_iterator_t<material_t> materialIter( material_ );
    while( !materialIter.end() )
    {
      render::gpuBufferDestroy( context, &materialIter.get().ubo_, &allocator_ );
      render::descriptorSetDestroy( context, &materialIter.get().descriptorSet_ );
      ++materialIter;
    }

    //Destroy instance resources
    packed_freelist_iterator_t<instance_t> instanceIter( instance_ );
    while( !instanceIter.end() )
    {
      render::gpuBufferDestroy( context, &instanceIter.get().ubo_, &allocator_ );
      render::descriptorSetDestroy( context, &instanceIter.get().descriptorSet_ );
      ++instanceIter;
    }

    //Destroy global resources
    render::shaderDestroy(context, &vertexShader_);
    render::shaderDestroy(context, &fragmentShader_);
    render::graphicsPipelineDestroy( context, &pipeline_ );
    render::descriptorSetDestroy( context, &descriptorSet_ );
    render::descriptorPoolDestroy( context, &descriptorPool_ );
    render::pipelineLayoutDestroy( context, &pipelineLayout_ );
    render::gpuBufferDestroy( context, &ubo_, &allocator_ );

    render::gpuAllocatorDestroy( context, &allocator_ );
  }

public:
  sample_utils::free_camera_t camera_;

private:
  bkk::transform_manager_t transformManager_;
  render::descriptor_set_layout_t descriptorSetLayout_;
  render::descriptor_set_layout_t materialDescriptorSetLayout_;
  render::descriptor_set_layout_t instanceDescriptorSetLayout_;
  render::descriptor_set_t descriptorSet_;
  render::gpu_buffer_t ubo_;

  render::vertex_format_t vertexFormat_;
  render::pipeline_layout_t pipelineLayout_;
  render::graphics_pipeline_t pipeline_;
  render::descriptor_pool_t descriptorPool_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;
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
  window::create( "Scene", 400u, 400u, &window );

  //Initialize context
  render::context_t context;
  render::contextCreate( "Scene", "", window, 3, &context );

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
  bkk::handle_t bunny = scene.AddMesh( context, "../resources/bunny.ply" );
  bkk::handle_t quad = scene.AddQuadMesh( context );

  //Add instances
  scene.AddInstance( context, bunny, material0, maths::computeTransform( maths::vec3(-3.0f, 0.0f, -1.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material1, maths::computeTransform( maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material2, maths::computeTransform( maths::vec3(4.0f, 0.0f, -4.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material3, maths::computeTransform( maths::vec3(-1.5f, 0.0f, 3.5f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, bunny, material4, maths::computeTransform( maths::vec3(2.5f, 0.0f, 3.0f), maths::vec3(10.0f, 10.0f, 10.0f), maths::QUAT_UNIT ) );
  scene.AddInstance( context, quad,  material0, maths::computeTransform( maths::vec3(0.0f, 0.35f, 0.0f), maths::vec3(5.0f, 5.0f, 5.0f), maths::QUAT_UNIT));
  scene.BuildCommandBuffers( context );

  maths::vec2 mousePosition = vec2(0.0f,0.0f);
  bool mouseButtonPressed = false;
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

  render::contextFlush( context );
  scene.Destroy( context );
  render::contextDestroy( &context );
  window::destroy( &window );
  return 0;
}