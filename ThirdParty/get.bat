@echo off

set SEVENZIP=.\7z\x64\7za.exe
set CURL_ZIP=.\curl-win64.7z
set CURL=.\curl\bin\curl.exe

if NOT EXIST .\curl (
    %SEVENZIP% x %CURL_ZIP%
)

if NOT EXIST .\Binaries (
    mkdir .\Binaries
)

set VSWHERE_URL=https://github.com/microsoft/vswhere/releases/download/2.8.4/vswhere.exe
set VSWHERE=.\Binaries\vswhere.exe

if NOT EXIST %VSWHERE% (
    echo.
    echo bb: downloading required dependency "vswhere"
    echo     from %VSWHERE_URL%
    echo.
    %CURL% -L %VSWHERE_URL% --output %VSWHERE%
)

set CMAKE_VER=3.18.2
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

set DXC=.\Binaries\DirectXShaderCompiler

if NOT EXIST %DXC% (
    %SEVENZIP% x .\dxc-win64.7z -o%DXC%
)
