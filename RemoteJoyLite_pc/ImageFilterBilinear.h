#ifndef IMAGE_FILTER_BILINEAR_H
#define IMAGE_FILTER_BILINEAR_H

#include "ImageFilter.h"

class ImageFilterBilinear : public ImageFilter {
public:
	ImageFilterBilinear();
	virtual ~ImageFilterBilinear();
	virtual void set(IDirect3DDevice9* device);
};

#endif
