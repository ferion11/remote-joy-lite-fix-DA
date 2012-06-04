/*------------------------------------------------------------------------------*/
/* WinMain																		*/
/*------------------------------------------------------------------------------*/
#include <cstdio>
#include <string>
#include <memory>
#include <windows.h>
#include <dbt.h>
#include <vfw.h>
#include "Deleter.h"
#include "Direct3D.h"
#include "DirectInput.h"
#include "DebugFont.h"
#include "ImageFilterLanczos4.h"
#include "ImageFilterSpline36.h"
#include "Log.h"
#include "Setting.h"
#include "RemoteJoyLite.h"
#include "WinMain.h"
#include "Wave.h"
#include "Macro.h"

/*------------------------------------------------------------------------------*/
/* work																			*/
/*------------------------------------------------------------------------------*/
static const char* 	USB_RESET_STATUS_NAMES[] = {
	"USB_RESET_STATUS_CLOSING",
	"USB_RESET_STATUS_RESETTING",
	"USB_RESET_STATUS_OPENING",
	"USB_RESET_STATUS_OPENED",
};

static const GUID GUID_DEVINTERFACE_USB_DEVICE =
{ 0xA5DCBF10L, 0x6530, 0x11D2,
{ 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };

enum USB_RESET_DEVICE_STATUS {
	USB_RESET_DEVICE_STATUS_NONE,
	USB_RESET_DEVICE_STATUS_RESETTING,
};

static int SCREEN_W = 480;
static int SCREEN_H = 272;

static std::unique_ptr<AkindDI> pAkindDI;
static std::unique_ptr<AkindD3D> pAkindD3D;

static HWND hWndMain;
static BOOL LoopFlag  = TRUE;
static int FullScreen = 0;
static int StyleFlag  = 0;
static int MouseHiddenWaitCount = 0;
static int ScreenSaveCount = 0;
static volatile bool ResetUsb = false;
static volatile USB_RESET_DEVICE_STATUS UsbResetDeviceStatus = USB_RESET_DEVICE_STATUS_NONE;
static volatile USB_RESET_STATUS UsbResetStatus = USB_RESET_STATUS_OPENING;
static std::unique_ptr<HANDLE, HandleDeleter> multipleStartupMutex;
static std::unique_ptr<HDEVNOTIFY, DeviceNotificationDeleter> pspDeviceNotify;

/*------------------------------------------------------------------------------*/
/* GetUsbResetStatus															*/
/*------------------------------------------------------------------------------*/
USB_RESET_STATUS GetUsbResetStatus( void ) {
	return UsbResetStatus;
}

/*------------------------------------------------------------------------------*/
/* SetUsbResetStatus															*/
/*------------------------------------------------------------------------------*/
void SetUsbResetStatus( USB_RESET_STATUS status ) {
	LOG(LOG_LEVEL_INFO, "SetUsbResetStatus(): Changed to %s", USB_RESET_STATUS_NAMES[status]);
	UsbResetStatus = status;
}

/*------------------------------------------------------------------------------*/
/* WaitForUsbResetStatus														*/
/*------------------------------------------------------------------------------*/
void WaitForUsbResetStatus( USB_RESET_STATUS status ) {
	// TODO(nahanaha): Check the bug that the loop below exits with timeout.
	for ( int i = 0; GetUsbResetStatus() != status && i < 10; ++i ) {
		LOG(LOG_LEVEL_INFO, "WaitForUsbResetStatus(): Waiting for %s", USB_RESET_STATUS_NAMES[status]);
		Sleep(100);
	}
}

/*------------------------------------------------------------------------------*/
/* _GetLastError																*/
/*------------------------------------------------------------------------------*/
static const char* GetLastErrorString( void ) {
	static TCHAR buffer[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), buffer, sizeof(buffer) / sizeof(buffer[0]), NULL );
	static char errorString[1024];
	WideCharToMultiByte(CP_ACP, 0, buffer, -1, errorString, sizeof(errorString), NULL, NULL);
	return errorString;
}

