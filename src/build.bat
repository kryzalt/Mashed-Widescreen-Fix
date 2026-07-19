@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"

cl.exe /LD /Fe:dinput8.dll /O2 main.cpp user32.lib

if %ERRORLEVEL% EQU 0 (
    echo Success!
) else (
    echo Error
)
pause
