@echo off
setlocal
cd /d "%~dp0"
set "QT_ROOT=C:\msys64\ucrt64"
if defined CLINIC_QT_ROOT set "QT_ROOT=%CLINIC_QT_ROOT%"
set "PATH=%QT_ROOT%\bin;%PATH%"
if not exist build\ClinicQt.exe (
 echo Build the project using Build_and_Run.bat first.
 pause
 exit /b 1
)
build\ClinicQt.exe --self-test
if errorlevel 1 (
 echo Verification failed.
 pause
 exit /b 1
)
echo Verification passed using a temporary database.
pause
