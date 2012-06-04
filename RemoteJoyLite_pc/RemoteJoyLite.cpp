/*------------------------------------------------------------------------------*/
/* RemoteJoyLite																*/
/*------------------------------------------------------------------------------*/
#include <cstdio>
#include <errno.h>
#include <ctime>
#include <cmath>
#include <cctype>
#include <memory>

#include <windows.h>
#include <atlbase.h>

#include <vfw.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <dinput.h>
#include <dxerr.h>

#include <lusb0_usb.h>

#include "Direct3D.h"
#include "DirectInput.h"
#include "DebugFont.h"
#include "RemoteJoyLite.h"
#include "Bitmap.h"
#include "Log.h"
#include "Movie.h"
#include "Wave.h"
#include "WinMain.h"
#include "Setting.h"
#include "../remotejoy.h"
#include "ImageFilter.h"
#include "ImageFilterNearest.h"
#include "ImageFilterBilinear.h"
#include "ImageFilterSpline36.h"
#include "ImageFilterLanczos4.h"

/*------------------------------------------------------------------------------*/
/* define																		*/
/*------------------------------------------------------------------------------*/
#define PSP_SCREEN_W		480
#define PSP_SCREEN_H		272
#define LEFT_POS(x)			(work.disp_rot&1)?(PSP_SCREEN_H-6*(x)-1):(PSP_SCREEN_W-6*(x)-1)
#define FRAME_COUNTER_SIZE	32

/*------------------------------------------------------------------------------*/
/* work																			*/
/*------------------------------------------------------------------------------*/
static struct {
	int			rect_x;
	int			rect_y;
	int			rect_w;
	int			rect_h;
	char		buff[PSP_SCREEN_W*PSP_SCREEN_H*4];
	HANDLE		buff_sema;
	int			buff_mode;
	int			gamma_flag;
	int			buff_vcount;
	int			prev_vcount;
	int			disp_flag;
	int			disp_rot;
	int			disp_debug;
	int			fps_idx;
	LONGLONG	fps_cnt[FRAME_COUNTER_SIZE];
	int			button;
	int			axis_x;
	int			axis_y;
	int			save_flag;
	int			save_bmp;
	int			save_avi;
	int			save_wav;
	PSP_BITMAP	psp_bmp;
} work;

static std::unique_ptr<ImageFilter> imageFilter;

static const char* trim(const char* s) {
	static char buffer[1024];
	strncpy(buffer, s, sizeof(buffer) - 1);
	for (int i = strlen(buffer) - 1; i >= 0 && isspace(buffer[i]); --i) {
		buffer[i] = '\0';
	}
	return buffer;
}

/*------------------------------------------------------------------------------*/
/* UpdateFps																	*/
/*------------------------------------------------------------------------------*/
static void UpdateFps( int diff_vcount )
{
	int now_index = work.fps_idx;
	int lst_index = (now_index + 1) % FRAME_COUNTER_SIZE;

	if ( diff_vcount != 0 ){
		now_index = lst_index;
		lst_index = (now_index + 1) % FRAME_COUNTER_SIZE;

		QueryPerformanceCounter( (PLARGE_INTEGER)&work.fps_cnt[now_index] );
		work.fps_idx = now_index;
	}

	double fps = 0.0f;
	if ( work.fps_cnt[lst_index] != 0 ){
		LONGLONG frequency;
		LONGLONG diff_count = work.fps_cnt[now_index] - work.fps_cnt[lst_index];

		QueryPerformanceFrequency( (PLARGE_INTEGER)&frequency );
		fps = ((double)(FRAME_COUNTER_SIZE-1))/((double)diff_count/(double)frequency);
	}
	if ( work.disp_flag == 0 ){
		dprintf( LEFT_POS(8), 0, "DISP OFF" );
	} else {
		if ( work.disp_debug != 0 ){
			dprintf( LEFT_POS(8), 0, "FPS %4.1f", fps );
		}
	}
}

/********************************************************************************/
/* Usb																			*/
/********************************************************************************/
/*------------------------------------------------------------------------------*/
/* usb device																	*/
/*------------------------------------------------------------------------------*/
static usb_dev_handle *UsbDev = NULL;
static volatile int UsbhostfsReady = 0;
static volatile bool UsbhostfsRunning = false;
static volatile bool UsbhostfsExited = false;
static volatile int UsbhostError = 0;
static volatile int PSPReady = 0;

/*------------------------------------------------------------------------------*/
/* async_remotejoy																*/
/*------------------------------------------------------------------------------*/
static void async_remotejoy( void *read, int read_len )
{
	if ( read_len < (int)sizeof(JoyScrHeader) ){ return; }

	JoyScrHeader *cmd = (JoyScrHeader *)read;

	if ( cmd->mode == ASYNC_CMD_DEBUG ){
		dprintf( 0, 0, "%s", (void *)(cmd+1) );
	}
}

