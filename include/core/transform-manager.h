/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef TRANSFORM_MANAGER_H
#define TRANSFORM_MANAGER_H

#include "core/maths.h"
#include "core/packed-freelist.h"

#include <vector>

namespace bkk
{
  namespace core
  {
    class transform_manager_t
    {
    public:
      bkk_handle_t createTransform(const maths::mat4& transform);
      bool destroyTransform(bkk_handle_t id);

      maths::mat4* getTransform(bkk_handle_t id);
      bool setTransform(bkk_handle_t id, const maths::mat4& transform);

      bool setParent(bkk_handle_t id, bkk_handle_t parentId);
      bkk_handle_t getParent(bkk_handle_t id);

      maths::mat4* getWorldMatrix(bkk_handle_t id);

      void update();

    private:

      //Sorts transform by hierarchy level
      void sortTransforms();

      packed_freelist_t<maths::mat4> transform_;
      std::vector<bkk_handle_t> parent_;
      std::vector<maths::mat4> world_;

      bool hierarchy_changed_;
    };

  }//core
}//bkk
#endif  //  TRANSFORM_MANAGER_H