@echo on
@rem Usage: build-windows {32|64} {msvc|mingw}
if %2 == msvc (
    if %1 == 64 (
       call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
       call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
    )
    choco install zip
    bash ./build-scripts/build-windows %1 %2
) else (
    @rem The vs2017-win2016 pool has an ancient 64-bit-only mingw.
    @rem Install msys2 so we can get current gcc toolchains.
    choco install msys2
    C:\tools\msys64\usr\bin\env.exe MSYSTEM=MINGW64 /bin/bash -l %CD%/build-scripts/build-windows %1 %2
)
