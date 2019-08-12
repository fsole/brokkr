/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdint.h>

#include "core/maths.h"
#include "core/render.h"
#include "core/handle.h"

#include "framework/shader.h"
#include "framework/frame-buffer.h"

namespace bkk
{
  namespace framework
  {
    typedef bkk::core::bkk_handle_t material_handle_t;

    class renderer_t;
    
    class material_t
    {
      public:
        material_t();
        material_t(shader_handle_t shader, renderer_t* renderer );

        bool setProperty(const char* property, float value);
        bool setProperty(const char* property, uint32_t value);
        bool setProperty(const char* property, const core::maths::vec2& value);
        bool setProperty(const char* property, const core::maths::vec3& value);
        bool setProperty(const char* property, const core::maths::vec4& value);
        bool setProperty(const char* property, const core::maths::mat3& value);
        bool setProperty(const char* property, const core::maths::mat4& value);
        bool setProperty(const char* property, void* value);
        
        bool setBuffer(const char* property, core::render::gpu_buffer_t buffer);
        bool setTexture(const char* property, core::render::texture_t texture );
        bool setTexture(const char* property, render_target_handle_t randerTarget);
        
        void destroy(renderer_t* renderer);

        core::render::graphics_pipeline_t getPipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer);
        core::render::descriptor_set_t getDescriptorSet(const char* pass = nullptr);
        core::render::descriptor_set_t getDescriptorSet(uint32_t pass);

        void updateDescriptorSets();

        shader_t* getShader();

      protected:
        renderer_t* renderer_;
        shader_handle_t shader_;

        std::vector<uint8_t*> bufferData_;
        std::vector<size_t> bufferDataSize_;
        std::vector<core::render::gpu_buffer_t> buffers_;
        std::vector<bool> bufferUpdate_;

        std::vector<core::render::descriptor_t> descriptors_;

        std::vector<core::render::descriptor_set_t> descriptorSet_;
        std::vector<bool> updateDescriptorSet_;
    };

  }//framework
}//bkk

#endif