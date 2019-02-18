/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <stddef.h>

namespace bkk
{
  namespace core
  {
    namespace image
    {
      struct image2D_t
      {
        uint32_t width;
        uint32_t height;
        uint32_t componentCount;
        uint32_t componentSize;
        uint32_t dataSize;
        uint8_t* data;
      };

      bool load(const char* path, bool flipVertical, image2D_t* image);
      void unload(image2D_t* image);

    } //image
  }//core
}//bkk

#endif /* IMAGE_H */