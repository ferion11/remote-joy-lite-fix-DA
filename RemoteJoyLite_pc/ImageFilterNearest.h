#ifndef IMAGE_FILTER_NEAREST_H
#define IMAGE_FILTER_NEAREST_H

#include "ImageFilter.h"

class ImageFilterNearest : public ImageFilter {
public:
	ImageFilterNearest();
	virtual ~ImageFilterNearest();
	virtual void set(IDirect3DDevice9* device);
};

#endif
