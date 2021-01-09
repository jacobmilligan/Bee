@echo off

set SEVENZIP=..\ThirdParty\7z\x64\7za.exe

if NOT EXIST .\Binaries (
    mkdir .\Binaries
)

if NOT EXIST .\Binaries\bee-reflect.exe (
    %SEVENZIP% x .\binaries-win32.7z -o.\Binaries -aos
)

if NOT EXIST .\Binaries\bee-imgui-generator.exe (
    %SEVENZIP% x .\binaries-win32.7z -o.\Binaries -aos
)