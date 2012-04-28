#ifndef IMAGE_FILTER_NEAREST_H
#define IMAGE_FILTER_NEAREST_H

#include "ImageFilter.h"

class ImageFilterNearest : public ImageFilter {
public:
	ImageFilterNearest(IDirect3DDevice9* device);
	virtual ~ImageFilterNearest();
	virtual void set() const;
};

#endif
