@echo off

if "%1"=="" (
    echo usage: update-submodule.bat ^<submodule^>
    exit /b 0
)

pushd %~dp0

git submodule update --remote --merge -- %1

exit /b 0