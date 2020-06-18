@echo off
setlocal
set gmod_include="C:\devtools\gmod13headers"
set lua_include="C:\Program Files (x86)\Lua\5.1\include"
set DDK="C:\devtools\WindowsDDK"

set CL86="C:\devtools\WindowsDDK\bin\x86\x86\cl.exe"
set LINK86="C:\devtools\WindowsDDK\bin\x86\x86\link.exe"
set LIB86=%LINK86% /LIB

del lua51.lib
call format_scripts

if errorlevel 1 goto :eof
%CL86% /c /MD /I%DDK%\inc\crt /I%DDK%\inc\api /I%lua_include% /I%gmod_include% /Tplapi.c /Folua51.obj
%CL86% /c /MD /I%DDK%\inc\crt /I%DDK%\inc\api /I%lua_include% /I%gmod_include% /Tclauxlib.c /Folauxlib.obj
if errorlevel 1 goto :eof
%LIB86% /EXPORT:gmod_open /EXPORT:gmod_close lua51.obj lauxlib.obj
::%LIB86% /EXPORT:gmod_open /EXPORT:gmod_close lua51.obj
del lua51.obj
del lauxlib.obj
endlocal