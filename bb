#!/usr/bin/env bash

BUILD_DIR=./Build/Bootstrap
CMAKE_EXE=./ThirdParty/Binaries/CMake.app/Contents/bin/cmake

cd ./ThirdParty
if [[ $OSTYPE == "darwin"* ]]; then
    ./get-darwin.sh
fi
cd ..

if [[ ! -f ./Build/Release/bb ]]; then
    echo
    echo bb not found
    echo Running bootstrap build...
    echo

    $CMAKE_EXE . -B $BUILD_DIR -DMONOLITHIC_BUILD=ON -DBUILD_TESTS=OFF -DENABLE_MEMORY_TRACKING=OFF -DENABLE_REFLECTION=OFF
    $CMAKE_EXE --build $BUILD_DIR --target bb --config Release
    rm -r $BUILD_DIR
fi

# .\Build\Release\bb %*
