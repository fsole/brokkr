/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/image.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

using namespace bkk::core;
using namespace bkk::core::image;

static const char* getFileExtension(const char *filename) 
{
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}

bool image::load( const char* path, bool flipVertical, image2D_t* image )
{
  if( image->data != nullptr )
  {
    free(image);
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

  image->width = width;
  image->height = height;
  image->componentCount = componentCount;
  image->componentSize = componentSize;
  image->dataSize = width * height * componentCount * componentSize;
  image->data = data;

 //Add missing channels, otherwise Vulkan validation layers will complain
  if( componentCount < 4 )
  {    
    image->componentCount = 4;
    image->dataSize = width * height * 4 * componentSize;
    image->data = (uint8_t*)malloc(image->dataSize);

    if (componentSize == 1)
    {
      for (int i(0); i < width*height; ++i)
      { 
        for (int component(0); component<4; ++component)
          image->data[4*i + component] = component < componentCount ? data[componentCount*i + component] : 0u;
      }
    }
    else if (componentSize == 4)
    {
      float* imageDataPtr = (float*)image->data;
      float* dataPtr = (float*)data;
      for (int i(0); i < width*height; ++i)
      {
        for (int component(0); component<4; ++component)
          imageDataPtr[4*i + component] = component < componentCount ? dataPtr[componentCount*i + component] : 0.0f;
      }
    }

    ::free(data);
  }
  
  return true;
}

void image::free( image2D_t* image )
{
  ::free( image->data );
  image->data = nullptr;
  image->width = image->height = image->componentCount = image->dataSize = 0u;
}
