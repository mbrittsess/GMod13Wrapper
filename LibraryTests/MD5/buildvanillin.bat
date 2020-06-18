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

set CL86="C:\devtools\WindowsDDK\bin\x86\x86\cl.exe"
set LINK86="C:\devtools\WindowsDDK\bin\x86\x86\link.exe"

::Compiler basic setup
set DefClOpts=/c /MD /I"C:\devtools\WindowsDDK\inc\api" /I"C:\devtools\WindowsDDK\inc\ddk" /I"C:\devtools\WindowskSDK\Include" /I"C:\devtools\WindowsDDK\inc\crt" /I"C:\Program Files (x86)\Lua\5.1\include"
set CLDefault=%CL86% %DefClOpts%

set md5_srcfiles="md5.c md5lib.c"
set des56_srcfiles="des56.c ldes56.c"

cd src

call :format_files %md5_srcfiles%
mkdir gmsv_md5
mkdir gmcl_md5
%LINK86% /DLL /DEF:md5.def /EXPORT:luaopen_md5_core /LIBPATH:%APILib% /LIBPATH:%SDKLib% /LIBPATH:%CRTLib% /LIBPATH:%LuaPLib% /DEFAULTLIB:lua51.lib *.obj /OUT:gmsv_md5\core.dll
del *.obj
del gmsv_md5\core.lib
del gmsv_md5\core.exp
copy gmsv_md5\core.dll gmcl_md5\core.dll

call :format_files %des56_srcfiles%
%LINK86% /DLL /DEF:des56.def /EXPORT:luaopen_des56 /LIBPATH:%APILib% /LIBPATH:%SDKLib% /LIBPATH:%CRTLib% /LIBPATH:%LuaPLib% /DEFAULTLIB:lua51.lib *.obj /OUT:gmsv_des56.dll
del *.obj
del gmsv_des56.lib
del gmsv_des56.exp
copy gmsv_des56.dll gmcl_des56.dll

rename gmsv_md5\core.dll core_win32.dll
rename gmcl_md5\core.dll core_win32.dll
rename gmsv_des56.dll gmsv_des56_win32.dll
rename gmcl_des56.dll gmcl_des56_win32.dll

cd ..

endlocal
goto :EOF

:format_files
for /F "tokens=1,*" %%F in ("%~1") do call :compile_files "%%F" "%%G"
goto :EOF

:compile_files
%CLDefault% %~1
call :format_files %2
goto :EOF