#ifndef _WINMAIN_H_
#define _WINMAIN_H_
/*------------------------------------------------------------------------------*/
/* WinMain																		*/
/*------------------------------------------------------------------------------*/

class AkindD3D;

enum USB_RESET_STATUS {
	USB_RESET_STATUS_CLOSING,
	USB_RESET_STATUS_RESETTING,
	USB_RESET_STATUS_OPENING,
	USB_RESET_STATUS_OPENED,
};

/*------------------------------------------------------------------------------*/
/* prototype																	*/
/*------------------------------------------------------------------------------*/
extern void ChangeAspect( void );
extern void ChangeDispTop( void );
extern int GetCanvasWidth( void );
extern int GetCanvasHeight( void );
extern bool GetResetUsbAndReset();
extern void UsbResetDevice();
extern void SetUsbResetStatus(USB_RESET_STATUS status);
extern void WaitForUsbResetStatus(USB_RESET_STATUS status);
extern void ChangeZoomMax( HWND hWnd );
extern const AkindD3D& GetAkindD3D( void );

#endif	// _WINMAIN_H_
