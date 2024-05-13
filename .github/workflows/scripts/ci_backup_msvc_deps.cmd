@echo off
setlocal enableextensions enabledelayedexpansion
cls
if not exist "projects\msvc\" exit /b 9

set ARCH=
set DEPS=
if "%*" == "" exit /b 8
for %%P in (%*) do (
	if /i "%%P" == "x86" (set ARCH=x86) else (
	if /i "%%P" == "x64" (set ARCH=x64) else (
	set "DEPS=!DEPS! %%P") )
)

if not defined ARCH exit /b 7

if exist "data\" xcopy /e data pkg
if errorlevel 1 exit /b 6
if exist "pkg\mupen64plus.desktop" del /f /q pkg\mupen64plus.desktop

if not defined DEPS (
	echo :: None...
	exit /b 0
)

set PKG=%CD%\pkg
cd ..\mupen64plus-win32-deps
if errorlevel 1 exit /b 5
for %%D in (%DEPS%) do (
	for /f "tokens=*" %%T in ('dir /b /s %%D ^| findstr "%ARCH%"') do (
		if exist "%%T" xcopy "%%T" "%PKG%\"
		if errorlevel 1 exit /b 4
	)
)

exit /b 0