/*------------------------------------------------------------------------------*/
/* bulk_remotejoy																*/
/*------------------------------------------------------------------------------*/
static void bulk_remotejoy( void *read, int read_len )
{
	JoyScrHeader *cmd = (JoyScrHeader *)read;

	work.buff_mode   = cmd->mode;
	work.buff_vcount = cmd->ref;
	WaitForSingleObject( work.buff_sema, INFINITE );
	memcpy( work.buff, (void *)(cmd+1), cmd->size );
	ReleaseSemaphore( work.buff_sema, 1, NULL );
}

/*------------------------------------------------------------------------------*/
/* send_event																	*/
/*------------------------------------------------------------------------------*/
static void send_event( int type, int value1, int value2 )
{
	int ret;
	struct {
		AsyncCommand	async;
		JoyEvent		event;
	} data;

	if ( PSPReady == 0 ){ return; }
	data.async.magic   = ASYNC_MAGIC;
	data.async.channel = ASYNC_USER;
	data.event.magic   = JOY_MAGIC;
	data.event.type    = type;
	data.event.value1  = value1;
	data.event.value2  = value2;
	ret = usb_bulk_write( UsbDev, 3, (char *)&data, sizeof(data), 10000 );
	if ( ret < 0 ){
		//LOG("send_event(): usb_bulk_write() failed: (%s)", trim(usb_strerror()));
		return;
	}
}

/*------------------------------------------------------------------------------*/
/* handle_hello																	*/
/*------------------------------------------------------------------------------*/
static void handle_hello( void )
{
	int ret;
	HostFsHelloResp Resp;

	memset( &Resp, 0, sizeof(Resp) );
	Resp.cmd.magic   = HOSTFS_MAGIC;
	Resp.cmd.command = HOSTFS_CMD_HELLO(RJL_VERSION);
	ret = usb_bulk_write( UsbDev, 2, (char *)&Resp, sizeof(Resp), 10000 );
	if ( ret < 0 ){
		LOG(LOG_LEVEL_WARN, "handle_hello(): usb_bulk_write(){reason=\"%s\"}", trim(usb_strerror()));
		return;
	}
	if ( SettingData.DispOff == 0 ){ work.disp_flag = 1; }
	PSPReady = 1;
	RemoteJoyLite_SendPSPCmd();
}

/*------------------------------------------------------------------------------*/
/* do_default																	*/
/*------------------------------------------------------------------------------*/
static bool do_default( void *read, int read_len ) {
	return true;
}

/*------------------------------------------------------------------------------*/
/* do_hostfs																	*/
/*------------------------------------------------------------------------------*/
static bool do_hostfs( void *read, int read_len )
{
	HostFsCmd *cmd = (HostFsCmd *)read;

	if ( read_len < (int)sizeof(HostFsCmd) ){
		return false;
	}

	if ( (int)cmd->command == HOSTFS_CMD_HELLO(RJL_VERSION) ){
		UsbhostError = 0;
		handle_hello();
	} else {
		UsbhostError = 1;
		LOG(LOG_LEVEL_WARN, "do_hostfs(): command != HOSTFS_CMD_HELLO");
	}

	return true;
}

/*------------------------------------------------------------------------------*/
/* do_async																		*/
/*------------------------------------------------------------------------------*/
static bool do_async( void *read, int read_len )
{
	AsyncCommand *cmd = (AsyncCommand *)read;

	if ( read_len < (int)sizeof(AsyncCommand) ){
		return false;
	}
	if ( read_len > (int)sizeof(AsyncCommand) ){
		char *data = (char *)cmd + sizeof(AsyncCommand);
		async_remotejoy( data, read_len - sizeof(AsyncCommand) );
	}

	return true;
}

/*------------------------------------------------------------------------------*/
/* do_bulk																		*/
/*------------------------------------------------------------------------------*/
static char BulkBlock[HOSTFS_BULK_MAXWRITE];

static bool do_bulk( void *read, int read_len )
{
	BulkCommand *cmd = (BulkCommand *)read;

	if ( read_len < (int)sizeof(BulkCommand) ){
		return false;
	}

	int read_size = 0;
	int data_size = cmd->size;

	while ( read_size < data_size ){
		int rest_size = data_size - read_size;
		if ( rest_size > HOSTFS_MAX_BLOCK ){ rest_size = HOSTFS_MAX_BLOCK; }
		int ret = usb_bulk_read( UsbDev, 0x81, &BulkBlock[read_size], rest_size, 3000 );
		if (ret < 0) {
			LOG(LOG_LEVEL_WARN, "do_bulk(): usb_bulk_read(){len=%d, reason=\"%s\"}", ret, trim(usb_strerror()));
			return false;
		}
		if ( ret != rest_size ) {
			break;
		}
		read_size += rest_size;
	}
	bulk_remotejoy( BulkBlock, data_size );
	return true;
}

/********************************************************************************/
static struct usb_device* UsbFindDevice() {
	usb_find_busses();
	usb_find_devices();

