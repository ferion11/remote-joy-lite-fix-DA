#ifndef IMAGE_FILTER_SPLINE36_H
#define IMAGE_FILTER_SPLINE36_H

#include "ImageFilter.h"

class ImageFilterSpline36 : public ImageFilter {
public:
	ImageFilterSpline36(IDirect3DDevice9* device);
	virtual ~ImageFilterSpline36();
	virtual void set() const;

private:
	static CComPtr<IDirect3DPixelShader9> pixelShader;
};

#endif
