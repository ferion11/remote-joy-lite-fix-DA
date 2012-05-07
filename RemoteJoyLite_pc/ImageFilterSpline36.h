#ifndef IMAGE_FILTER_SPLINE36_H
#define IMAGE_FILTER_SPLINE36_H

#include "ImageFilterPixelShaderBase.h"

class ImageFilterSpline36 : public ImageFilterPixelShaderBase {
public:
	ImageFilterSpline36();
	virtual ~ImageFilterSpline36();
	virtual const CComPtr<IDirect3DPixelShader9>& getPixelShaderCache();
	virtual void setPixelShaderCache(const CComPtr<IDirect3DPixelShader9>& pixelShader);
	static void release();

private:
	static CComPtr<IDirect3DPixelShader9> pixelShaderCache;
};

#endif
