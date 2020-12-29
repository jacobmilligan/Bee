@echo off

set VSWHERE_VER=2.8.4
set CMAKE_VER=3.18.2

if NOT EXIST .\Binaries (
    mkdir .\Binaries
)

set SEVENZIP=.\7z\x64\7za.exe

call :extract curl curl
set CURL=.\Binaries\curl\bin\curl.exe

set VSWHERE_URL=https://github.com/microsoft/vswhere/releases/download/%VSWHERE_VER%/vswhere.exe
set VSWHERE=.\Binaries\vswhere.exe

if NOT EXIST %VSWHERE% (
    echo.
    echo bb: downloading required dependency "vswhere"
    echo     from %VSWHERE_URL%
    echo.
    %CURL% -L %VSWHERE_URL% --output %VSWHERE%
)

set CMAKE_FILENAME=cmake-%CMAKE_VER%-win64-x64
set CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v%CMAKE_VER%/%CMAKE_FILENAME%.zip
set CMAKE=.\Binaries\cmake

if NOT EXIST %CMAKE% (
    echo.
    echo bb: downloading required dependency "cmake"
    echo     from %CMAKE_URL%
    echo.
    %CURL% -L %CMAKE_URL% --output %CMAKE%.zip
    %SEVENZIP% x %CMAKE%.zip -o.\Binaries
    ren .\Binaries\%CMAKE_FILENAME% cmake
    del %CMAKE%.zip
)

call :extract dxc DirectXShaderCompiler
call :extract luajit LuaJIT

exit /b 0

:extract
if NOT exist ".\Binaries\%2" (
    %SEVENZIP% x .\%1-win64.7z -o".\Binaries\%2"
)
exit /b 0