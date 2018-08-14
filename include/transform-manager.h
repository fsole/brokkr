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

#include "maths.h"
#include "packed-freelist.h"

namespace bkk
{
  struct transform_manager_t
  {
    
    bkk::handle_t createTransform(const maths::mat4& transform);
    bool destroyTransform(bkk::handle_t id);
    
    maths::mat4* getTransform(bkk::handle_t id);
    bool setTransform(bkk::handle_t id, const maths::mat4& transform);

    bool setParent(bkk::handle_t id, bkk::handle_t parentId);
    bkk::handle_t getParent(bkk::handle_t id);

    maths::mat4* getWorldMatrix(bkk::handle_t id);

    void update();

  private:

    //Sorts transform by hierarchy level
    void sortTransforms();

    packed_freelist_t<maths::mat4> transform_;    ///< Local transforms
    bkk::dynamic_array_t<bkk::handle_t> parent_;  ///< Parent of each transform
    bkk::dynamic_array_t<maths::mat4> world_;     ///< World transforms

    bool hierarchy_changed_;                    ///< Flag to indicates that the hierarchy has changed since the last update
  };

}//namespace bkk
#endif  //  TRANSFORM_MANAGER_H