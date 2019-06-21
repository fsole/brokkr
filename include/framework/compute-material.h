/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef COMPUTE_MATERIAL_H
#define COMPUTE_MATERIAL_H

#include "framework/material.h"

namespace bkk
{
  namespace framework
  {
    typedef bkk::core::bkk_handle_t compute_material_handle_t;

    class compute_material_t : public material_t
    {
    public:
      compute_material_t();
      compute_material_t(shader_handle_t shader, renderer_t* renderer);

      void dispatch(core::render::command_buffer_t& commandBuffer, const char* passName, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);
      void dispatch(core::render::command_buffer_t& commandBuffer, uint32_t pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);
    };

  }//framework
}//bkk

#endif