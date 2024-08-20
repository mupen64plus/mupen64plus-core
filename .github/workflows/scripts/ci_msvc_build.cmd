@echo off
setlocal enableextensions disabledelayedexpansion
cls
if not exist "projects\msvc\" exit /b 9

set ARCH=
set CONF=Release
if "%*" == "" exit /b 8
for %%P in (%*) do (
	if /i "%%P" == "x86" set ARCH_ARG=x86& set ARCH=Win32
	if /i "%%P" == "x64" set ARCH_ARG=x64& set ARCH=x64
	if /i "%%P" == "newdyn" set CONF=New_Dynarec_Release
)

if not defined ARCH exit /b 7

for %%T in (.) do set REPO=%%~nxT
if not defined REPO exit /b 6

set ARTIFACT=
set FPROJ=
set EXT=dll
echo %REPO% | findstr "ui-console" >nul 2>&1
if not errorlevel 1 set EXT=exe
if not defined TOOLSET set TOOLSET=v143

for /f "tokens=1" %%R in ('git rev-parse --short HEAD') do set G_REV=%%R
set PKG_NAME=%REPO%-msvc-%ARCH_ARG%-g%G_REV%
if exist "%GITHUB_ENV%" (
	type "%GITHUB_ENV%" | findstr "PKG_NAME=%PKG_NAME%" >nul 2>&1
	if errorlevel 1 echo PKG_NAME=%PKG_NAME%>> "%GITHUB_ENV%"
)

echo.
msbuild --version
echo.

if not exist "..\mupen64plus-win32-deps\" git clone --depth 1 https://github.com/mupen64plus/mupen64plus-win32-deps.git ..\mupen64plus-win32-deps

pushd projects\msvc\
for /f "tokens=*" %%F in ('dir /b *.vcxproj') do set FPROJ=%%F
popd
if not defined FPROJ exit /b 5

echo.
msbuild "projects\msvc\%FPROJ%" /p:Configuration=%CONF%;Platform=%ARCH%;PlatformToolset=%TOOLSET% /t:Rebuild
if errorlevel 1 exit /b 4
echo.

if exist "projects\msvc\%ARCH%\Release\mupen64plus.dll" ren "projects\msvc\%ARCH%\Release\mupen64plus.dll" mupen64plus-old.dll

pushd projects\msvc\%ARCH%\%CONF%\
for /f "tokens=*" %%S in ('dir /b mupen64plus*%EXT%') do set ARTIFACT=%%S
popd
if not defined ARTIFACT exit /b 3

md pkg 2>nul
xcopy "projects\msvc\%ARCH%\%CONF%\%ARTIFACT%" pkg\
if errorlevel 1 exit /b 2
dir "pkg\%ARTIFACT%"

exit /b 0
