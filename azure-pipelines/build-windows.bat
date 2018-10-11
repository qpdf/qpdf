@echo on
@rem Usage: build-windows {32|64} {msvc|mingw}
if %2 == msvc (
    if %1 == 64 (
       call "%VS140COMNTOOLS%\..\..\VC\bin\amd64\vcvars64.bat"
    ) else (
       call "%VS140COMNTOOLS%\..\..\VC\bin\vcvars32.bat"
    )
    choco install zip
    bash ./azure-pipelines/build-windows %1 %2
) else (
    @rem The vs2017-win2016 pool has an ancient 64-bit-only mingw.
    @rem Install msys2 so we can get current gcc toolchains.
    choco install msys2
    C:\tools\msys64\usr\bin\env.exe MSYSTEM=MINGW64 /bin/bash -l %CD%/azure-pipelines/build-windows %1 %2
)
