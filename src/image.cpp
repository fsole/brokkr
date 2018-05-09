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
#include "stb-image.h"
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
  if (strcmp("hdr", getFileExtension(path)) == 0 )
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

 
  if( componentSize == 1 )
  {
    //Add missing channels.
    //TODO: Figure out how to handle textures with less than 4 channels :)
    if(componentCount == 3)
    {    
      image->componentCount_ = 4;
      image->dataSize_ = width * height * 4;
      image->data_ = (uint8_t*)malloc(image->dataSize_);
      for (int i(0); i < width*height; ++i )
      {
        image->data_[4*i] =   data[3*i];
        image->data_[4*i+1] = data[3*i+1];
        image->data_[4*i+2] = data[3*i+2];
        image->data_[4*i+3] = 0u;
      }

      free( data );
    }
    else if (componentCount == 2)
    {
      image->componentCount_ = 4;
      image->dataSize_ = width * height * 4;
      image->data_ = (uint8_t*)malloc(image->dataSize_);
      for (int i(0); i < width*height; ++i)
      {
        image->data_[4 * i] = data[2 * i];
        image->data_[4 * i + 1] = data[2 * i + 1];
        image->data_[4 * i + 2] = 0u;
        image->data_[4 * i + 3] = 0u;
      }

      free(data);
    }
    else if (componentCount == 1)
    {
      image->componentCount_ = 4;
      image->dataSize_ = width * height * 4;
      image->data_ = (uint8_t*)malloc(image->dataSize_);
      for (int i(0); i < width*height; ++i)
      {
        image->data_[4 * i] = data[i];
        image->data_[4 * i + 1] = 0u;
        image->data_[4 * i + 2] = 0u;
        image->data_[4 * i + 3] = 0u;
      }

      free(data);
    }
  }
  
  return true;
}

void image::unload( image2D_t* image )
{
  free( image->data_ );
  image->data_ = nullptr;
  image->width_ = image->height_ = image->componentCount_ = image->dataSize_ = 0u;
}
