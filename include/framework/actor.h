/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef ACTOR_H
#define ACTOR_H

#include "core/handle.h"
#include "core/render.h"

namespace bkk
{ 
  namespace framework
  {
    typedef core::bkk_handle_t mesh_handle_t;
    typedef core::bkk_handle_t transform_handle_t;
    typedef core::bkk_handle_t material_handle_t;
    typedef core::bkk_handle_t actor_handle_t;

    class renderer_t;

    class actor_t
    {   
    public:
      actor_t();

      actor_t(const char* name, 
              mesh_handle_t mesh, transform_handle_t transform, material_handle_t material,
              uint32_t instanceCount, renderer_t* renderer);
      
      void destroy(renderer_t* renderer);

      mesh_handle_t getMeshHandle() { return mesh_; }
      transform_handle_t getTransformHandle() { return transform_; }
      material_handle_t getMaterialHandle() { return material_; }
      const char* getName() { return name_.c_str();  }
      core::render::gpu_buffer_t getUniformBuffer() { return uniformBuffer_; }
      core::render::descriptor_set_t getDescriptorSet() { return descriptorSet_; }
      uint32_t getInstanceCount() { return instanceCount_; }
    private:
      std::string name_;
      mesh_handle_t mesh_;
      transform_handle_t transform_;
      material_handle_t material_;
      uint32_t instanceCount_;

      core::render::gpu_buffer_t uniformBuffer_;
      core::render::descriptor_set_t descriptorSet_;
    };

  }//framework
}//bkk

#endif
