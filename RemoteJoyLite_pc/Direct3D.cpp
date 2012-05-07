/*------------------------------------------------------------------------------*/
/* Direct3D																		*/
/*------------------------------------------------------------------------------*/
#include <windows.h>
#include <d3d9.h>
#include <dxerr.h>
#include "Direct3D.h"
#include "Log.h"

AkindD3D::AkindD3D(HWND hwnd) : hwnd(hwnd), adapterIndex(-1) {
}

AkindD3D::~AkindD3D()
{
	exit();
}

bool AkindD3D::initialize() {
	object = Direct3DCreate9(D3D_SDK_VERSION);
	if (object == NULL) {
		LOG(LOG_LEVEL_ERROR, "Failed to create direct3d object.");
	}
	return object != NULL;
}

void AkindD3D::exit() {
	release();
	object = NULL;
}

bool AkindD3D::create(bool fullScreen) {
	adapterIndex = getCurrentAdapterIndex();

	D3DPRESENT_PARAMETERS presentParameters = getPresentParameters(fullScreen);

	// Create the device.
	D3DDEVTYPE deviceTypes[] = {
		D3DDEVTYPE_HAL,
		D3DDEVTYPE_HAL,
		D3DDEVTYPE_REF,
	};
	DWORD behaviorFlags[] = {
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
	};

	for (int i = 0; i < 3; ++i) {
		HRESULT result;
		if (SUCCEEDED(result = object->CreateDevice(adapterIndex, deviceTypes[i], hwnd, behaviorFlags[i], &presentParameters, &device))) {
			break;
		}

		LOG(LOG_LEVEL_WARN, "Failed to create device. (%s)", DXGetErrorStringA(result));
	}

	if (device == NULL) {
		LOG(LOG_LEVEL_ERROR, "Failed to create device.");
		return NULL;
	}
	
	canvasSize.cx = presentParameters.BackBufferWidth;
	canvasSize.cy = presentParameters.BackBufferHeight;

	// Call create event handlers.
	for (std::vector<CREATE_EVENT_HANDLER>::iterator it = createEventHandlers.begin(), itEnd = createEventHandlers.end(); it != itEnd; ++it) {
		if (!(*it)(this)) {
			return false;
		}
	}

	return true;
}

void AkindD3D::release() {
	for (std::vector<RELEASE_EVENT_HANDLER>::iterator it = releaseEventHandlers.begin(), itEnd = releaseEventHandlers.end(); it != itEnd; ++it) {
		(*it)();
	}

	device = NULL;
}

IDirect3DDevice9 *AkindD3D::getDevice( void ) const {
	return device;
}

void AkindD3D::reset(bool fullScreen) {
	int currentAdapterIndex = getCurrentAdapterIndex();

	if (currentAdapterIndex == adapterIndex) {
		D3DPRESENT_PARAMETERS presentParameters = getPresentParameters(fullScreen);
		HRESULT result;
		if (FAILED(result = device->Reset(&presentParameters))) {
			LOG(LOG_LEVEL_ERROR, "Failed to reset the device. (%s)", DXGetErrorStringA(result));
		}

		canvasSize.cx = presentParameters.BackBufferWidth;
		canvasSize.cy = presentParameters.BackBufferHeight;

	} else {
		release();
		create(fullScreen);
	}
}

int AkindD3D::getCurrentAdapterIndex() const {
	int numberOfAdapters = object->GetAdapterCount();
	int currentAdapterIndex = 0;
	HMONITOR currentMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
	for (; currentAdapterIndex < numberOfAdapters; ++currentAdapterIndex) {
		HMONITOR monitor = object->GetAdapterMonitor(currentAdapterIndex);
		if (monitor == currentMonitor) {
			break;
		}
	}

	if (currentAdapterIndex == numberOfAdapters) {
		LOG(LOG_LEVEL_WARN, "Failed to find the proper adapter.");
		currentAdapterIndex = D3DADAPTER_DEFAULT;
	}

	return currentAdapterIndex;
}

D3DPRESENT_PARAMETERS AkindD3D::getPresentParameters(bool fullScreen) const {
	D3DPRESENT_PARAMETERS presentParameters = {0};
	presentParameters.SwapEffect           = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferCount      = 2;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	presentParameters.hDeviceWindow        = hwnd;

	if (fullScreen) {
		HRESULT result = NULL;
		D3DDISPLAYMODE displayMode = {0};
		if (FAILED(result = object->GetAdapterDisplayMode(adapterIndex, &displayMode))) {
			LOG(LOG_LEVEL_WARN, "Failed to get the adapter display mode. (%s)", DXGetErrorStringA(result));
		}

		presentParameters.Windowed         = FALSE;
		presentParameters.BackBufferWidth  = displayMode.Width;
		presentParameters.BackBufferHeight = displayMode.Height;
		presentParameters.BackBufferFormat = displayMode.Format;

	} else {
		presentParameters.Windowed         = TRUE;
	}

	return presentParameters;
}

void AkindD3D::addCreateEventHandler(CREATE_EVENT_HANDLER createEventHandler) {
	createEventHandlers.push_back(createEventHandler);
}

void AkindD3D::addReleaseEventHandler(RELEASE_EVENT_HANDLER releaseEventHandler) {
	releaseEventHandlers.push_back(releaseEventHandler);
}

SIZE AkindD3D::getCanvasSize() const {
	return canvasSize;
}
