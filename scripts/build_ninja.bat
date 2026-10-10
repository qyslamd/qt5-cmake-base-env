@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

:: Change to source root before any argument parsing,
:: so later %~dp0 references remain stable.
set "PROJECT_ROOT=%~dp0"
cd /d "%PROJECT_ROOT%.."

:: =======================================
:: 参数解析
:: =======================================
set "BUILD_TYPE="
set "TARGET="
set "ACTION="
set "CONFIGURE_ONLY="
set "ARCH=x64"

set "FIRST_ARG=%~1"
if not defined FIRST_ARG goto :old_mode
set "FIRST_CHAR=!FIRST_ARG:~0,1!"
if "!FIRST_CHAR!"=="-" goto :parse_new

:old_mode
set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=debug"
if /i "%~2"=="deploy"  set "ACTION=deploy"
if /i "%~2"=="package" set "ACTION=package"
goto :setup_build

:parse_new
if "%~1"=="" goto :setup_new

if /i "%~1"=="-h" goto :show_help
if /i "%~1"=="--help" goto :show_help

if /i "%~1"=="-d" (
    set "ACTION=deploy"
    shift
    goto :parse_new
)
if /i "%~1"=="--deploy" (
    set "ACTION=deploy"
    shift
    goto :parse_new
)
if /i "%~1"=="-p" (
    set "ACTION=package"
    shift
    goto :parse_new
)
if /i "%~1"=="--package" (
    set "ACTION=package"
    shift
    goto :parse_new
)

if /i "%~1"=="-c" (
    set "CONFIGURE_ONLY=1"
    shift
    goto :parse_new
)
if /i "%~1"=="--configure" (
    set "CONFIGURE_ONLY=1"
    shift
    goto :parse_new
)

if /i "%~1"=="-a" (
    set "ARCH=%~2"
    shift
    shift
    goto :parse_new
)
if /i "%~1"=="--arch" (
    set "ARCH=%~2"
    shift
    shift
    goto :parse_new
)

set "ARG=%~1"
set "PREFIX=!ARG:~0,7!"
if /i "!PREFIX!"=="--type=" (
    set "BUILD_TYPE=!ARG:~7!"
    shift
    goto :parse_new
)

if /i "%~1"=="-t" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto :parse_new
)
if /i "%~1"=="--type" (
    set "BUILD_TYPE=%~2"
    shift
    shift
    goto :parse_new
)

set "PREFIX=!ARG:~0,9!"
if /i "!PREFIX!"=="--target=" (
    set "TARGET=!ARG:~9!"
    shift
    goto :parse_new
)

if /i "%~1"=="-m" (
    set "TARGET=%~2"
    shift
    shift
    goto :parse_new
)
if /i "%~1"=="--target" (
    set "TARGET=%~2"
    shift
    shift
    goto :parse_new
)

echo Unknown argument: %~1
goto :show_help

:show_help
echo Usage:
echo   Legacy:  build_ninja.bat [debug^|relwithdebinfo^|release] [deploy^|package]
echo   Modern:  build_ninja.bat -t ^<type^> [-a ^<arch^>] [-m ^<target^>] [-d^|-p]
echo.
echo Options:
echo   -t, --type ^<type^>       Build type: debug / relwithdebinfo / release
echo   -a, --arch ^<arch^>       Target architecture: x64 (default) / x86
echo   -m, --target ^<target^>   Build target (e.g. demo_app), omit for all
echo   -d, --deploy              Run windeployqt after build
echo   -p, --package             Run windeployqt + ISCC after build
echo   -c, --configure           Only run CMake configure, skip build
echo   -h, --help                Show this help
echo.
echo Architecture notes:
echo   VS2019 is located automatically via vswhere (shipped with the
echo   VS Installer). Install VS2019 (or VS2019 Build Tools) with the
echo   Desktop C++ workload for either x64 or x86 support.
echo.
echo Toolchain notes:
echo   ninja and cmake come from 3rdparty\ (version pinned, ninja ^>= 1.10.0),
echo   not from the copies VS2019 happens to ship. VS2019 provides the
echo   compiler and the Windows SDK only.
echo.
echo Examples:
echo   build_ninja.bat
echo   build_ninja.bat debug deploy
echo   build_ninja.bat -t debug -m demo_app
echo   build_ninja.bat -t relwithdebinfo -m demo_app -p
echo   build_ninja.bat -t debug -a x86 -c
exit /b 1

:: =======================================
:: Build setup
:: =======================================
:setup_new
:setup_build

:: Validate arch
if /i not "%ARCH%"=="x64" if /i not "%ARCH%"=="x86" (
    echo Unknown arch: %ARCH% ^(expected x64 or x86^)
    exit /b 1
)

:: Init vcvars env for the chosen arch.
:: VS2019 is located via vswhere (shipped with the VS Installer);
:: no environment variables are required.
set "VCVARS_DIR="
call :find_vs2019

