#include <d3d9.h>
#include "ImageFilterNearest.h"

ImageFilterNearest::ImageFilterNearest(IDirect3DDevice9* device) : ImageFilter(device) {
}

ImageFilterNearest::~ImageFilterNearest() {
}

void ImageFilterNearest::set() const {
	device->SetPixelShader( NULL );
	device->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
	device->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
}
