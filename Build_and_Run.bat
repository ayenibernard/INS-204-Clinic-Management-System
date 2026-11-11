@echo off
setlocal
cd /d "%~dp0"
set "QT_ROOT=C:\msys64\ucrt64"
if defined CLINIC_QT_ROOT set "QT_ROOT=%CLINIC_QT_ROOT%"
set "PATH=%QT_ROOT%\bin;%PATH%"
where cmake >nul 2>nul
if errorlevel 1 goto missing
if not exist "%QT_ROOT%\bin\g++.exe" goto missing
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%QT_ROOT%" "-DCMAKE_CXX_COMPILER=%QT_ROOT%/bin/g++.exe"
if errorlevel 1 goto failed
cmake --build build
if errorlevel 1 goto failed
build\ClinicQt.exe
if errorlevel 1 goto failed
exit /b 0
:missing
echo Qt or the build tools were not found.
echo Read README.md for the MSYS2 UCRT64 setup command.
:failed
pause
exit /b 1
