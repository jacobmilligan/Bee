@echo off

set BUILD_DIR=.\Build\Bootstrap
set CMAKE_EXE=.\ThirdParty\Binaries\cmake\bin\cmake.exe

cd .\ThirdParty
call .\get.bat
cd ..

if NOT EXIST .\Build\Release\bb.exe (
    echo.
    echo bb.exe not found
    echo Running bootstrap build...
    echo.

    %CMAKE_EXE% . -B %BUILD_DIR% -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release -DMONOLITHIC_BUILD=ON -DBUILD_TESTS=OFF -DENABLE_MEMORY_TRACKING=OFF -DDISABLE_REFLECTION=ON
    %CMAKE_EXE% --build %BUILD_DIR% --target bb --config Release
    rmdir /s /q %BUILD_DIR%
)

.\Build\Release\bb.exe %*