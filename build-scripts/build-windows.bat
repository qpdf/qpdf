@echo on
@rem Usage: build-windows {x86|x64|arm64} {msvc|mingw}
setlocal ENABLEDELAYEDEXPANSION
set ARCH=%1
set TOOL=%2
@rem Accept the old word size arguments.
if "%ARCH%" == "32" set ARCH=x86
if "%ARCH%" == "64" set ARCH=x64
set VALID_ARCH=
if "%ARCH%" == "x86" set VALID_ARCH=1
if "%ARCH%" == "x64" set VALID_ARCH=1
if "%ARCH%" == "arm64" set VALID_ARCH=1
if not defined VALID_ARCH (
    echo Usage: build-windows {x86^|x64^|arm64} {msvc^|mingw}
    exit /b 2
)
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
if not exist "!VSWHERE!" (
    set VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe
)
if "%TOOL%" == "msvc" (
    set VSINSTALL=
    set VCVARS=
    set VCVARS_SCRIPT=vcvars64.bat
    set VC_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.x86.x64
    if "%ARCH%" == "x86" set VCVARS_SCRIPT=vcvars32.bat
    if "%ARCH%" == "arm64" (
        set VC_COMPONENT=Microsoft.VisualStudio.Component.VC.Tools.ARM64
        rem Use the cross compiler unless we are already running on ARM64.
        if /i "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
            set VCVARS_SCRIPT=vcvarsarm64.bat
        ) else (
            set VCVARS_SCRIPT=vcvarsamd64_arm64.bat
        )
    )
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires !VC_COMPONENT! -property installationPath`) do set VSINSTALL=%%I
        if defined VSINSTALL (
            set VCVARS=!VSINSTALL!\VC\Auxiliary\Build\!VCVARS_SCRIPT!
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
    bash ./build-scripts/build-windows %ARCH% %TOOL%
) else (
    if "%ARCH%" == "arm64" (
        echo mingw is not supported for arm64
        exit /b 2
    )
    choco install nsis
    set MSYS=C:\msys64
    !MSYS!\usr\bin\env.exe MSYSTEM=MINGW64 MSYS2_PATH_TYPE=inherit /bin/bash -l %CD%/build-scripts/build-windows %ARCH% %TOOL%
)
