#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include <cstddef>

namespace bkk
{
	namespace image
	{
		struct image2D_t
		{
			uint32_t width_;
			uint32_t height_;
			uint32_t componentCount_;
			uint32_t dataSize_;
			uint8_t* data_;
		};

		bool load(const char* path, image2D_t* image);
		void unload(image2D_t* image);

	} //namespace image
}//namespace bkk
#endif /* IMAGE_H */