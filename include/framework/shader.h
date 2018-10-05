#ifndef SHADER_FRAMEWORK_H
#define SHADER_FRAMEWORK_H

#include <stdint.h>
#include "core/maths.h"
#include "core/render.h"

namespace bkk
{
  namespace framework
  {
    struct shader_property_t
    {
      enum type {
        FLOAT = 0,
        VEC2,
        VEC3,
        VEC4,
        TEXTURE,
        BUFFER,
        TYPE_COUNT
      };

      type type;
      uint32_t id;
      uint32_t offset;
    };

    struct shader_property_layout_t
    {
      shader_property_layout_t():properties(nullptr), propertyCount(0) {}
      shader_property_t* properties;
      uint32_t propertyCount;
    };

    struct shader_t
    {
      struct pass_t
      {
        const char* name_;
        uint32_t id_;
      };

      shader_t();
      shader_t(const char* file);

      ~shader_t();
      
      char* name_;
      shader_property_layout_t properties_;
      core::render::shader_t shader_;
      core::render::context_t* context_;
    };
  }
}

#endif