	for ( struct usb_bus* bus=usb_get_busses(); bus!=NULL; bus=bus->next ){
		for ( struct usb_device* dev=bus->devices; dev!=NULL; dev=dev->next ){
			if ( dev->descriptor.idVendor != SONY_VID ){ continue; }
			if ( dev->descriptor.idProduct != HOSTFSDRIVER_PID ){ continue; }
			return dev;
		}
	}

	return NULL;
}

/*------------------------------------------------------------------------------*/
/* UsbOpenDevice																*/
/*------------------------------------------------------------------------------*/
static void UsbOpenDevice( void )
{
	LOG(LOG_LEVEL_INFO, "UsbOpenDevice()");

	while (UsbhostfsRunning) {
		LOG(LOG_LEVEL_INFO, "UsbOpenDevice(): start enumerating");

		for (;;) {
			struct usb_device *dev = UsbFindDevice();

			if (dev == NULL) {
				break;
			}

			UsbDev = usb_open( dev );

			if ( UsbDev == NULL ){
				LOG(LOG_LEVEL_WARN, "UsbOpenDevice(): usb_open(){reason=\"%s\"}", trim(usb_strerror()));
				break;
			}

			if ( usb_set_configuration( UsbDev, 1 ) ){
				LOG(LOG_LEVEL_WARN, "UsbOpenDevice(): usb_set_configuration(){reason=\"%s\"}", trim(usb_strerror()));
				usb_close( UsbDev );
				break;
			}

			if ( usb_claim_interface( UsbDev, 0 ) ){
				LOG(LOG_LEVEL_WARN, "UsbOpenDevice(): usb_claim_interface(){reason=\"%s\"}", trim(usb_strerror()));
				usb_close( UsbDev );
				break;
			}

			return;
		}

		Sleep( 100 );
	}
}

/*------------------------------------------------------------------------------*/
/* UsbCloseDevice																*/
/*------------------------------------------------------------------------------*/
static void UsbCloseDevice( void )
{
	LOG(LOG_LEVEL_INFO, "UsbCloseDevice()");

	if ( UsbDev != NULL ){
		if (usb_release_interface( UsbDev, 0 )) {
			LOG(LOG_LEVEL_WARN, "UsbCloseDevice(): usb_release_interface(){reason=\"%s\"}", trim(usb_strerror()));
		}

		if (usb_close( UsbDev )) {
			LOG(LOG_LEVEL_WARN, "UsbCloseDevice(): usb_close(){reason=\"%s\"}", trim(usb_strerror()));
		}
	}
	UsbDev = NULL;
}

/*------------------------------------------------------------------------------*/
/* UsbCheckDevice																*/
/*------------------------------------------------------------------------------*/
static int UsbCheckDevice( void )
{
	LOG(LOG_LEVEL_INFO, "UsbCheckDevice()");

	if ( UsbDev == NULL ) {
		return -1;
	}
	int mag = HOSTFS_MAGIC;
	int len = usb_bulk_write( UsbDev, 2, (char *)&mag, 4, 1000 );
	if (len < 0) {
		LOG(LOG_LEVEL_WARN, "UsbCheckDevice(): usb_bulk_write(){len=%d, reason=\"%s\"}", len, trim(usb_strerror()));
		return -1;
	} else if (len != 4) {
		LOG(LOG_LEVEL_WARN, "UsbCheckDevice(): usb_bulk_write(){len=%d, reason=\"%s\"}", len, trim(usb_strerror()));
		return -1;
	}
	return 0;
}

/*------------------------------------------------------------------------------*/
/* UsbDeviceMain																*/
/*------------------------------------------------------------------------------*/
static DWORD WINAPI UsbDeviceMain( LPVOID lpv )
{
	LOG(LOG_LEVEL_INFO, "UsbDeviceMain()");

	SetUsbResetStatus(USB_RESET_STATUS_RESETTING);
	UsbResetDevice();

	usb_init();
	while( 1 ){
		WaitForUsbResetStatus(USB_RESET_STATUS_OPENING);

		UsbOpenDevice();

		if (UsbCheckDevice()) {
			UsbCloseDevice();
			if (!UsbhostfsRunning){
				break;
			}
			Sleep(10);
			continue;
		}

		UsbhostfsReady = 1;
		while (UsbhostfsRunning) {
			SetUsbResetStatus(USB_RESET_STATUS_OPENED);

			if (GetResetUsbAndReset()) {
				// デバイスのリセット
				SetUsbResetStatus(USB_RESET_STATUS_CLOSING);
				UsbResetDevice();
				break;
			}

			int data[512/sizeof(int)];
			int len = usb_bulk_read( UsbDev, 0x81, (char *)data, 512, 1000 );

			if ( len < 0 ){
				LOG(LOG_LEVEL_WARN, "UsbDeviceMain(): usb_bulk_read(){len=%d, reason=\"%s\"}", len, trim(usb_strerror()));

				if (len == -116) {
					// 復旧できる可能性が高いのでしばらく待ってやり直す
					Sleep(100);
					continue;
				} else if (len == -5) {
					// 接続が切断されたのでデバイスを閉じる
					SetUsbResetStatus(USB_RESET_STATUS_CLOSING);
					UsbResetDevice();
					break;
				}
			}

			if ( len < 4 ){
				LOG(LOG_LEVEL_WARN, "UsbDeviceMain(): usb_bulk_read(){len=%d, reason=\"%s\"}", len, trim(usb_strerror()));
				Sleep(100);
				continue;
			}

			if (data[0] == HOSTFS_MAGIC) {
				if (!do_hostfs(data, len)) {
					Sleep(100);
					continue;
				}
			} else if (data[0] == ASYNC_MAGIC) {
				if (!do_async(data, len)) {
					Sleep(100);
					continue;
				}
			} else if (data[0] == BULK_MAGIC) {
				// do_bulk()でエラーが起きた場合は待機しても復帰できない。
				// USBデバイスをリセットする
				if (!do_bulk(data, len)) {
					SetUsbResetStatus(USB_RESET_STATUS_CLOSING);
					UsbResetDevice();
					break;
				}
			} else {
				if (!do_default(data, len)) {
					Sleep(100);
					continue;
				}
			}
		}
		PSPReady = 0;
		UsbhostfsReady = 0;
		UsbCloseDevice();

		if (!UsbhostfsRunning) {
			break;
		}

		SetUsbResetStatus(USB_RESET_STATUS_RESETTING);
	}
	UsbhostfsExited = true;

	SetUsbResetStatus(USB_RESET_STATUS_RESETTING);
	UsbResetDevice();
	WaitForUsbResetStatus(USB_RESET_STATUS_OPENING);

	return( 0 );
}

