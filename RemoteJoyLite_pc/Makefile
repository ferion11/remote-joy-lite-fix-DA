TARGET   = RemoteJoyLite.exe
MAIN_OBJ = WinMain.obj RemoteJoyLite.res RemoteJoyLite.obj Direct3D.obj DirectInput.obj DebugFont.obj FontData_5x5.obj Log.obj
CAPT_OBJ = Bitmap.obj Movie.obj Wave.obj Macro.obj
CONF_OBJ = Setting.obj Setting_Joy.obj Setting_Key.obj Setting_Etc.obj Setting_Filt.obj Setting_Capt.obj Setting_PSP.obj Setting_Macro.obj
OBJS     = $(MAIN_OBJ) $(CAPT_OBJ) $(CONF_OBJ)
LIBS     = d3d9.lib dinput8.lib dxguid.lib libusb.lib vfw32.lib winmm.lib User32.lib Gdi32.lib
LANGUAGE =  LANG_JA
#LANGUAGE =  LANG_EN
CFLAGS   = /O2 /GL /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /I "C:\WinDDK\7600.16385.1\inc" /I "..\..\\libusb-win32-bin-1.2.6.0\include"
LDFLAGS  = /LTCG /LIBPATH:"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x86" /LIBPATH:"..\..\libusb-win32-bin-1.2.6.0\lib\msvc"
RCFLAGS  = -O coff

.SUFFIXES: .cpp .c .rc .res .obj

$(TARGET) : $(OBJS)
	link $(LDFLAGS) $(OBJS) $(LIBS) /OUT:$(TARGET)

.c.obj:
	cl $(CFLAGS) -c $*.c

.cpp.obj:
	cl $(CFLAGS) -c $*.cpp

.rc.res:
	windres $(RCFLAGS) $*.rc -o $*.res

clean: 
	@rm -rf $(TARGET) *.obj *.res *~
