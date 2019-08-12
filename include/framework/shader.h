/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef SHADER_H
#define SHADER_H

#include <stdint.h>
#include <string>
#include <vector>

#include "core/maths.h"
#include "core/render.h"
#include "core/handle.h"
#include "core/dictionary.h"

#include "framework/frame-buffer.h"

namespace bkk
{
  namespace framework
  {
    class renderer_t;

    typedef bkk::core::bkk_handle_t shader_handle_t;

    struct texture_desc_t
    {
      enum type_e {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_CUBE,
        TEXTURE_ARRAY,
        TEXTURE_STORAGE_IMAGE,
        TYPE_COUNT
      };

      enum format_e
      {
         FORMAT_RGBA8I,
         FORMAT_RGBA8UI,
         FORMAT_RGBA32I,
         FORMAT_RGBA32UI,
         FORMAT_RGBA32F
      };

      std::string name;
      type_e type;
      format_e format;
      int32_t binding;
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

        std::string name;
        type_e type;
        uint32_t byteOffset;
        uint32_t size;
        uint32_t count;  //0 means it's an array with no size defined "[]"
        std::vector<field_desc_t> fields; //For compound types (fields composed of other fields)
      };

      enum type_e {
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        TYPE_COUNT
      };

      std::string name;
      type_e type;
      int32_t binding;      
      uint32_t size;
      bool shared;
      std::vector<field_desc_t> fields;
    };
    
    class shader_t
    {
    public:
      shader_t();
      shader_t(const char* file, renderer_t* renderer);
      ~shader_t();
      
      bool initializeFromFile(const char* file, renderer_t* renderer);
      void destroy(renderer_t* renderer);

      void preparePipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer);
      core::render::graphics_pipeline_t getPipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer);
      core::render::graphics_pipeline_t getPipeline(uint32_t pass, frame_buffer_handle_t, renderer_t* renderer);
        
      core::render::descriptor_set_layout_t getDescriptorSetLayout();
      const std::vector<texture_desc_t>& getTextureDescriptions() const;
      const std::vector<buffer_desc_t>& getBufferDescriptions() const;

      uint32_t getPassCount() const{return (uint32_t)pass_.size();}
      uint32_t getPassIndexFromName(const char* pass) const;

      core::render::compute_pipeline_t getComputePipeline(const char* name);
      core::render::compute_pipeline_t getComputePipeline(uint32_t pass);

      core::render::pipeline_layout_t getPipelineLayout(const char* name);
      core::render::pipeline_layout_t getPipelineLayout(uint32_t pass);

    private:
      std::string name_;

      std::vector<texture_desc_t> textures_;
      std::vector<buffer_desc_t> buffers_;
      core::render::descriptor_set_layout_t descriptorSetLayout_;

      //Pass data
      std::vector<uint64_t> pass_;
      std::vector<core::render::shader_t> vertexShaders_;
      std::vector<core::render::shader_t> fragmentShaders_;
      std::vector<core::render::shader_t> computeShaders_;
      std::vector<core::render::vertex_format_t> vertexFormats_;
      std::vector<core::render::pipeline_layout_t> pipelineLayouts_;
      std::vector<core::render::graphics_pipeline_t::description_t> graphicsPipelineDescriptions_;

      core::dictionary_t<frame_buffer_handle_t, std::vector<core::render::graphics_pipeline_t> > graphicsPipelines_;
      std::vector<core::render::compute_pipeline_t> computePipelines_;
    };

  }//framework
}//bkk

#endif