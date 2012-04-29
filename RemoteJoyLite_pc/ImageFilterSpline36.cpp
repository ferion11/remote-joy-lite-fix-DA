#include <windows.h>
#include <atlbase.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
#include "ImageFilterSpline36.h"

extern const char* PIXEL_SHADER;

static void showError(HRESULT result, const char* message) {
	WCHAR buffer[1024];
	wsprintf(buffer, L"ImageFilterSpline36: %s\n%s", DXGetErrorString(result), message);
	MessageBox(NULL, buffer, L"RemoteJoyLite", MB_OK);
}

ImageFilterSpline36::ImageFilterSpline36(IDirect3DDevice9* device) : ImageFilter(device) {
	//const string spaceSearchRadiusString = boost::lexical_cast<string>(spaceSearchRadius);
	//const string timeSearchRadiusString = boost::lexical_cast<string>(timeSearchRadius);

	//D3DXMACRO macros[3] = {0};
	//macros[0].Name = "SPACE_SEARCH_RADIUS";
	//macros[0].Definition = spaceSearchRadiusString.c_str();
	//macros[1].Name = "TIME_SEARCH_RADIUS";
	//macros[1].Definition = timeSearchRadiusString.c_str();

	// Compile the pixel shader.
	CComPtr<ID3DXBuffer> buffer;
	CComPtr<ID3DXBuffer> errorMessage;
	D3DXMACRO macros[1] = {0};
	HRESULT result = NULL;

	if (FAILED(result = D3DXCompileShader(PIXEL_SHADER, strlen(PIXEL_SHADER), macros, NULL,
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

ImageFilterSpline36::~ImageFilterSpline36() {
}

void ImageFilterSpline36::set() const {
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

// fxc /T ps_3_0 /Eprocess /Fxspline36.html /Gfp /Cc spline36.psh
