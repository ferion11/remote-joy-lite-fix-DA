@echo off
set DESTNAME=RemoteJoyLite_019_20120429

mkdir %DESTNAME%

call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd" /x86 /Release
pushd RemoteJoyLite_pc
copy libusb0.dll ..\%DESTNAME%\libusb0.dll
nmake clean
nmake LANGUAGE=LANG_EN
copy RemoteJoyLite.exe ..\%DESTNAME%\RemoteJoyLite_en.exe
nmake clean
nmake LANGUAGE=LANG_JP
copy RemoteJoyLite.exe ..\%DESTNAME%\RemoteJoyLite.exe
popd

:: pushd RemoteJoyLite_psp
:: make clean
:: make
:: copy RemoteJoyLite.prx ../%DESTNAME%/RemoteJoyLite.prx
:: popd

call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd" /x86 /Release
cl /O2 /DUNICODE Setupapi.lib RemoteJoyLite_pc\ResetUsb.cpp
mkdir %DESTNAME%\x86
move ResetUsb.exe %DESTNAME%\x86\ResetUsb.exe

call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd" /x64 /Release
cl /O2 /DUNICODE Setupapi.lib RemoteJoyLite_pc\ResetUsb.cpp
mkdir %DESTNAME%\x64
move ResetUsb.exe %DESTNAME%\x64\ResetUsb.exe

pushd %DESTNAME%
zip -9 -r ../%DESTNAME%.zip *
popd

call "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.Cmd" /x86 /Release