/********************************************************************************/
/* RemoteJoy Main																*/
/********************************************************************************/
/*------------------------------------------------------------------------------*/
/* define																		*/
/*------------------------------------------------------------------------------*/
#define PRIM_FVF	(D3DFVF_XYZRHW|D3DFVF_TEX1)

struct PRIM {
	float x, y, z;
	float rhw;
	float u, v;
};

/*------------------------------------------------------------------------------*/
/* work																			*/
/*------------------------------------------------------------------------------*/
static CComPtr<IDirect3DTexture9> pD3DTex;

/*------------------------------------------------------------------------------*/
/* Gamma																		*/
/*------------------------------------------------------------------------------*/
static unsigned char GammaTable[3][256];

void RemoteJoyLite_CalcGammaTable( void )
{
	float gamma[3];

	for ( int rgb=0; rgb<3; rgb++ ){
		if ( SettingData.GammaRGB == 0 ){
			gamma[rgb] = 100.0f/(float)SettingData.Gamma[3];
		} else {
			gamma[rgb] = 100.0f/(float)SettingData.Gamma[rgb];
		}
	}

	for ( int rgb=0; rgb<3; rgb++ ){
		unsigned char *tbl = GammaTable[rgb];
		for ( int i=0; i<256; i++ ){
			float raw = (float)i/255.0f;
			float cor = powf( raw, gamma[rgb] );
			tbl[i] = (unsigned char)(0.5f + cor*255.0f);
		}
	}
	work.gamma_flag = 0;
	if ( SettingData.GammaRGB == 0 ){
		if ( SettingData.Gamma[3] != 100 ){ work.gamma_flag = 1; }
	} else {
		if ( SettingData.Gamma[0] != 100 ){ work.gamma_flag = 1; }
		if ( SettingData.Gamma[1] != 100 ){ work.gamma_flag = 1; }
		if ( SettingData.Gamma[2] != 100 ){ work.gamma_flag = 1; }
	}
}

/*------------------------------------------------------------------------------*/
/* TranceGamma																	*/
/*------------------------------------------------------------------------------*/
static DWORD TranceGamma( DWORD argb )
{
	DWORD r = (argb >> 16) & 0xFF;
	DWORD g = (argb >>  8) & 0xFF;
	DWORD b = (argb >>  0) & 0xFF;
	r = GammaTable[0][r];
	g = GammaTable[1][g];
	b = GammaTable[2][b];
	return( (r<<16)|(g<<8)|b|0xFF000000 );
}

/*------------------------------------------------------------------------------*/
/* Trancetexture_ARGB0565														*/
/*------------------------------------------------------------------------------*/
static void Trancetexture_ARGB0565( D3DLOCKED_RECT *plockRect )
{
	WORD  *src = (WORD *)work.buff;
	DWORD *dst = (DWORD *)plockRect->pBits;
	DWORD texw = plockRect->Pitch/sizeof(DWORD);
	DWORD addp = texw - work.rect_w;
	dst += texw*work.rect_y + work.rect_x;

	if ( work.buff_mode & 0x100 ){ dst += texw; }
	for ( int y=0; y<work.rect_h; y++ ){
		for ( int i=0; i<work.rect_w; i++ ){
			DWORD r = ((DWORD)*src & 0x001F) << 19;
			DWORD g = ((DWORD)*src & 0x07E0) <<  5;
			DWORD b = ((DWORD)*src & 0xF800) >>  8;
			DWORD c = r|g|b|0xFF000000;
			DWORD m = (c & 0x00E000E0) >> 5;
			DWORD n = (c & 0x0000C000) >> 6;
			DWORD o = c|m|n;
			if ( work.gamma_flag != 0 ){ o = TranceGamma( o ); }
			*dst++ = o;
			src++;
		}
		dst += addp;
		if ( work.buff_mode & 0x400 ){ if ( !(y&1) ){ src -= work.rect_w; } }
		if ( work.buff_mode & 0x200 ){ dst += texw; y++; }
	}
}

