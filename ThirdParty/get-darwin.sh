#!/usr/bin/env bash

if ![ -f ".\Binaries" ]; then
    mkdir .\Binaries
fi

set CMAKE_VER=3.15.4
set CMAKE_FILENAME=cmake-%CMAKE_VER%-macos-x64
set CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v%CMAKE_VER%/%CMAKE_FILENAME%.zip
set CMAKE=.\Binaries\cmake

if ![ -f $CMAKE ]; then
    echo.
    echo bb: downloading required dependency "cmake"
    echo     from %CMAKE_URL%
    echo.
    %CURL% -L %CMAKE_URL% --output %CMAKE%.zip
    %SEVENZIP% x %CMAKE%.zip -o.\Binaries
    ren .\Binaries\%CMAKE_FILENAME% cmake
    del %CMAKE%.zip
fi

set DXC=.\Binaries\DirectXShaderCompiler

if NOT EXIST %DXC% (
    %SEVENZIP% x .\dxc-win64.7z -o%DXC%
)
