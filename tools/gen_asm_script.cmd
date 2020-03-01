@echo off
setlocal enableextensions disabledelayedexpansion
echo "%~dp0" | find /i "\mupen64plus-core\tools\" >nul
if errorlevel 1 exit /b 1
set R1=1
:L1
set R2=
for /f "tokens=%R1% delims=\" %%a in ('CD') do set R2=%%a
if "%R2%" NEQ "" set /a R1=%R1%+1& goto L1
set /a R1=%R1%-3
if "%R1%" LEQ "1" exit /b 2
set R1=
set R2=
set R3=
for /f "tokens=1,2*" %%a in ('echo %*') do set R1=%%a& set R2=%%b& set R3=%%c
if not exist "%R2%asm_defines.c" exit /b 3
del %R2%asm_defines_* 2>nul
cl /c /Fo%R1% %R3% /I ..\..\src %R2%asm_defines.c
::"..\..\..\mupen64plus-win32-deps\gawk-3.1.6-1\bin\gawk.exe" -v dest_dir="../../src/asm_defines" -f ..\..\tools\gen_asm_defines.awk %R1%asm_defines.obj
if not exist "%R1%asm_defines.obj" exit /b 404
for /f "tokens=2,3" %%a in ('type %R1%asm_defines.obj ^| find "@ASM_DEFINE"') do (
echo #define %%a ^(%%b^)>>%R2%asm_defines_gas.h
echo %%define %%a ^(%%b^)>>%R2%asm_defines_nasm.h
)
exit /b 0