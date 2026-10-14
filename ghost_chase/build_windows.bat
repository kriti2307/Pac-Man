@echo off
rem Builds both programs with MinGW g++ (MSYS2). Run this from the project folder.
echo Building the terminal version (ghost_chase.exe)...
g++ -O2 -std=c++17 -static src/main.cpp -o ghost_chase.exe
if errorlevel 1 goto fail
echo Building the web version (ghost_server.exe)...
g++ -O2 -std=c++17 -static src/server.cpp -o ghost_server.exe -lws2_32
if errorlevel 1 goto fail
echo.
echo Done. Start the web version with:  ghost_server.exe
goto end
:fail
echo.
echo Build failed. Check that g++ works: g++ --version
:end
pause