if not defined VCVARS_DIR (
    echo VS2019 not found.
    echo Install Visual Studio 2019 ^(or VS2019 Build Tools^) with the
    echo Desktop C++ workload, then re-run this script.
    exit /b 1
)

set "VCVARS=%VCVARS_DIR%\vcvars64.bat"
if /i "%ARCH%"=="x86" set "VCVARS=%VCVARS_DIR%\vcvars32.bat"
if not exist "%VCVARS%" (
    echo vcvars script not found: "%VCVARS%"
    exit /b 1
)

call "%VCVARS%"
if errorlevel 1 (
    echo Failed to initialize VS2019 %ARCH% environment.
    exit /b 1
)

:: Toolchain: the copies shipped in 3rdparty are the ones builds use, so every
:: machine configures with the same versions. The presets already point ninja
:: at the bundled copy; the check here turns a missing file into a clear
:: message instead of CMake failing to run a program that is not there.
set "BUNDLED_NINJA=%PROJECT_ROOT%..\3rdparty\ninja.exe"
if not exist "%BUNDLED_NINJA%" (
    echo Bundled ninja not found: "%BUNDLED_NINJA%"
    echo It is version pinned ^(1.10.0 or newer^) and tracked in git; restore it.
    exit /b 1
)

set "CMAKE_CMD=%PROJECT_ROOT%..\3rdparty\cmake\bin\cmake.exe"
if not exist "%CMAKE_CMD%" (
    echo Warning: bundled cmake not found, falling back to the one on PATH.
    set "CMAKE_CMD=cmake"
)

if "%BUILD_TYPE%"=="" set "BUILD_TYPE=debug"

if /i "%BUILD_TYPE%"=="debug" (
    set "PRESET=ninja-debug-%ARCH%"
    set "BUILD_DIR=build\ninja-debug-%ARCH%"
)
if /i "%BUILD_TYPE%"=="relwithdebinfo" (
    set "PRESET=ninja-relwithdebinfo-%ARCH%"
    set "BUILD_DIR=build\ninja-relwithdebinfo-%ARCH%"
)
if /i "%BUILD_TYPE%"=="release" (
    set "PRESET=ninja-release-%ARCH%"
    set "BUILD_DIR=build\ninja-release-%ARCH%"
)

if not defined PRESET (
    echo Unknown build type: %BUILD_TYPE%
    echo Usage: build_ninja.bat -t ^<debug^|relwithdebinfo^|release^> [-a ^<x64^|x86^>] [-m ^<target^>] [-d^|-p]
    exit /b 1
)

echo Build type: %BUILD_TYPE% (%PRESET%)
echo Target arch: %ARCH%
echo Toolchain : %CMAKE_CMD% + %BUNDLED_NINJA%
if defined TARGET echo Build target: %TARGET%

"%CMAKE_CMD%" -S . --preset %PRESET%

if errorlevel 1 (
    echo CMake configure failed
    exit /b 1
)

echo Configure OK

if defined CONFIGURE_ONLY (
    echo Configure only mode, skipping build.
    exit /b 0
)

if defined TARGET (
    "%CMAKE_CMD%" --build %BUILD_DIR% --target %TARGET%
) else (
    "%CMAKE_CMD%" --build %BUILD_DIR%
)
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build OK!

if /i "%ACTION%"=="deploy" (
    echo.
    echo Running windeployqt ...
    cmake --build %BUILD_DIR% --target make_deploy
    if errorlevel 1 (
        echo windeployqt failed
        exit /b 1
    )
    echo windeployqt done!
)

if /i "%ACTION%"=="package" (
    echo.
    echo Packaging ...
    cmake --build %BUILD_DIR% --target make_package
    if errorlevel 1 (
        echo Package failed
        exit /b 1
    )
    echo Package done!
)

exit /b 0

:: =======================================
:: Locate VS2019 via vswhere.
:: Sets VCVARS_DIR to the VS2019 build
:: tools directory on success, leaves it
:: empty on failure.
:find_vs2019
:: vswhere lives under "Program Files (x86)", a path containing spaces.
:: Putting a quoted, space-containing program path inside for /f's
:: backtick form is fragile: the command goes through cmd /c and the
:: quoting gets mangled. Run vswhere directly, redirect to a temp file,
:: then read the file back — no nested quotes to trip over.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 1

set "VSWHERE_OUT=%TEMP%\vswhere_%RANDOM%%RANDOM%.txt"
"%VSWHERE%" -version "[16.0,17.0)" -property installationPath -products * > "%VSWHERE_OUT%" 2>nul

for /f "usebackq tokens=*" %%I in ("%VSWHERE_OUT%") do (
    rem Keep the first instance that actually ships vcvars64.bat,
    rem so we skip VS2019 installs without the Desktop C++ workload.
    if not defined VCVARS_DIR (
        if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" (
            set "VCVARS_DIR=%%I\VC\Auxiliary\Build"
        )
    )
)

del "%VSWHERE_OUT%" >nul 2>nul
exit /b 0