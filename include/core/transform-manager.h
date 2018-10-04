/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef TRANSFORM_MANAGER_H
#define TRANSFORM_MANAGER_H

#include "core/maths.h"
#include "core/packed-freelist.h"

namespace bkk
{
  namespace core
  {
    struct transform_manager_t
    {

      handle_t createTransform(const maths::mat4& transform);
      bool destroyTransform(handle_t id);

      maths::mat4* getTransform(handle_t id);
      bool setTransform(handle_t id, const maths::mat4& transform);

      bool setParent(handle_t id, handle_t parentId);
      handle_t getParent(handle_t id);

      maths::mat4* getWorldMatrix(handle_t id);

      void update();

    private:

      //Sorts transform by hierarchy level
      void sortTransforms();

      packed_freelist_t<maths::mat4> transform_;    ///< Local transforms
      dynamic_array_t<handle_t> parent_;  ///< Parent of each transform
      dynamic_array_t<maths::mat4> world_;     ///< World transforms

      bool hierarchy_changed_;                    ///< Flag to indicates that the hierarchy has changed since the last update
    };
  }//core namespace
}//bkk namespace
#endif  //  TRANSFORM_MANAGER_H