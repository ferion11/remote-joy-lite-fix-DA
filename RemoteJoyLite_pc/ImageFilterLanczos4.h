#ifndef IMAGE_FILTER_LANCZOS4_H
#define IMAGE_FILTER_LANCZOS4_H

#include <atlbase.h>
#include "ImageFilter.h"

struct IDirect3DPixelShader9;

class ImageFilterLanczos4 : public ImageFilter {
public:
	ImageFilterLanczos4(IDirect3DDevice9* device);
	virtual ~ImageFilterLanczos4();
	virtual void set() const;

private:
	static CComPtr<IDirect3DPixelShader9> pixelShader;
};

#endif
