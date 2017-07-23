
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

  stbi_set_flip_vertically_on_load( true ); // flip the image vertically, so the first pixel in the output array is the bottom left
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
