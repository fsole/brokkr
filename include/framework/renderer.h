/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "core/render-types.h"
#include "core/packed-freelist.h"
#include "core/transform-manager.h"

#include "core/mesh.h"

#include "framework/shader.h"
#include "framework/material.h"
#include "framework/compute-material.h"
#include "framework/render-target.h"
#include "framework/frame-buffer.h"
#include "framework/actor.h"
#include "framework/camera.h"

namespace bkk
{
  namespace core
  {
    namespace window { struct window_t; }
  }

  namespace framework
  {
    typedef core::handle_t mesh_handle_t;
    typedef core::handle_t camera_handle_t;

    class command_buffer_t;

    class renderer_t
    {
      public:
        renderer_t();
        ~renderer_t();
        
        void initialize(const char* title, uint32_t imageCount, const core::window::window_t& window);
        core::render::context_t& getContext();

        shader_handle_t shaderCreate(const char* file);
        shader_t* getShader(shader_handle_t handle);

        material_handle_t materialCreate(shader_handle_t shader);
        material_t* getMaterial(material_handle_t handle);

        compute_material_handle_t computeMaterialCreate(shader_handle_t shader);
        compute_material_t* getComputeMaterial(compute_material_handle_t handle);

        render_target_handle_t renderTargetCreate(uint32_t width, uint32_t height,VkFormat format,bool depthBuffer);
        render_target_t* getRenderTarget(render_target_handle_t handle);

        frame_buffer_handle_t frameBufferCreate(render_target_handle_t* renderTargets, uint32_t targetCount,VkImageLayout* initialLayouts = nullptr, VkImageLayout* finalLayouts = nullptr);
        frame_buffer_t* getFrameBuffer(frame_buffer_handle_t handle);

        mesh_handle_t addMesh(const core::mesh::mesh_t& mesh);
        mesh_handle_t meshCreate(const char* file, core::mesh::export_flags_e exportFlags, core::render::gpu_memory_allocator_t* allocator = nullptr, uint32_t submesh = 0);
        core::mesh::mesh_t* getMesh(mesh_handle_t handle);

        actor_handle_t actorCreate(const char* name, mesh_handle_t mesh, material_handle_t material, core::maths::mat4 transform = core::maths::mat4(), uint32_t instanceCount = 1);
        actor_t* getActor(actor_handle_t handle);        
        void actorSetParent(actor_handle_t actor, actor_handle_t parent);
        void actorSetTransform(actor_handle_t actor, const core::maths::mat4& newTransform);
        actor_handle_t getRootActor() { return rootActor_; }

        camera_handle_t addCamera(const camera_t& camera);
        camera_t* getCamera(camera_handle_t handle);
        camera_t* getActiveCamera();
        bool setupCamera(camera_handle_t camera);
        int getVisibleActors(camera_handle_t camera, actor_t** actors);

        frame_buffer_handle_t getBackBuffer();
        VkSemaphore* getRenderCompleteSemaphore();
        core::render::descriptor_set_layout_t getGlobalsDescriptorSetLayout();
        core::render::descriptor_set_layout_t getObjectDescriptorSetLayout();
        core::render::descriptor_pool_t getDescriptorPool();

        void presentFrame();
        void update();

        material_t* getTextureBlitMaterial() { return materials_.get(textureBlit_); }

        void releaseCommandBuffer(const command_buffer_t* cmdBuffer);

      private:
        void createTextureBlitResources();
        void buildPresentationCommandBuffers();
        
        core::render::context_t context_;

        core::packed_freelist_t<actor_t> actors_;
        core::packed_freelist_t<camera_t> cameras_;
        core::packed_freelist_t<core::mesh::mesh_t> meshes_;        
        core::packed_freelist_t<material_t> materials_;
        core::packed_freelist_t<compute_material_t> computeMaterials_;
        core::packed_freelist_t<shader_t> shaders_;
        core::packed_freelist_t<render_target_t> renderTargets_;
        core::packed_freelist_t<frame_buffer_t> framebuffers_;
        
        frame_buffer_handle_t backBuffer_;
        camera_handle_t activeCamera_;
        actor_handle_t rootActor_;

        core::render::descriptor_set_layout_t globalsDescriptorSetLayout_;
        core::render::descriptor_set_layout_t objectDescriptorSetLayout_;
        core::render::descriptor_pool_t globalDescriptorPool_;

        core::transform_manager_t transformManager_;

        //Presentation pass resources
        bkk::core::mesh::mesh_t fullScreenQuad_;        
        bkk::core::render::descriptor_set_t presentationDescriptorSet_;        
        bkk::core::render::graphics_pipeline_t presentationPipeline_;

        //Texture blit resources
        material_handle_t textureBlit_;
        bkk::core::render::descriptor_set_layout_t textureBlitDescriptorSetLayout_;
        bkk::core::render::pipeline_layout_t textureBlitPipelineLayout_;
        bkk::core::render::shader_t textureBlitVertexShader_;
        bkk::core::render::shader_t textureBlitFragmentShader_;
        VkSemaphore renderComplete_;

        //Command buffers to be released on the next frame
        std::vector<command_buffer_t> releasedCommandBuffers_;
    };

  }//framework
}//bkk
#endif