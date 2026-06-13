@echo on
@rem Usage: run-vcpkg.bat {x64-windows-static|x64-mingw-static|x86-windows-static|x86-mingw-static}
setlocal ENABLEDELAYEDEXPANSION
set VCVARS_SCRIPT=
set VALID_TRIPLET=
if "%1" == "x64-windows-static" (
    set VCVARS_SCRIPT=vcvars64.bat
    set VALID_TRIPLET=1
) else if "%1" == "x86-windows-static" (
    set VCVARS_SCRIPT=vcvars32.bat
    set VALID_TRIPLET=1
) else if "%1" == "x64-mingw-static" (
    set VALID_TRIPLET=1
) else if "%1" == "x86-mingw-static" (
    set VALID_TRIPLET=1
)
if not defined VALID_TRIPLET (
    echo Usage: run-vcpkg.bat {x64-windows-static^|x64-mingw-static^|x86-windows-static^|x86-mingw-static}
    exit /b 2
)
if defined VCVARS_SCRIPT (
    set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
    if not exist "!VSWHERE!" (
        set VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe
    )
    set VSINSTALL=
    set VCVARS=
    if exist "!VSWHERE!" (
        rem Find the latest Visual Studio with C++ build tools installed.
        for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%I
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
)
set MSYS2_PATH_TYPE=inherit
C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 /bin/bash -l %CD%/build-scripts/run-vcpkg %1
