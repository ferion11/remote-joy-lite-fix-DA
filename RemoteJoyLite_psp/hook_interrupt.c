/*------------------------------------------------------------------------------*/
/* hook_interrupt																*/
/*------------------------------------------------------------------------------*/
#include <pspkernel.h>
#include "debug.h"
#include "hook.h"

/*------------------------------------------------------------------------------*/
/* prototype																	*/
/*------------------------------------------------------------------------------*/
extern void SendDispEvent( void );

/*------------------------------------------------------------------------------*/
/* work																			*/
/*------------------------------------------------------------------------------*/
static int (*sceKernelRegisterSubIntrHandler_Func)( int, int, void *, void * ) = NULL;
static int IndexUseNow = -1;
static int IndexUseFlag[16] = { 0 };

/*------------------------------------------------------------------------------*/
/* FindUnuseIndex																*/
/*------------------------------------------------------------------------------*/
static int FindUnuseIndex( void )
{
	int	idx;

	for ( idx=1; idx<16; idx++ ){
		if ( IndexUseFlag[idx] == 0 ){ return( idx ); }
	}
	return( -1 );
}

/*------------------------------------------------------------------------------*/
/* TranceAsyncOn																*/
/*------------------------------------------------------------------------------*/
void TranceAsyncOn( void )
{
	if ( IndexUseNow != -1 ){ return; }
	if ( sceKernelRegisterSubIntrHandler_Func == NULL ){ return; }

	IndexUseNow = FindUnuseIndex();
	if ( IndexUseNow == -1 ){ return; }
	sceKernelRegisterSubIntrHandler_Func( 30, IndexUseNow, SendDispEvent, 0 );
	sceKernelEnableSubIntr( 30, IndexUseNow );
}

/*------------------------------------------------------------------------------*/
/* TranceAsyncOff																*/
/*------------------------------------------------------------------------------*/
void TranceAsyncOff( void )
{
	if ( IndexUseNow == -1 ){ return; }
	sceKernelDisableSubIntr( 30, IndexUseNow );
	sceKernelReleaseSubIntrHandler( 30, IndexUseNow );
	IndexUseNow = -1;
}

/*------------------------------------------------------------------------------*/
/* MyKernelRegisterSubIntrHandler												*/
/*------------------------------------------------------------------------------*/
static int MyKernelRegisterSubIntrHandler( int intno, int no, void *handler, void *arg )
{
	IndexUseFlag[no] = 1;
	if ( IndexUseNow == no ){
		TranceAsyncOff();
		TranceAsyncOn();
	}
	int ret = sceKernelRegisterSubIntrHandler_Func( intno, no, handler, arg );
	return( ret );
}

/*------------------------------------------------------------------------------*/
/* hookInterrupt																*/
/*------------------------------------------------------------------------------*/
void hookInterrupt( void )
{
	SceModule *module = sceKernelFindModuleByName( "sceInterruptManager" );
	if ( module == NULL ){ return; }

	if ( sceKernelRegisterSubIntrHandler_Func == NULL ){
		sceKernelRegisterSubIntrHandler_Func = HookNidAddress( module, "InterruptManager", 0xCA04A2B9 );
		void *hook_addr = HookSyscallAddress( sceKernelRegisterSubIntrHandler_Func );
		HookFuncSetting( hook_addr, MyKernelRegisterSubIntrHandler );
	}
}