/*------------------------------------------------------------------------------*/
/* Trancetexture_ARGB1555														*/
/*------------------------------------------------------------------------------*/
static void Trancetexture_ARGB1555( D3DLOCKED_RECT *plockRect )
{
	WORD  *src = (WORD *)work.buff;
	DWORD *dst = (DWORD *)plockRect->pBits;
	DWORD texw = plockRect->Pitch/sizeof(DWORD);
	DWORD addp = texw - work.rect_w;
	dst += texw*work.rect_y + work.rect_x;

	if ( work.buff_mode & 0x100 ){ dst += texw; }
	for ( int y=0; y<work.rect_h; y++ ){
		for ( int i=0; i<work.rect_w; i++ ){
			DWORD r = ((DWORD)*src & 0x001F) << 19;
			DWORD g = ((DWORD)*src & 0x03E0) <<  6;
			DWORD b = ((DWORD)*src & 0x7C00) >>  7;
			DWORD c = r|g|b|0xFF000000;
			DWORD m = (c & 0x00E0E0E0) >> 5;
			DWORD o = c|m;
			if ( work.gamma_flag != 0 ){ o = TranceGamma( o ); }
			*dst++ = o;
			src++;
		}
		dst += addp;
		if ( work.buff_mode & 0x400 ){ if ( !(y&1) ){ src -= work.rect_w; } }
		if ( work.buff_mode & 0x200 ){ dst += texw; y++; }
	}
}

/*------------------------------------------------------------------------------*/
/* Trancetexture_ARGB4444														*/
/*------------------------------------------------------------------------------*/
static void Trancetexture_ARGB4444( D3DLOCKED_RECT *plockRect )
{
	WORD  *src = (WORD *)work.buff;
	DWORD *dst = (DWORD *)plockRect->pBits;
	DWORD texw = plockRect->Pitch/sizeof(DWORD);
	DWORD addp = texw - work.rect_w;
	dst += texw*work.rect_y + work.rect_x;

	if ( work.buff_mode & 0x100 ){ dst += texw; }
	for ( int y=0; y<work.rect_h; y++ ){
		for ( int i=0; i<work.rect_w; i++ ){
			DWORD r = ((DWORD)*src & 0x000F) << 20;
			DWORD g = ((DWORD)*src & 0x00F0) <<  8;
			DWORD b = ((DWORD)*src & 0x0F00) >>  4;
			DWORD c = r|g|b|0xFF000000;
			DWORD m = (c & 0x00F0F0F0) >> 4;
			DWORD o = c|m;
			if ( work.gamma_flag != 0 ){ o = TranceGamma( o ); }
			*dst++ = o;
			src++;
		}
		dst += addp;
		if ( work.buff_mode & 0x400 ){ if ( !(y&1) ){ src -= work.rect_w; } }
		if ( work.buff_mode & 0x200 ){ dst += texw; y++; }
	}
}

/*------------------------------------------------------------------------------*/
/* Trancetexture_ARGB8888														*/
/*------------------------------------------------------------------------------*/
static void Trancetexture_ARGB8888( D3DLOCKED_RECT *plockRect )
{
	DWORD *src = (DWORD *)work.buff;
	DWORD *dst = (DWORD *)plockRect->pBits;
	DWORD texw = plockRect->Pitch/sizeof(DWORD);
	DWORD addp = texw - work.rect_w;
	dst += texw*work.rect_y + work.rect_x;

	for ( int y=0; y<work.rect_h; y++ ){
		for ( int i=0; i<work.rect_w; i++ ){
			DWORD r = (*src & 0x0000FF) << 16;
			DWORD g = (*src & 0x00FF00) <<  0;
			DWORD b = (*src & 0xFF0000) >> 16;
			DWORD o = r|g|b|0xFF000000;
			if ( work.gamma_flag != 0 ){ o = TranceGamma( o ); }
			*dst++ = o;
			src++;
		}
		dst += addp;
	}
}

/*------------------------------------------------------------------------------*/
/* Trancetexture_UNKNOWN														*/
/*------------------------------------------------------------------------------*/
static void Trancetexture_UNKNOWN( D3DLOCKED_RECT *plockRect )
{
	DWORD *dst = (DWORD *)plockRect->pBits;
	DWORD texw = plockRect->Pitch/sizeof(DWORD);

	for ( int y=0; y<272; y++ ){
		memset(dst, 0, sizeof(DWORD) * 480);
		dst += texw;
	}
}

