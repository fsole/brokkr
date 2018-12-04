#ifndef ACTOR_H
#define ACTOR_H

#include "core/packed-freelist.h"
#include "core/render.h"

namespace bkk
{ 
  namespace framework
  {
    typedef core::handle_t mesh_handle_t;
    typedef core::handle_t transform_handle_t;
    typedef core::handle_t material_handle_t;
    typedef core::handle_t actor_handle_t;

    class renderer_t;

    struct actor_t
    {   
      actor_t();

      actor_t(const char* name, 
              mesh_handle_t mesh, transform_handle_t transform, material_handle_t material, 
              renderer_t* renderer);
      
      void destroy(renderer_t* renderer);

      mesh_handle_t getMesh();
      transform_handle_t getTransform();
      material_handle_t getMaterial();
      const char* getName();


      std::string name_;
      mesh_handle_t mesh_;
      transform_handle_t transform_;
      material_handle_t material_;
      core::render::gpu_buffer_t uniformBuffer_;
      core::render::descriptor_set_t descriptorSet_;
    };
  }
}

#endif
