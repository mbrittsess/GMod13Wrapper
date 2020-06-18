@echo off
setlocal
set SDKInc="C:\devtools\WindowskSDK\Include"
set SDKLib="C:\devtools\WindowskSDK\Lib"

set DDKInc="C:\devtools\WindowsDDK\inc\ddk"

set APIInc="C:\devtools\WindowsDDK\inc\api"
set APILib="C:\devtools\WindowsDDK\lib\win7\i386"

set CRTInc="C:\devtools\WindowsDDK\inc\crt"
set CRTLib=C:\devtools\WindowsDDK\lib\Crt\i386

set LuaPInc="C:\Program Files (x86)\Lua\5.1\include"
set LuaPLib="C:\Projects\GMod13Wrapper"
::set LuaPLib="C:\Program Files (x86)\Lua\5.1\lib"

set CL86="C:\devtools\WindowsDDK\bin\x86\x86\cl.exe"
set LINK86="C:\devtools\WindowsDDK\bin\x86\x86\link.exe"

%CL86% /c /MD /I%APIInc% /I%DDKInc% /I%SDKInc% /I%CRTInc% /I%LuaPInc% /Tchello.c
if not errorlevel 1 (
%LINK86% /DLL /EXPORT:luaopen_hello /LIBPATH:%APILib% /LIBPATH:%SDKLib% /LIBPATH:%CRTLib% /LIBPATH:%LuaPLib% /DEFAULTLIB:lua51.lib hello.obj /OUT:gmsv_hello.dll
del hello.obj
del gmsv_hello.exp
del gmsv_hello.lib

rename gmsv_hello.dll gmsv_hello_win32.dll
)

endlocal