/*------------------------------------------------------------------------------*/
/* _UsbResetDevice																*/
/*------------------------------------------------------------------------------*/
static void _UsbResetDevice( void ) {
	WaitForUsbResetStatus(USB_RESET_STATUS_RESETTING);

	LOG(LOG_LEVEL_INFO, "_UsbResetDevice()");

	PROCESS_INFORMATION pi = {0};
	STARTUPINFO si = {0};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	TCHAR commandLine[256];
	lstrcpy(commandLine, TEXT("ResetUsb.exe"));

	if (!CreateProcess(NULL, commandLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
		LOG(LOG_LEVEL_WARN, "UsbResetDevice(): CreateProcess(){reason=\"%s\"}", GetLastErrorString());
	}
	CloseHandle(pi.hThread);
	// TODO(nahanaha): Check the bug that ResetUsb.exe does not exit in some cases.
	WaitForSingleObject(pi.hProcess, 1 * 1000);
	CloseHandle(pi.hProcess);

	// Use notification to detect that PSP is connected.
	// SetUsbResetStatus(USB_RESET_STATUS_OPENING);
}

/*------------------------------------------------------------------------------*/
/* UsbResetDevice																*/
/*------------------------------------------------------------------------------*/
void UsbResetDevice( void ) {
	UsbResetDeviceStatus = USB_RESET_DEVICE_STATUS_RESETTING;
}

/*------------------------------------------------------------------------------*/
/* InitAll																		*/
/*------------------------------------------------------------------------------*/
static BOOL InitAll( HWND hWnd, HINSTANCE hInst )
{
	// Prevent multiple start-up.
	SECURITY_DESCRIPTOR sd = {0};
	InitializeSecurityDescriptor(&sd,SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, 0, FALSE);	    

	SECURITY_ATTRIBUTES secAttribute = {0};
	secAttribute.nLength = sizeof (secAttribute);
	secAttribute.lpSecurityDescriptor = &sd;
	secAttribute.bInheritHandle = TRUE; 

	multipleStartupMutex.reset(::CreateMutex(&secAttribute, FALSE, TEXT("RemoteJoyLite")));
	if (multipleStartupMutex == NULL) {
		LOG(LOG_LEVEL_ERROR, "Could not get mutex.");
		return FALSE;
	} else if (::GetLastError() == ERROR_ALREADY_EXISTS) {
		LOG(LOG_LEVEL_ERROR, "Already started.");
		return FALSE;
	}

	// Register device notification.
	DEV_BROADCAST_DEVICEINTERFACE filter;
	filter.dbcc_size       = sizeof(filter);
	filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	filter.dbcc_classguid  = GUID_DEVINTERFACE_USB_DEVICE;
	pspDeviceNotify.reset(::RegisterDeviceNotification( hWnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE));

	if ( SettingInit( hWnd, hInst )     == FALSE ){ return( FALSE ); }
	if (!pAkindD3D->initialize()) {
		return FALSE;
	}
	if (!pAkindD3D->create(false)) {
		return FALSE;
	}
	if ( SettingData.InputBG != 0 ){
		if ( pAkindDI->Init( hWnd, TRUE )  == FALSE ){ return( FALSE ); }
	} else {
		if ( pAkindDI->Init( hWnd, FALSE )  == FALSE ){ return( FALSE ); }
	}
	WaveInit();
	return( TRUE );
}

/*------------------------------------------------------------------------------*/
/* ExitAll																		*/
/*------------------------------------------------------------------------------*/
static void ExitAll( void )
{
	if (SettingData.PSPDisp != 0) {
		SettingData.PSPDisp = 0;
		RemoteJoyLite_SendPSPCmd();
	}

	pspDeviceNotify = NULL;
	multipleStartupMutex = NULL;
	WaveExit();
	pAkindDI->Exit();
	pAkindD3D->exit();
	SettingExit();
}

// Returns if the mouse cursor is showing.
static bool IsMouseShowing() {
	CURSORINFO cursorInfo = {0};
	cursorInfo.cbSize = sizeof(cursorInfo);
	GetCursorInfo(&cursorInfo);
	return cursorInfo.flags == CURSOR_SHOWING;
}

/*------------------------------------------------------------------------------*/
/* MainSync																		*/
/*------------------------------------------------------------------------------*/
void ChangeZoomMax( HWND hWnd );
static void MainSync( HWND hWnd )
{
	IDirect3DDevice9 *pD3DDev = pAkindD3D->getDevice();

	RECT SrcRect = { 0, 0, SCREEN_W, SCREEN_H };
	RECT DesRect = { 0, 0, SCREEN_W, SCREEN_H };

	pD3DDev->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0, 0 );
	pD3DDev->BeginScene();
	RemoteJoyLiteDraw( pAkindD3D.get() );
	DebugFontDraw( pAkindD3D.get() );
	pD3DDev->EndScene();

	HRESULT hResult;
	if ( (FullScreen != 0) && (SettingData.FullAdjust != 0) ){
		DesRect.left   = SettingData.FullRectX;
		DesRect.right  = SettingData.FullRectX + SettingData.FullRectW;
		DesRect.top    = SettingData.FullRectY;
		DesRect.bottom = SettingData.FullRectY + SettingData.FullRectH;
		hResult = pD3DDev->Present( &SrcRect, &DesRect, NULL, NULL );
	} else if ( SettingData.DispAspe == 0 ){
		hResult = pD3DDev->Present( NULL, NULL, NULL, NULL );
	} else {
		RECT rect;

		GetClientRect( hWnd, &rect );
		int disp_w = rect.right - rect.left;
		int disp_h = rect.bottom - rect.top;
		int calc_w = (disp_h*SCREEN_W)/SCREEN_H;
		int calc_h = (disp_w*SCREEN_H)/SCREEN_W;

		if ( calc_w > disp_w ){ calc_w = disp_w; }
		if ( calc_h > disp_h ){ calc_h = disp_h; }
		DesRect.left   = (disp_w - calc_w)/2;
		DesRect.right  = DesRect.left + calc_w;
		DesRect.top    = (disp_h - calc_h)/2;
		DesRect.bottom = DesRect.top + calc_h;
		hResult = pD3DDev->Present( &SrcRect, &DesRect, NULL, NULL );
	}

	if (hResult == D3DERR_DEVICELOST) {
		if (pD3DDev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
			if (FullScreen) {
				LOG(LOG_LEVEL_WARN, "MainSync() fullscreen.");
				pAkindD3D->reset(true);
			} else {
				LOG(LOG_LEVEL_WARN, "MainSync() windowed.");
				pAkindD3D->reset(false);
			}
		}
	}

	pAkindDI->Sync();
	SettingSync( pAkindDI.get() );
	MacroSync( pAkindDI.get() );
	RemoteJoyLiteSync();
	WaveSync();

	if ( SettingData.InputBG != 0 ){
		if ( ScreenSaveCount++ > 50*60 ){
			keybd_event( 0x88, 0, KEYEVENTF_KEYUP, 0 );
			ScreenSaveCount = 0;
		}
	}

	if ( IsSettingDialogShowing() ){
		if (!IsMouseShowing()) {
			ShowCursor(TRUE);
		}
	} else {
		if (IsMouseShowing()) {
			++MouseHiddenWaitCount;
			if (MouseHiddenWaitCount == 60) {
				ShowCursor(FALSE);
			}
		}
	}

	// The restart and the open of PSP device always fails during fullscreen mode.
	// We have to change the device to windowed mode during the restart and the open.

	// Reset the usb device.
	if ( UsbResetDeviceStatus == USB_RESET_DEVICE_STATUS_RESETTING ) {
		// Change the device to windowed mode temporary.
		if ( FullScreen ) {
			LOG(LOG_LEVEL_WARN, "MainSync() windowed.");
			pAkindD3D->reset(false);
		}
		_UsbResetDevice();
		UsbResetDeviceStatus = USB_RESET_DEVICE_STATUS_NONE;
	}

	// We cannot use WaitForUsbResetStatus() because the state will be changed
	// to USB_RESET_STATUS_OPENED in the main loop.
	if (FullScreen &&
		!IsSettingDialogShowing() &&
		!pAkindD3D->isFullScreenMode() &&
		GetUsbResetStatus() == USB_RESET_STATUS_OPENED &&
		GetFocus() == hWnd) {
		LOG(LOG_LEVEL_WARN, "MainSync() fullscreen.");
		pAkindD3D->reset(true);
	}
}

