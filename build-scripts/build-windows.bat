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
    bash ./build-scripts/build-windows %1 %2
) else (
    if exist C:\msys64 (
       set MSYS=C:\msys64
    ) else (
       choco install msys2
       set MSYS=C:\tools\msys64
    )
    !MSYS!\usr\bin\env.exe MSYSTEM=MINGW64 /bin/bash -l %CD%/build-scripts/build-windows %1 %2
)
