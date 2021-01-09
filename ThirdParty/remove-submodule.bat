@echo off

if "%1"=="" (
    echo usage: remove-submodule.bat ^<submodule^>
    exit /b 0
)

pushd ..

git submodule deinit -f -- ThirdParty\%1
rmdir /s .git\modules\ThirdParty\%1
git rm -rf ThirdParty\%1

exit /b 0
