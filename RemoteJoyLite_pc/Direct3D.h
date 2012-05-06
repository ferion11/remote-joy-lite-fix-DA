#ifndef _DIRECT3D_H_
#define _DIRECT3D_H_
/*------------------------------------------------------------------------------*/
/* Direct3D																		*/
/*------------------------------------------------------------------------------*/
#include <windows.h>
#include <d3d9.h>
#include <atlbase.h>

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

private:
	HWND hwnd;
	CComPtr<IDirect3D9> object;
	CComPtr<IDirect3DDevice9> device;
	int adapterIndex;

	AkindD3D( const AkindD3D& ){}
	AkindD3D &operator=( const AkindD3D& ){ return *this; }
	int getCurrentAdapterIndex() const;
	D3DPRESENT_PARAMETERS getPresentParameters(bool fullScreen) const;
};

#endif	// _DIRECT3D_H_
