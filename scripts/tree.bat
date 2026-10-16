@echo off
setlocal enabledelayedexpansion

rem Repo tree snapshot: scripts\tree.ps1 with this repo's exclude list,
rem written to scripts\info.txt (git-ignored). See tree.ps1 for options.

set "SCRIPT=%~dp0tree.ps1"
set "OUTFILE=%~dp0info.txt"
set "EXCLUDES=node_modules .git build .codegraph 3rdparty dist out temp .vs .idea"

rem Scan root is pinned to this repo (the script's parent dir), not the caller's
rem cwd -- otherwise running it from elsewhere snapshots someone else's tree.
for %%I in ("%~dp0..") do set "ROOT=%%~fI"

set "EXCLUDE_ARG="
for %%E in (%EXCLUDES%) do set "EXCLUDE_ARG=!EXCLUDE_ARG!,'%%E'"
set "EXCLUDE_ARG=!EXCLUDE_ARG:~1!"

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "& '%SCRIPT%' -Path '%ROOT%' -Exclude @(%EXCLUDE_ARG%) -ShowFiles | Out-File -FilePath '%OUTFILE%' -Encoding utf8"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] exit code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo Output written to: %OUTFILE%
exit /b 0