/*------------------------------------------------------------------------------*/
/* ChangeZoomMax																*/
/*------------------------------------------------------------------------------*/
void ChangeZoomMax( HWND hWnd )
{
	static RECT PrevRect;
	if ( FullScreen == 0 ){
		GetWindowRect( hWnd, &PrevRect );
		HMONITOR currentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO monitorInfo = {0};
		monitorInfo.cbSize = sizeof(monitorInfo);
		GetMonitorInfo(currentMonitor, &monitorInfo);
		int X = monitorInfo.rcMonitor.left;
		int Y = monitorInfo.rcMonitor.top;
		int cx = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
		int cy = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
		SetWindowLong( hWnd, GWL_STYLE, WS_POPUP );
		SetWindowPos( hWnd, HWND_TOPMOST, X, Y, cx, cy, SWP_SHOWWINDOW );
		FullScreen = 1;
		LOG(LOG_LEVEL_WARN, "ChangeZoomMax() fullscreen.");
		pAkindD3D->reset(true);
	} else {
		int x = PrevRect.left;
		int y = PrevRect.top;
		int w = PrevRect.right - PrevRect.left;
		int h = PrevRect.bottom - PrevRect.top;
		if ( StyleFlag == 0 ){
			SetWindowLong( hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW );
		} else {
			SetWindowLong( hWnd, GWL_STYLE, WS_POPUP );
		}
		SetWindowPos( hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW );
		if ( SettingData.DispTop == 0 ){
			SetWindowPos( hWnd, HWND_NOTOPMOST, x, y, w, h, 0 );
		} else {
			SetWindowPos( hWnd, HWND_TOPMOST, x, y, w, h, 0 );
		}
		ShowWindow( hWnd, SW_SHOWNORMAL );
		FullScreen = 0;
		LOG(LOG_LEVEL_WARN, "ChangeZoomMax() windowed.");
		pAkindD3D->reset(false);
	}
}

