#ifndef MATERIAL_FRAMEWORK_H
#define MATERIAL_FRAMEWORK_H

#include <stdint.h>
#include "core/maths.h"
#include "core/render.h"
#include "framework/shader.h"

namespace bkk
{
  namespace framework
  {
    struct material_t
    {
      //material_t(shader_t shader);
      bool setProperty(uint32_t id, void* value);

    private:
      shader_property_layout_t propertyLayout_;
      
      void* uniformData_;      
      core::render::gpu_buffer_t uniformBuffer_;
    };
  }
}

#endif