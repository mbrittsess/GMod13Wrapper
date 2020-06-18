@echo off
setlocal
set gmod_include="C:\devtools\gmod13headers"

set SDKInc="C:\devtools\WindowskSDK\Include"
set SDKLib="C:\devtools\WindowskSDK\Lib"

set DDKInc="C:\devtools\WindowsDDK\inc\ddk"

set APIInc="C:\devtools\WindowsDDK\inc\api"
set APILib="C:\devtools\WindowsDDK\lib\win7\i386"

set CRTInc="C:\devtools\WindowsDDK\inc\crt"
set CRTLib=C:\devtools\WindowsDDK\lib\Crt\i386

set LuaPInc="C:\Program Files (x86)\Lua\5.1\include"
set LuaPLib="C:\Projects\GModWrapper"

set CL86="C:\devtools\WindowsDDK\bin\x86\x86\cl.exe"
set LINK86="C:\devtools\WindowsDDK\bin\x86\x86\link.exe"
set LIB86=%LINK86% /LIB

if errorlevel 1 goto :eof
%CL86% /c /MD /I%CRTInc% /I%APIInc% /I%LuaPInc% /I%gmod_include% /Tplapi.c /Folua51.obj
if errorlevel 1 goto :eof
%LINK86% /DLL /EXPORT:gmod13_open /EXPORT:gmod13_close /LIBPATH:%APILib% /LIBPATH:%SDKLib% /LIBPATH:%CRTLib% /DEFAULTLIB:user32.lib lua51.obj /OUT:gmsv_hello.dll
del lua51.obj
del gmsv_hello.lib
del gmsv_hello.exp
copy gmsv_hello.dll gmcl_hello.dll
call copy_basic
endlocal