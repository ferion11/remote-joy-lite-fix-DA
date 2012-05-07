#ifndef IMAGE_FILTER_LANCZOS4_H
#define IMAGE_FILTER_LANCZOS4_H

#include <atlbase.h>
#include "ImageFilterPixelShaderBase.h"

struct IDirect3DPixelShader9;

class ImageFilterLanczos4 : public ImageFilterPixelShaderBase {
public:
	ImageFilterLanczos4();
	virtual ~ImageFilterLanczos4();
	virtual const CComPtr<IDirect3DPixelShader9>& getPixelShaderCache();
	virtual void setPixelShaderCache(const CComPtr<IDirect3DPixelShader9>& pixelShader);
	static void release();

private:
	static CComPtr<IDirect3DPixelShader9> pixelShaderCache;
};

#endif
