#ifndef SHADER_FRAMEWORK_H
#define SHADER_FRAMEWORK_H

#include <stdint.h>
#include <string>
#include <vector>

#include "core/maths.h"
#include "core/render.h"

namespace bkk
{
  namespace framework
  {
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
      bool shared_;
      std::vector<field_desc_t> fields_;
    };


    struct shader_t
    {
      shader_t();
      shader_t(const char* file, core::render::context_t* context);
      ~shader_t();
      
      bool initializeFromFile(const char* file, core::render::context_t* context);

    private:

      void clear();

      std::string name_;

      std::vector<texture_desc_t> textures_;
      std::vector<buffer_desc_t> buffers_;

      core::render::context_t* context_;

      std::vector<std::string> passNames_;
      std::vector<core::render::shader_t> vertexShaders_;
      std::vector<core::render::shader_t> fragmentShaders_;

      std::string glslHeader_;
    };
  }
}

#endif