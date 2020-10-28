@echo off

set SEVENZIP=..\ThirdParty\7z\x64\7za.exe

%SEVENZIP% a binaries-win32.7z .\Binaries\* -t7z -m0=lzma2 -mx9 -aoa -mfb64 -md1024m -ms=on