/*------------------------------------------------------------------------------*/
/* TranceSaveBmpBuff															*/
/*------------------------------------------------------------------------------*/
static void TranceSaveBmpBuff( D3DLOCKED_RECT *plockRect )
{
	DWORD *src = (DWORD *)plockRect->pBits;
	DWORD texw = plockRect->Pitch/sizeof(DWORD);
	DWORD addp = texw - 480;

	for ( int y=0; y<272; y++ ){
		char *dst = &work.psp_bmp.Buff[PSP_SCREEN_W*3*(271-y)];
		for ( int i=0; i<480; i++ ){
			*dst++ = *src >>  0;
			*dst++ = *src >>  8;
			*dst++ = *src >> 16;
			src++;
		}
		src += addp;
	}
}

/*------------------------------------------------------------------------------*/
/* Trancetexture																*/
/*------------------------------------------------------------------------------*/
static void Trancetexture( void )
{
	D3DLOCKED_RECT lockRect;

	work.save_flag = 0;
	if ( work.save_bmp != 0 ){ work.save_flag = 1; }
	if ( work.save_avi != 0 ){ work.save_flag = 1; }

	WaitForSingleObject( work.buff_sema, INFINITE );
	HRESULT result = NULL;
	if (FAILED(result = pD3DTex->LockRect(0, &lockRect, NULL, D3DLOCK_DISCARD))) {
		LOG(LOG_LEVEL_WARN, "Trancetexture(): Failed to lock the texture.");
	} else {
		switch ( (work.buff_mode >> 4) & 0x0F ){
		case 0x00 : Trancetexture_ARGB0565( &lockRect );	break;
		case 0x01 : Trancetexture_ARGB1555( &lockRect );	break;
		case 0x02 : Trancetexture_ARGB4444( &lockRect );	break;
		case 0x03 : Trancetexture_ARGB8888( &lockRect );	break;
		default   : Trancetexture_UNKNOWN(  &lockRect );	break;
		}

		if ( work.save_flag != 0 ){ TranceSaveBmpBuff( &lockRect ); }
	}

	if (FAILED(result = pD3DTex->UnlockRect(0))) {
		LOG(LOG_LEVEL_WARN, "Trancetexture(): Failed to unlock the texture.");
	}

	ReleaseSemaphore( work.buff_sema, 1, NULL );
	if ( work.save_bmp != 0 ){
		Bitmap_Save( &work.psp_bmp );
		work.save_bmp = 0;
	}
	if ( work.save_avi != 0 ){
		u_int vcount = (u_int)work.buff_vcount;
		Movie_Save( &work.psp_bmp, vcount );
	}
}

