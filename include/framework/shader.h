#ifndef SHADER_H
#define SHADER_H

#include <stdint.h>
#include <string>
#include <vector>

#include "core/maths.h"
#include "core/render.h"
#include "core/packed-freelist.h"
#include "core/hash-table.h"

#include "framework/frame-buffer.h"

namespace bkk
{
  namespace framework
  {
    class renderer_t;

    typedef bkk::core::handle_t shader_handle_t;

    struct texture_desc_t
    {
      enum type_e {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_ARRAY,
        TYPE_COUNT
      };

      std::string name_;
      type_e type_;
      int32_t binding_;
    };

    struct buffer_desc_t
    {
      struct field_desc_t
      {
        enum type_e {
          INT,
          FLOAT,
          VEC2,
          VEC3,
          VEC4,
          MAT4,
          COMPOUND_TYPE,
          TYPE_COUNT
        };

        std::string name_;
        type_e type_;
        uint32_t byteOffset_;
        uint32_t size_;
        uint32_t count_;  //0 means it's an array with no size defined "[]"
        std::vector<field_desc_t> fields_; //For compound types (fields composed of other fields)
      };

      enum type_e {
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        TYPE_COUNT
      };

      std::string name_;
      type_e type_;
      int32_t binding_;      
      uint32_t size_;
      bool shared_;
      std::vector<field_desc_t> fields_;
    };
    
    class shader_t
    {
      public:
        shader_t();
        shader_t(const char* file, renderer_t* renderer);
        ~shader_t();
      
        bool initializeFromFile(const char* file, renderer_t* renderer);
        void destroy(renderer_t* renderer);

        core::render::graphics_pipeline_t getPipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer);
        core::render::graphics_pipeline_t getPipeline(uint32_t pass, frame_buffer_handle_t, renderer_t* renderer);
        
        core::render::descriptor_set_layout_t getDescriptorSetLayout();
        const std::vector<texture_desc_t>& getTextureDescriptions() const;
        const std::vector<buffer_desc_t>& getBufferDescriptions() const;

      private:
        std::string name_;

        std::vector<texture_desc_t> textures_;
        std::vector<buffer_desc_t> buffers_;
        core::render::descriptor_set_layout_t descriptorSetLayout_;

        //Pass data
        std::vector<uint64_t> pass_;
        std::vector<core::render::shader_t> vertexShaders_;
        std::vector<core::render::shader_t> fragmentShaders_;
        std::vector<core::render::vertex_format_t> vertexFormats_;
        std::vector<core::render::pipeline_layout_t> pipelineLayouts_;
        std::vector<core::render::graphics_pipeline_t::description_t> graphicsPipelineDescriptions_;
        //std::vector<core::render::shader_t> computeShaders_;
        //std::vector<core::render::compute_pipeline_t> computePipelines_;

        core::hash_table_t<frame_buffer_handle_t, std::vector<core::render::graphics_pipeline_t> > graphicsPipelines_;
    };
  }
}

#endif