#ifndef _DIRECT3D_H_
#define _DIRECT3D_H_
/*------------------------------------------------------------------------------*/
/* Direct3D																		*/
/*------------------------------------------------------------------------------*/
#include <vector>
#include <windows.h>
#include <d3d9.h>
#include <atlbase.h>

class AkindD3D;
typedef BOOL (*CREATE_EVENT_HANDLER)(AkindD3D *pAkindD3D);
typedef void (*RELEASE_EVENT_HANDLER)();

/*------------------------------------------------------------------------------*/
/* class																		*/
/*------------------------------------------------------------------------------*/
class AkindD3D {
public:
	AkindD3D(HWND hWnd);
	virtual ~AkindD3D();
	// Initializes the direct3d object.
	bool initialize();
	// Initializes all the resources.
	bool create(bool fullScreen);
	// Releases all the resources except the direct3d object.
	void release();
	// Releases all the resources.
	void exit();
	// Resets the display mode. Recreates the direct3d device if monitor was changed.
	void reset( bool fullScreen );
	// Returns the direct3d object.
	IDirect3DDevice9 *getDevice( void ) const;
	// Adds a create event handler. This event handler is called after
	// create() is called and after the device is created.
	void addCreateEventHandler(CREATE_EVENT_HANDLER createEventHandler);
	// Adds a release event handler. This event handler is called after
	// release() is called and before the device is released.
	void addReleaseEventHandler(RELEASE_EVENT_HANDLER releaseEventHandler);
	SIZE getCanvasSize() const;
	bool isFullScreenMode() const;

private:
	HWND hwnd;
	CComPtr<IDirect3D9> object;
	CComPtr<IDirect3DDevice9> device;
	int adapterIndex;
	std::vector<CREATE_EVENT_HANDLER> createEventHandlers;
	std::vector<RELEASE_EVENT_HANDLER> releaseEventHandlers;
	SIZE canvasSize;
	bool fullScreenMode;

	AkindD3D( const AkindD3D& ){}
	AkindD3D &operator=( const AkindD3D& ){ return *this; }
	int getCurrentAdapterIndex() const;
	D3DPRESENT_PARAMETERS getPresentParameters(bool fullScreen) const;
};

#endif	// _DIRECT3D_H_
