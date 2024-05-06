@echo off
setlocal enableextensions disabledelayedexpansion
cls
if not exist "projects\msvc\" exit /b 9

set ARCH=
set CONF=Release
if "%*" == "" exit /b 8
for %%P in (%*) do (
	if /i "%%P" == "x86" set ARCH=Win32
	if /i "%%P" == "x64" set ARCH=x64
	if /i "%%P" == "newdyn" set CONF=New_Dynarec_Release
)

if not defined ARCH exit /b 7

set ARTIFACT=
set FPROJ=
if not defined EXT set EXT=dll
if not defined TOOLSET set TOOLSET=v143
for /f "tokens=1" %%R in ('git rev-parse --short HEAD') do set G_REV=%%R
if exist "%GITHUB_ENV%" (
	type "%GITHUB_ENV%" | findstr "G_REV=%G_REV%" >nul 2>&1
	if errorlevel 1 echo G_REV=%G_REV%>> "%GITHUB_ENV%"
)

echo.
msbuild --version
echo.

if not exist "..\mupen64plus-win32-deps\" git clone --depth 1 https://github.com/mupen64plus/mupen64plus-win32-deps.git ..\mupen64plus-win32-deps

pushd projects\msvc\
for /f "tokens=*" %%F in ('dir /b *.vcxproj') do set FPROJ=%%F
popd
if not defined FPROJ exit /b 6

echo.
msbuild "projects\msvc\%FPROJ%" /p:Configuration=%CONF%;Platform=%ARCH%;PlatformToolset=%TOOLSET% /t:Rebuild
if errorlevel 1 exit /b 5
echo.

if exist "projects\msvc\%ARCH%\Release\mupen64plus.dll" ren "projects\msvc\%ARCH%\Release\mupen64plus.dll" mupen64plus-old.dll

pushd projects\msvc\%ARCH%\%CONF%\
for /f "tokens=*" %%S in ('dir /b mupen64plus*%EXT%') do set ARTIFACT=%%S
popd
if not defined ARTIFACT exit /b 4

md pkg 2>nul
copy "projects\msvc\%ARCH%\%CONF%\%ARTIFACT%" pkg\
if errorlevel 1 exit /b 3
dir "pkg\%ARTIFACT%"

exit /b 0
