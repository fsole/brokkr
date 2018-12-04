#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>

#include "core/maths.h"
#include "core/render.h"
#include "core/packed-freelist.h"

#include "framework/shader.h"

namespace bkk
{
  namespace framework
  {
    typedef bkk::core::handle_t material_handle_t;
    
    class renderer_t;

    class material_t
    {
      public:
        material_t();
        material_t(shader_handle_t shader, renderer_t* renderer );

        bool setProperty(const char* property, float value);
        bool setProperty(const char* property, const core::maths::vec2& value);
        bool setProperty(const char* property, const core::maths::vec3& value);
        bool setProperty(const char* property, const core::maths::vec4& value);
        bool setProperty(const char* property, const core::maths::mat3& value);
        bool setProperty(const char* property, const core::maths::mat4& value);
        bool setProperty(const char* property, void* value);
        
        
        bool setBuffer(const char* property, core::render::gpu_buffer_t buffer);
        bool setTexture(const char* property, core::render::texture_t texture );
        

        void destroy(renderer_t* renderer);

        core::render::graphics_pipeline_t getPipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer);
        core::render::descriptor_set_t getDescriptorSet();


      private:
        renderer_t* renderer_;
        shader_handle_t shader_;

        std::vector<uint8_t*> uniformData_;
        std::vector<core::render::gpu_buffer_t> uniformBuffers_;
        std::vector<bool> uniformBufferUpdate_;

        std::vector<core::render::descriptor_t> descriptors_;

        core::render::descriptor_set_t descriptorSet_;
        bool updateDescriptorSet_;
    };
  }
}

#endif