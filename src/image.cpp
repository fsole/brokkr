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

using namespace bkk;
using namespace bkk::image;

bool image::load( const char* path, image2D_t* image )
{
  if( image->data_ != nullptr )
  {
    unload(image);
  }

  //stbi_set_flip_vertically_on_load( true ); // flip the image vertically, so the first pixel in the output array is the bottom left
  image->data_ = stbi_load(path, (int*)&image->width_, (int*)&image->height_, (int*)&image->componentCount_, 0);

  if( image->data_ == nullptr )
  {
    return false;
  }

  image->dataSize_ = image->width_ * image->height_ * image->componentCount_;
  return true;
}

void image::unload( image2D_t* image )
{
  free( image->data_ );
  image->data_ = nullptr;
  image->width_ = image->height_ = image->componentCount_ = image->dataSize_ = 0;
}
