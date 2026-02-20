@echo off

echo Building the project
echo Ensure you have cmake, clang++ and minGW installed
echo ===================================================
cd SRC
powershell -ExecutionPolicy Bypass -File "build.ps1"
pause
