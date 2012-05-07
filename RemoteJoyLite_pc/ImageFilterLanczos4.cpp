#include <windows.h>
#include <atlbase.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
#include "ImageFilterLanczos4.h"

extern const char* PIXEL_SHADER_LANCZOS4;
CComPtr<IDirect3DPixelShader9> ImageFilterLanczos4::pixelShaderCache;

ImageFilterLanczos4::ImageFilterLanczos4() : ImageFilterPixelShaderBase(PIXEL_SHADER_LANCZOS4) {
}

ImageFilterLanczos4::~ImageFilterLanczos4() {
}

const CComPtr<IDirect3DPixelShader9>& ImageFilterLanczos4::getPixelShaderCache() {
	return pixelShaderCache;
}

void ImageFilterLanczos4::setPixelShaderCache(const CComPtr<IDirect3DPixelShader9>& pixelShader) {
	pixelShaderCache = pixelShader;
}

void ImageFilterLanczos4::release() {
	pixelShaderCache = NULL;
}
