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

#include "image.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"
#include <iostream>

using namespace bkk;
using namespace bkk::image;

static const char* getFileExtension(const char *filename) 
{
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}

bool image::load( const char* path, bool flipVertical, image2D_t* image )
{
  if( image->data_ != nullptr )
  {
    unload(image);
  }

  int width, height, componentCount;
  stbi_set_flip_vertically_on_load(flipVertical);

  uint8_t* data = nullptr;
  uint32_t componentSize = 0;

  if (strcmp(getFileExtension(path), "hdr") == 0 )
  {
    data = (uint8_t*)stbi_loadf(path, &width, &height, &componentCount, 0);
    componentSize = 4;
  }
  else
  {
    data = stbi_load(path, &width, &height, &componentCount, 0);    
    componentSize = 1;
  }

  if (data == nullptr)
  {
    return false;
  }

  image->width_ = width;
  image->height_ = height;
  image->componentCount_ = componentCount;
  image->componentSize_ = componentSize;
  image->dataSize_ = width * height * componentCount * componentSize;
  image->data_ = data;

 //Add missing channels, otherwise Vulkan validation layers will complain
  if( componentCount < 4 )
  {    
    image->componentCount_ = 4;
    image->dataSize_ = width * height * 4 * componentSize;
    image->data_ = (uint8_t*)malloc(image->dataSize_);

    if (componentSize == 1)
    {
      for (int i(0); i < width*height; ++i)
      { 
        for (int component(0); component<4; ++component)
          image->data_[4*i + component] = component < componentCount ? data[componentCount*i + component] : 0u;
      }
    }
    else if (componentSize == 4)
    {
      float* imageDataPtr = (float*)image->data_;
      float* dataPtr = (float*)data;
      for (int i(0); i < width*height; ++i)
      {
        for (int component(0); component<4; ++component)
          imageDataPtr[4*i + component] = component < componentCount ? dataPtr[componentCount*i + component] : 0.0f;
      }
    }

    free(data);
  }
  
  return true;
}

void image::unload( image2D_t* image )
{
  free( image->data_ );
  image->data_ = nullptr;
  image->width_ = image->height_ = image->componentCount_ = image->dataSize_ = 0u;
}
