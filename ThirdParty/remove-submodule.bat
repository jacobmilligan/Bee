@echo off

if "%1"=="" (
    echo usage: remove-submodule.bat ^<submodule^>
    exit /b 0
)

pushd %~dp0

git submodule deinit -f -- %1
rmdir /s ..\.git\modules\ThirdParty\%1
git rm -rf %1

exit /b 0