/*------------------------------------------------------------------------------*/
/* InitWindowSize																*/
/*------------------------------------------------------------------------------*/
static void InitWindowSize( HWND hWnd )
{
	RECT rect;

	int x = SettingData.DispRectX;
	int y = SettingData.DispRectY;

	if ( SettingData.DispRot & 1 ){
		SetRect( &rect, 0, 0, SettingData.DispRectH, SettingData.DispRectW );
	} else {
		SetRect( &rect, 0, 0, SettingData.DispRectW, SettingData.DispRectH );
	}
	AdjustWindowRect( &rect, WS_OVERLAPPEDWINDOW, FALSE );
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	if ( SettingData.DispTop == 0 ){
		SetWindowPos( hWnd, HWND_NOTOPMOST, x, y, w, h, 0 );
	} else {
		SetWindowPos( hWnd, HWND_NOTOPMOST, x, y, w, h, 0 );
	}
	if ( SettingData.DispSize == 2 ){ ShowWindow( hWnd, SW_SHOWMINIMIZED ); }
	else							{ ShowWindow( hWnd, SW_SHOWNORMAL );    }
	if ( SettingData.DispSize == 1 ){ ChangeZoomMax( hWnd ); }
}

/*------------------------------------------------------------------------------*/
/* ChangeWindowStyle															*/
/*------------------------------------------------------------------------------*/
static void ChangeWindowStyle( HWND hWnd )
{
	if ( FullScreen != 0 ){ return; }
	if ( StyleFlag == 0 ){
		RECT NowRect, ChgRect;
		GetWindowRect( hWnd, &NowRect );
		GetClientRect( hWnd, &ChgRect );
		AdjustWindowRect( &ChgRect, WS_POPUP, FALSE );
		int w = ChgRect.right - ChgRect.left;
		int h = ChgRect.bottom - ChgRect.top;
		int x = NowRect.left + GetSystemMetrics( SM_CXSIZEFRAME );
		int y = NowRect.top  + GetSystemMetrics( SM_CYSIZEFRAME ) + GetSystemMetrics( SM_CYCAPTION );
		SetWindowLong( hWnd, GWL_STYLE, WS_POPUP );
		SetWindowPos( hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW );
		if ( SettingData.DispTop == 0 ){
			SetWindowPos( hWnd, HWND_NOTOPMOST, x, y, w, h, SWP_SHOWWINDOW );
		} else {
			SetWindowPos( hWnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW );
		}
		ShowWindow( hWnd, SW_SHOWNORMAL );
		StyleFlag = 1;
	} else {
		RECT NowRect, ChgRect;
		GetWindowRect( hWnd, &NowRect );
		GetClientRect( hWnd, &ChgRect );
		AdjustWindowRect( &ChgRect, WS_OVERLAPPEDWINDOW, FALSE );
		int w = ChgRect.right - ChgRect.left;
		int h = ChgRect.bottom - ChgRect.top;
		int x = NowRect.left - GetSystemMetrics( SM_CXSIZEFRAME );
		int y = NowRect.top  - GetSystemMetrics( SM_CYSIZEFRAME ) - GetSystemMetrics( SM_CYCAPTION );
		SetWindowLong( hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW );
		SetWindowPos( hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW );
		if ( SettingData.DispTop == 0 ){
			SetWindowPos( hWnd, HWND_NOTOPMOST, x, y, w, h, SWP_SHOWWINDOW );
		} else {
			SetWindowPos( hWnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW );
		}
		ShowWindow( hWnd, SW_SHOWNORMAL );
		StyleFlag = 0;
	}
}

