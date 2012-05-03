@echo off
set DESTNAME=RemoteJoyLite_019_20120502

:: Get Path for SetEvn.Cmd, DirectX, WINDDK
FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Installer\UserData\S-1-5-18\Components\622F95D9B7214F04A9EC4512FB0B0A56" /v D633651344E817636AEFEA153D6D65E4') DO SET SETENVPATH=%%B
FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SYSTEM\ControlSet001\Control\Session Manager\Environment" /v DXSDK_DIR') DO SET DXSDKPATH=%%B
FOR /F "tokens=2* delims=	 " %%A IN ('REG QUERY "HKLM\SOFTWARE\Wow6432Node\Microsoft\WDKDocumentation\7600.091201\Setup" /v BUILD') DO SET WINDDKPATH=%%B

:: If Paths do not exist, fall back on default location
IF NOT EXIST "%SETENVPATH%" SET SETENVPATH=C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd
IF NOT EXIST "%DXSDKPATH%" SET DXSDKPATH=C:\Program Files (x86)\Microsoft Direct X SDK (June 2010)\
IF NOT EXIST "%WINDDKPATH%" SET WINDDKPATH=C:\WinDDK\7600.16385.1\

mkdir %DESTNAME%

call "%SETENVPATH%" /x86 /Release
pushd RemoteJoyLite_pc
copy libusb0.dll ..\%DESTNAME%\libusb0.dll
nmake clean
nmake LANGUAGE=LANG_EN
copy RemoteJoyLite.exe ..\%DESTNAME%\RemoteJoyLite_en.exe
nmake clean
nmake LANGUAGE=LANG_JP
copy RemoteJoyLite.exe ..\%DESTNAME%\RemoteJoyLite.exe
popd

pushd RemoteJoyLite_psp
make clean
make
copy RemoteJoyLite.prx ..\%DESTNAME%\RemoteJoyLite.prx
popd

call "%SETENVPATH%" /x86 /Release
cl /O2 /DUNICODE Setupapi.lib RemoteJoyLite_pc\ResetUsb.cpp
mkdir %DESTNAME%\x86
move ResetUsb.exe %DESTNAME%\x86\ResetUsb.exe

call "%SETENVPATH%" /x64 /Release
cl /O2 /DUNICODE Setupapi.lib RemoteJoyLite_pc\ResetUsb.cpp
mkdir %DESTNAME%\x64
move ResetUsb.exe %DESTNAME%\x64\ResetUsb.exe

pushd %DESTNAME%
zip -9 -r ../%DESTNAME%.zip *
popd

call "%SETENVPATH%" /x86 /Release
