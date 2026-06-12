@echo on
@rem Usage: build-windows {32|64} {msvc|mingw}
setlocal ENABLEDELAYEDEXPANSION
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if %2 == msvc (
    set VSINSTALL=
    set VCVARS=
    if exist "%VSWHERE%" (
        for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%I
        if defined VSINSTALL (
            if %1 == 64 (
                set VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat
            ) else (
                set VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars32.bat
            )
        )
    )
    if not defined VCVARS (
        echo Could not locate a Visual Studio installation with C++ build tools.
        exit /b 1
    )
    call "!VCVARS!"
    if errorlevel 1 (
        echo Failed to initialize MSVC build environment.
        exit /b !errorlevel!
    )
    choco install zip
    choco install nsis
    bash ./build-scripts/build-windows %1 %2
) else (
    choco install nsis
    set MSYS=C:\msys64
    !MSYS!\usr\bin\env.exe MSYSTEM=MINGW64 MSYS2_PATH_TYPE=inherit /bin/bash -l %CD%/build-scripts/build-windows %1 %2
)
