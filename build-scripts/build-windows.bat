@echo on
@rem Usage: build-windows {32|64} {msvc|mingw}
setlocal ENABLEDELAYEDEXPANSION
set MSYS=C:\msys64
if %2 == msvc (
    if %1 == 64 (
       call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
       call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
    )
    !MSYS!\usr\bin\bash.exe -l %CD%/build-scripts/build-windows %1 %2
) else (
    !MSYS!\usr\bin\env.exe MSYSTEM=MINGW64 /bin/bash -l %CD%/build-scripts/build-windows %1 %2
)
