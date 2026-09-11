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
echo   -m, --target ^<target^>   Build target (e.g. ETestStudio), omit for all
echo   -d, --deploy              Run windeployqt after build
echo   -p, --package             Run windeployqt + ISCC after build
echo   -c, --configure           Only run CMake configure, skip build
echo   -h, --help                Show this help
echo.
echo Architecture notes:
echo   VS2019_CMD_DIR should point to the directory containing vcvars64.bat and vcvars32.bat
echo.
echo Examples:
echo   build_ninja.bat
echo   build_ninja.bat debug deploy
echo   build_ninja.bat -t debug -m ETestStudio
echo   build_ninja.bat -t relwithdebinfo -m ETestStudio -p
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

:: Init vcvars env for the chosen arch
if not defined VS2019_CMD_DIR (
    echo VS2019_CMD_DIR env var not set.
    echo Set it to the directory containing vcvars64.bat and vcvars32.bat.
    exit /b 1
)

if /i "%ARCH%"=="x86" (
    call "%VS2019_CMD_DIR%\vcvars32.bat"
) else (
    call "%VS2019_CMD_DIR%\vcvars64.bat"
)

set PATH=%PATH%;"%ETest_CMake_Path%"

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
if defined TARGET echo Build target: %TARGET%

cmake -S . --preset %PRESET%

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
    cmake --build %BUILD_DIR% --target %TARGET%
) else (
    cmake --build %BUILD_DIR%
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
