@echo on
@rem Usage: build-windows {32|64} {msvc|mingw}
setlocal ENABLEDELAYEDEXPANSION
if %2 == msvc (
    if %1 == 64 (
       call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
       call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
    )
    choco install zip
    choco install nsis
    set VCPKG_ROOT=%VCPKG_ROOT%
    bash ./build-scripts/build-windows %1 %2
) else (
    choco install nsis
    set MSYS=C:\msys64
    set VCPKG_ROOT=%VCPKG_ROOT%
    !MSYS!\usr\bin\env.exe MSYSTEM=MINGW64 MSYS2_PATH_TYPE=inherit VCPKG_ROOT=!VCPKG_ROOT! /bin/bash -l %CD%/build-scripts/build-windows %1 %2
)
