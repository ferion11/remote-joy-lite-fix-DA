TARGET   = RemoteJoyLite.exe
MAIN_OBJ = WinMain.obj RemoteJoyLite.res RemoteJoyLite.obj Direct3D.obj DirectInput.obj DebugFont.obj FontData_5x5.obj Log.obj
CAPT_OBJ = Bitmap.obj Movie.obj Wave.obj Macro.obj
CONF_OBJ = Setting.obj Setting_Joy.obj Setting_Key.obj Setting_Etc.obj Setting_Filt.obj Setting_Capt.obj Setting_PSP.obj Setting_Macro.obj
OBJS     = $(MAIN_OBJ) $(CAPT_OBJ) $(CONF_OBJ)
LIBS     = -ld3d9 -ldinput -ldxguid -lwinmm -lusb -lvfw32 -lsetupapi
LANGUAGE =  LANG_JA
#LANGUAGE =  LANG_EN
CFLAGS   = /Zi /nologo /W3 /WX- /MP /O2 /Oi /Oy- /GL /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_UNICODE" /D "UNICODE" /Gm- /EHsc /MD /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Fa"Release\" /Fo"Release\" /Gd /analyze- /errorReport:queue /I " C:\WinDDK\7600.16385.1\inc" /I "C:\develop\libusb-win32-bin-1.2.6.0\include"
LDFLAGS  = -L/usr/local/lib -mwindows -mno-cygwin
RCFLAGS  = -O coff

.SUFFIXES: .cpp .c .rc .res .obj

$(TARGET) : $(OBJS)
	link $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)

.c.obj:
	cl $(CFLAGS) -c $*.c

.cpp.obj:
	cl $(CFLAGS) -c $*.cpp

.rc.res:
	windres $(RCFLAGS) $*.rc -o $*.res

clean: 
	@rm -rf $(TARGET) *.obj *.res *~
