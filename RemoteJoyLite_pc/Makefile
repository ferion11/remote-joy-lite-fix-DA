TARGET   = RemoteJoyLite.exe
MAIN_OBJ = WinMain.obj RemoteJoyLite.res RemoteJoyLite.obj Direct3D.obj DirectInput.obj DebugFont.obj FontData_5x5.obj Log.obj Deleter.obj
CAPT_OBJ = Bitmap.obj Movie.obj Wave.obj Macro.obj
CONF_OBJ = Setting.obj Setting_Joy.obj Setting_Key.obj Setting_Etc.obj Setting_Filt.obj Setting_Capt.obj Setting_PSP.obj Setting_Macro.obj
FILTER_OBJ = ImageFilterNearest.obj ImageFilterBilinear.obj ImageFilterSpline36.obj Spline36.obj ImageFilterLanczos4.obj lanczos4.obj
OBJS     = $(MAIN_OBJ) $(CAPT_OBJ) $(CONF_OBJ) $(FILTER_OBJ)
LIBS     = d3d9.lib d3dx9.lib dinput8.lib dxguid.lib dxerr.lib libusb.lib vfw32.lib winmm.lib user32.lib gdi32.lib advapi32.lib
LANGUAGE =  LANG_JA
#LANGUAGE =  LANG_EN
CFLAGS   = /O2 /GL /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /DLANGUAGE=$(LANGUAGE) /DDIRECTINPUT_VERSION=0x0800 /I "$(DXSDKPATH)Include" /I "$(WINDDKPATH)inc" /I "$(WINDDKPATH)inc\atl71" /I "..\..\libusb-win32-bin-1.2.6.0\include"
LDFLAGS  = /LTCG /LIBPATH:"$(DXSDKPATH)Lib\x86" /LIBPATH:"$(WINDDKPATH)lib\ATL\i386" /LIBPATH:"..\..\libusb-win32-bin-1.2.6.0\lib\msvc"
RCFLAGS  =

.SUFFIXES: .cpp .c .rc .res .obj

$(TARGET) : $(OBJS)
	link $(LDFLAGS) $(OBJS) $(LIBS) /OUT:$(TARGET)

.c.obj:
	cl $(CFLAGS) -c $*.c

.cpp.obj:
	cl $(CFLAGS) -c $*.cpp

.rc.res:
	rc $(RCFLAGS) $*.rc

clean: 
	del /F /S $(TARGET) *.obj *.res *~
