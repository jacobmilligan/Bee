#!/usr/bin/env bash

CMAKE_VER=3.15.4
CMAKE_FILENAME=cmake-$CMAKE_VER-Darwin-x86_64
CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/$CMAKE_FILENAME.tar.gz
CMAKE=./Binaries/CMake.app
CMAKE_TARBALL=./Binaries/cmake.tar.gz

mkdir -p ./Binaries

if [ ! -d $CMAKE ]; then
    echo bb: downloading required dependency cmake
    echo     from $CMAKE_URL
    echo
    curl -L $CMAKE_URL --output $CMAKE_TARBALL
    # extract cmake.app and move it into the root ./Binaries directory
    tar -xvzf $CMAKE_TARBALL -C ./Binaries
    mv ./Binaries/$CMAKE_FILENAME/CMake.app $CMAKE
    rm -r ./Binaries/$CMAKE_FILENAME
    rm $CMAKE_TARBALL
fi

DXC=./Binaries/DirectXShaderCompiler

if [ ! -d $DXC ]; then
    echo bb: extracting Direct X Shader Compiler
    tar -xvzf ./dxc-macos.tar.gz -C ./Binaries
    mv ./Binaries/dxc-macos $DXC
fi
