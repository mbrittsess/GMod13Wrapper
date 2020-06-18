@echo off
for %%f in (LuaScripts\*.lua) do (
    luac -p %%f
    if errorlevel 0 (
        to_c < %%f > LuaIncs\%%~nf.txt
    ) else (
        echo Lua script "%%~nf" failed syntax check!
        set errorlevel=1
        goto :eof
    )
)