/*------------------------------------------------------------------------------*/
/* Error																		*/
/*------------------------------------------------------------------------------*/
static void Error( const TCHAR* message, HRESULT result )
{
	WCHAR buffer[1024];

	wsprintf(buffer, L"%s (%s)", message, DXGetErrorString(result));
	MessageBox(NULL, buffer, L"RemoteJoyLite", MB_OK);
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLiteInit															*/
/*------------------------------------------------------------------------------*/
BOOL RemoteJoyLiteInit( AkindD3D *pAkindD3D )
{
	DWORD   thid;
	HRESULT hRes;

	ZeroMemory( &work, sizeof(work) );
	work.buff_sema = CreateSemaphore( NULL, 1, 1, NULL );
	work.buff_mode = -1;
	work.disp_rot = SettingData.DispRot;
	work.axis_x = 32768/256;
	work.axis_y = 32768/256;
	Bitmap_Init( &work.psp_bmp );
	RemoteJoyLite_CalcGammaTable();
	RemoteJoyLite_SetImageFilter();

	IDirect3DDevice9 *pD3DDev = pAkindD3D->getDevice();
	if (FAILED(hRes = D3DXCreateTexture(pD3DDev, PSP_SCREEN_W, PSP_SCREEN_H, 1, 0, D3DFMT_A8B8G8R8, D3DPOOL_MANAGED, &pD3DTex))) {
		Error( 0, hRes );
		return FALSE;
	}

	UsbhostfsRunning = true;
	CreateThread( NULL, 0, UsbDeviceMain, 0, 0, &thid );
	return( TRUE );
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLiteExit															*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLiteExit( void )
{
	if ( work.save_avi == 1 ){ Movie_End(); }
	if (!UsbhostfsRunning) {
		return;
	}
	UsbhostfsRunning = false;
	printf( "Waiting for usbhostfs exit...\n" );
	while (!UsbhostfsExited) {
		Sleep( 10 );
	}
	pD3DTex = NULL;
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLiteDraw															*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLiteDraw( AkindD3D *pAkindD3D )
{
	int diff_vcount;

	diff_vcount = work.buff_vcount - work.prev_vcount;
	if ( diff_vcount == 0 ){ Sleep( 1 ); }
	diff_vcount = work.buff_vcount - work.prev_vcount;
	work.prev_vcount = work.buff_vcount;

	if ( UsbhostfsReady != 0 ){
		UpdateFps( diff_vcount );
	} else {
		work.buff_mode = -1;
		dprintf( LEFT_POS(7), 0, "WAITING" );
	}
	if ( UsbhostError != 0 ){
		dprintf( 240-6*9, 136-4, "PRX Version Error!" );
	}

	if ( work.disp_flag != 0 ){ Trancetexture(); }
	IDirect3DDevice9 *pD3DDev = pAkindD3D->getDevice();
	pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	pD3DDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	pD3DDev->SetTexture( 0, pD3DTex );
	pD3DDev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
	pD3DDev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	pD3DDev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	pD3DDev->SetFVF( PRIM_FVF );

	imageFilter->set(pD3DDev);

	float z = 0.0f;
	SIZE canvasSize = pAkindD3D->getCanvasSize();
	float w = canvasSize.cx;
	float h = canvasSize.cy;

	float x0 = 0.0f + 0.5f / PSP_SCREEN_W;
	float x1 = 1.0f + 0.5f / PSP_SCREEN_W;
	float y0 = 0.0f + 0.5f / PSP_SCREEN_H;
	float y1 = 1.0f + 0.5f / PSP_SCREEN_H;
	WORD DispIdx[6] = { 0, 1, 2, 3, 2, 1 };
	PRIM DispBuf[4][4] = {
		{
			{ z, z, 0.0f, 1.0f, x0, y0 },
			{ w, z, 0.0f, 1.0f, x1, y0 },
			{ z, h, 0.0f, 1.0f, x0, y1 },
			{ w, h, 0.0f, 1.0f, x1, y1 },
		}, {
			{ h, z, 0.0f, 1.0f, x0, y0 },
			{ h, w, 0.0f, 1.0f, x1, y0 },
			{ z, z, 0.0f, 1.0f, x0, y1 },
			{ z, w, 0.0f, 1.0f, x1, y1 },
		}, {
			{ w, h, 0.0f, 1.0f, x0, y0 },
			{ z, h, 0.0f, 1.0f, x1, y0 },
			{ w, z, 0.0f, 1.0f, x0, y1 },
			{ z, z, 0.0f, 1.0f, x1, y1 },
		}, {
			{ z, w, 0.0f, 1.0f, x0, y0 },
			{ z, z, 0.0f, 1.0f, x1, y0 },
			{ h, w, 0.0f, 1.0f, x0, y1 },
			{ h, z, 0.0f, 1.0f, x1, y1 },
		},
	};

	pD3DDev->DrawIndexedPrimitiveUP( D3DPT_TRIANGLELIST, 0, 4, 2, DispIdx,
		D3DFMT_INDEX16, DispBuf[work.disp_rot], sizeof(PRIM) );
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLiteSync															*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLiteSync( void )
{
	int	AxisData = work.axis_x | (work.axis_y << 16);
	send_event( TYPE_JOY_DAT, work.button, AxisData );

	if ( work.disp_debug != 0 ){
		switch ( work.buff_mode & 0x0F ){
		case 0x00 : dprintf( LEFT_POS(8), 7, "ARGB0565" );	break;
		case 0x01 : dprintf( LEFT_POS(8), 7, "ARGB1555" );	break;
		case 0x02 : dprintf( LEFT_POS(8), 7, "ARGB4444" );	break;
		case 0x03 : dprintf( LEFT_POS(8), 7, "ARGB8888" );	break;
		default   : dprintf( LEFT_POS(7), 7, "UNKNOWN"  );	break;
		}
	}
	if ( (work.save_avi != 0) || (work.save_wav != 0) ){
		dprintf( 0, 0, "REC" );
	}
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_ToggleDebug													*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_ToggleDebug( void )
{
	work.disp_debug ^= 1;
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_ToggleDisp														*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_ToggleDisp( void )
{
	work.disp_flag ^= 1;
	RemoteJoyLite_SendPSPCmd();
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_SendPSPCmd														*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_SendPSPCmd( void )
{
	int	arg1 = 0;
	if ( work.disp_flag       != 0 ){ arg1 |= SCREEN_CMD_ACTIVE; }
	if ( SettingData.PSPDisp  != 0 ){ arg1 |= SCREEN_CMD_SCROFF; }
	if ( SettingData.PSPDbg   != 0 ){ arg1 |= SCREEN_CMD_DEBUG;  }
	if ( SettingData.PSPAsync != 0 ){ arg1 |= SCREEN_CMD_ASYNC;  }
	arg1 |= SCREEN_CMD_SET_TRNSFPS(SettingData.PSPFPS);
	arg1 |= SCREEN_CMD_SET_TRNSMODE(SettingData.PSPMode);
	arg1 |= SCREEN_CMD_SET_PRIORITY(SettingData.PSPPri);
	arg1 |= SCREEN_CMD_SET_ADRESS1(SettingData.PSPAdr1);
	arg1 |= SCREEN_CMD_SET_ADRESS2(SettingData.PSPAdr2 - 1);

	work.rect_x = SettingData.PSPRectX & 0xFFF8;
	work.rect_y = SettingData.PSPRectY;
	work.rect_w = ((SettingData.PSPRectW+(SettingData.PSPRectX&7))+31) & 0xFFE0;
	work.rect_h = (SettingData.PSPRectH+1) & 0xFFFE;

	int	arg2 = 0;
	arg2 |= SCREEN_CMD_SET_TRNSX(work.rect_x/8);
	arg2 |= SCREEN_CMD_SET_TRNSY(work.rect_y);
	arg2 |= SCREEN_CMD_SET_TRNSW(work.rect_w/32);
	arg2 |= SCREEN_CMD_SET_TRNSH(work.rect_h/2);

	send_event( TYPE_JOY_CMD, arg1, arg2 );

	D3DLOCKED_RECT lockRect = {0};
	HRESULT result = 0;
	CComPtr<IDirect3DTexture9> texture = pD3DTex;
	if (!texture) {
		return;
	}

	if (FAILED(result = texture->LockRect(0, &lockRect, NULL, D3DLOCK_DISCARD))) {
		LOG(LOG_LEVEL_WARN, "RemoteJoyLite_SendPSPCmd(): Failed to lock the texture.");
	} else {
		Trancetexture_UNKNOWN( &lockRect );
	}

	if (FAILED(result = texture->UnlockRect(0))) {
		LOG(LOG_LEVEL_WARN, "RemoteJoyLite_SendPSPCmd(): Failed to unlock the texture.");
		return;
	}
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_SetButton														*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_SetButton( int button )
{
	work.button = button & 0x00FFFFFF;
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_SetAxis														*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_SetAxis( int x, int y )
{
	work.axis_x = x & 0xFF;
	work.axis_y = y & 0xFF;
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_GetButton														*/
/*------------------------------------------------------------------------------*/
int RemoteJoyLite_GetButton( void )
{
	return( work.button );
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_GetAxisXY														*/
/*------------------------------------------------------------------------------*/
int RemoteJoyLite_GetAxisXY( void )
{
	return( work.axis_x | (work.axis_y << 16) );
}

/*------------------------------------------------------------------------------*/
/* SetSaveName																	*/
/*------------------------------------------------------------------------------*/
static void SetSaveName( char *file_name )
{
	time_t		now;
	struct tm	*date;

	time( &now );
	date = localtime( &now );
	int year   = date->tm_year + 1900;
	int month  = date->tm_mon + 1;
	int day    = date->tm_mday;
	int hour   = date->tm_hour;
	int minute = date->tm_min;
	int second = date->tm_sec;

	sprintf( file_name, ".\\Capture\\%04d%02d%02d%02d%02d%02d",
		year, month, day, hour, minute, second );
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_SaveBitmap														*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_SaveBitmap( void )
{
	char	file_name[256];

	if ( work.save_bmp == 0 ){
		SetSaveName( file_name );
		Bitmap_Name( file_name );
		work.save_bmp = 1;
	}
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_SaveMovie														*/
/*------------------------------------------------------------------------------*/
void RemoteJoyLite_SaveMovie( void )
{
	char	file_name[256];

	SetSaveName( file_name );

	if ( SettingData.AVIOut != 0 ){
		work.save_avi ^= 1;

		if ( work.save_avi == 1 ){
			u_int vcount = (u_int)work.buff_vcount;
			if ( Movie_Init( file_name, &work.psp_bmp, vcount ) != 0 ){ work.save_avi = 0; }
		} else {
			Movie_End();
		}
	}
	if ( SettingData.WAVOut != 0 ){
		work.save_wav ^= 1;

		if ( work.save_wav == 1 ){
			WaveOpen( file_name );
		} else {
			WaveClose();
		}
	}
}

/*------------------------------------------------------------------------------*/
/* RemoteJoyLite_CheckMovie														*/
/*------------------------------------------------------------------------------*/
BOOL RemoteJoyLite_CheckMovie( void )
{
	if ( work.save_avi == 1 ){ return( TRUE ); }
	if ( work.save_wav == 1 ){ return( TRUE ); }
	return( FALSE );
}

void RemoteJoyLite_SetImageFilter( void )
{
	switch ( SettingData.ImageFilter ) {
	case IMAGE_FILTER_TYPE_NEAREST:
		imageFilter = std::move(std::unique_ptr<ImageFilter>(new ImageFilterNearest()));
		break;
	case IMAGE_FILTER_TYPE_BILINEAR:
		imageFilter = std::move(std::unique_ptr<ImageFilter>(new ImageFilterBilinear()));
		break;
	case IMAGE_FILTER_TYPE_SPLINE36:
		imageFilter = std::move(std::unique_ptr<ImageFilter>(new ImageFilterSpline36()));
		break;
	case IMAGE_FILTER_TYPE_LANCZOS4:
		imageFilter = std::move(std::unique_ptr<ImageFilter>(new ImageFilterLanczos4()));
	}
}
