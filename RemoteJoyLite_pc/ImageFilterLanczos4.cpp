#include <windows.h>
#include <atlbase.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
#include "ImageFilterLanczos4.h"

extern const char* PIXEL_SHADER_LANCZOS4;

static void showError(HRESULT result, const char* message) {
	WCHAR buffer[1024];
	wsprintf(buffer, L"ImageFilterLanczos4: %s\n%s", DXGetErrorString(result), message);
	MessageBox(NULL, buffer, L"RemoteJoyLite", MB_OK);
}

CComPtr<IDirect3DPixelShader9> ImageFilterLanczos4::pixelShader;

ImageFilterLanczos4::ImageFilterLanczos4(IDirect3DDevice9* device) : ImageFilter(device) {
	if (pixelShader) {
		return;
	}

	// Compile the pixel shader.
	CComPtr<ID3DXBuffer> buffer;
	CComPtr<ID3DXBuffer> errorMessage;
	D3DXMACRO macros[1] = {0};
	HRESULT result = NULL;

	if (FAILED(result = D3DXCompileShader(PIXEL_SHADER_LANCZOS4, strlen(PIXEL_SHADER_LANCZOS4), macros, NULL,
		"process", "ps_3_0", D3DXSHADER_PREFER_FLOW_CONTROL, &buffer, &errorMessage, NULL))) {
		showError(result, (const char*)buffer->GetBufferPointer());
		return;
	}

	// Create the pixel shader.
	if (FAILED(result = device->CreatePixelShader((DWORD*)buffer->GetBufferPointer(), &pixelShader))) {
		showError(result, NULL);
		return;
	}
}

ImageFilterLanczos4::~ImageFilterLanczos4() {
}

void ImageFilterLanczos4::set() const {
	HRESULT result = NULL;

	if (FAILED(result = device->SetPixelShader(pixelShader))) {
		showError(result, NULL);
		return;
	}

	CComPtr<IDirect3DSurface9> surface;
	if (FAILED(result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface))) {
		showError(result, NULL);
		return;
	}

	D3DSURFACE_DESC surfaceDesc;
	memset(&surfaceDesc, 0, sizeof(surfaceDesc));
	if (FAILED(result = surface->GetDesc(&surfaceDesc))) {
		showError(result, NULL);
		return;
	}

	D3DXVECTOR4 pixelShaderConstant[1];
	memset(pixelShaderConstant, 0, sizeof(pixelShaderConstant));
	pixelShaderConstant[0].x = surfaceDesc.Width;
	pixelShaderConstant[0].y = surfaceDesc.Height;

	if (FAILED(result = device->SetPixelShaderConstantF(0, (float*)pixelShaderConstant, 1))) {
		showError(result, NULL);
		return;
	}

	device->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
	device->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
}

// fxc /T ps_3_0 /Eprocess /Fxlanczos4.html /Gfp /Cc lanczos4.psh