static bool IsPspDevice(DEV_BROADCAST_HDR* broadcastHdr) {
	if (broadcastHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) {
		return false;
	}

	DEV_BROADCAST_DEVICEINTERFACE* broadcastDeviceInterface = (DEV_BROADCAST_DEVICEINTERFACE*)broadcastHdr;
	if (broadcastDeviceInterface->dbcc_classguid != GUID_DEVINTERFACE_USB_DEVICE) {
		return false;
	}

	// Filter only PSP.
	std::wstring name = broadcastDeviceInterface->dbcc_name;
	if (name.find(L"VID_054C&PID_01C9") == std::wstring::npos) {
		return false;
	}

	return true;
}

/*------------------------------------------------------------------------------*/
/* WndProc																		*/
/*------------------------------------------------------------------------------*/
static LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	SettingProc( msg, wParam, lParam );
	pAkindDI->Message( msg, wParam, lParam );

	switch ( msg ){
	case WM_KEYDOWN:
		switch( wParam ){
		case VK_F1  : RemoteJoyLite_ToggleDebug();	break;
		case VK_F3  : RemoteJoyLite_ToggleDisp();	break;
		case VK_F4  : ChangeWindowStyle( hWnd );	break;
		case VK_F5  : ResetUsb = true;              break;
		case VK_F9  : MacroRecodeToggle();			break;
		case VK_F11 : RemoteJoyLite_SaveBitmap();	break;
		case VK_F12 : RemoteJoyLite_SaveMovie();	break;
		}
		break;
	case WM_MENUCHAR:
		return( MNC_CLOSE << 16 );
	case WM_NCMOUSEMOVE:
	case WM_MOUSEMOVE:
		if (!IsMouseShowing()) {
			ShowCursor(TRUE);
		}
		MouseHiddenWaitCount = 0;
		break;
	case WM_LBUTTONDBLCLK:
		ChangeZoomMax( hWnd );
		break;
	case WM_SYSKEYDOWN:
		if ( wParam == VK_RETURN ){ ChangeZoomMax( hWnd ); }
		break;
	case WM_SYSCOMMAND:
		if ( wParam == SC_SCREENSAVE ){ return( 1 ); }
		break;
	case WM_DESTROY:
		LoopFlag = FALSE;
		PostQuitMessage( 0 );
		break;
	case WM_SIZE:
		if ( FullScreen == 0 && !IsIconic(hWnd) ) {
			LOG(LOG_LEVEL_WARN, "WndProc() windowed.");
			pAkindD3D->reset(false);
		}
		break;
	case WM_DEVICECHANGE:
		if (wParam == DBT_DEVICEARRIVAL && IsPspDevice((DEV_BROADCAST_HDR*)lParam)) {
			if (FullScreen) {
				LOG(LOG_LEVEL_WARN, "WndProc() windowed.");
				pAkindD3D->reset(false);
			}
			SetUsbResetStatus(USB_RESET_STATUS_OPENING);
		}
	}
	return( DefWindowProc( hWnd, msg, wParam, lParam ) );
}

