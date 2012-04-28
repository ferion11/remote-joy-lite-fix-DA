#include <d3d9.h>
#include "ImageFilterBilinear.h"

ImageFilterBilinear::ImageFilterBilinear(IDirect3DDevice9* device) : ImageFilter(device) {
}

ImageFilterBilinear::~ImageFilterBilinear() {
}

void ImageFilterBilinear::set() const {
	device->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	device->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
}