/*------------------------------------------------------------------------------*/
/* WinMain																		*/
/*------------------------------------------------------------------------------*/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR lpszCmdLine, int nCmdShow )
{
	HWND hWnd;
	WCHAR ClsName[] = L"RemoteJoyLiteWindow";

	if ( hPreInst == NULL ){
		WNDCLASS WndCls;

		WndCls.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		WndCls.lpfnWndProc   = WndProc;
		WndCls.cbClsExtra    = 0;
		WndCls.cbWndExtra    = 0;
		WndCls.hInstance     = hInstance;
		WndCls.hIcon         = LoadIcon( hInstance, L"REMOTEJOYLITE_ICON" );
		WndCls.hCursor       = LoadCursor( NULL, IDC_ARROW );
		WndCls.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
		WndCls.lpszMenuName  = NULL;
		WndCls.lpszClassName = ClsName;
		if ( RegisterClass(&WndCls) == FALSE ){ return( FALSE ); }
	}

	SettingLoad();
	MacroLoad();
	if ( SettingData.DispRot & 1 ){
		SCREEN_W = 272;
		SCREEN_H = 480;
	} else {
		SCREEN_W = 480;
		SCREEN_H = 272;
	}
	RECT rect = { 0, 0, SCREEN_W, SCREEN_H };

	AdjustWindowRect( &rect, WS_OVERLAPPEDWINDOW, FALSE );

	hWnd = CreateWindow( ClsName,
						 L"RemoteJoyLite Ver0.19",
						 WS_OVERLAPPEDWINDOW,
						 CW_USEDEFAULT,
						 CW_USEDEFAULT,
						 rect.right - rect.left,
						 rect.bottom - rect.top,
						 NULL,
						 NULL,
						 hInstance,
						 NULL );

	hWndMain = hWnd;

	pAkindDI = std::unique_ptr<AkindDI>(new AkindDI());
	pAkindD3D = std::unique_ptr<AkindD3D>(new AkindD3D(hWnd));
	pAkindD3D->addCreateEventHandler(RemoteJoyLiteInit);
	pAkindD3D->addReleaseEventHandler(RemoteJoyLiteExit);
	pAkindD3D->addCreateEventHandler(DebugFontInit);
	pAkindD3D->addReleaseEventHandler(DebugFontExit);
	pAkindD3D->addReleaseEventHandler(ImageFilterLanczos4::release);
	pAkindD3D->addReleaseEventHandler(ImageFilterSpline36::release);

	if ( InitAll( hWnd, hInstance ) == FALSE ){
		ExitAll();
		return( FALSE );
	}

	CreateDirectory( L".\\Capture", NULL );
	CreateDirectory( L".\\Macro", NULL );

	UpdateWindow( hWnd );
	InitWindowSize( hWnd );

	MSG msg;
	while ( LoopFlag != FALSE ){
		MainSync( hWnd );
		while ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ){
			GetMessage( &msg, NULL, 0, 0 );
			if ( SettingMessage( &msg, FullScreen, *pAkindD3D ) != FALSE ){ continue; }
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
	ExitAll();
	return( msg.wParam );
}

/********************************************************************************/
/*------------------------------------------------------------------------------*/
/* ChangeAspect																	*/
/*------------------------------------------------------------------------------*/
void ChangeAspect( void )
{
	IDirect3DDevice9 *pD3DDev = pAkindD3D->getDevice();
	pD3DDev->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0, 0 );
	pD3DDev->Present( NULL, NULL, NULL, NULL );
}

/*------------------------------------------------------------------------------*/
/* ChangeDispTop																*/
/*------------------------------------------------------------------------------*/
void ChangeDispTop( void )
{
	if ( SettingData.DispTop == 0 ){
		SetWindowPos( hWndMain, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
	} else {
		SetWindowPos( hWndMain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
	}
}

/*------------------------------------------------------------------------------*/
/* GetResetUsbAndReset															*/
/*------------------------------------------------------------------------------*/
bool GetResetUsbAndReset( void ) {
	bool result = ResetUsb;
	ResetUsb = false;
	return result;
}
