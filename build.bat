@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo   BUILD ConnectSphere C++ Web Server (g++ / UCRT64)
echo ============================================================
echo.

where g++ >nul 2>&1
if errorlevel 1 (
    echo [ERROR] g++ not found in PATH.
    echo Open an MSYS2 UCRT64 terminal and install:
    echo   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-nlohmann-json
    echo Ensure httplib.h is available in the include directory or compiler include path.
    exit /b 1
)

echo Compiling...
g++ -std=c++17 src/main.cpp -Iinclude -Isrc -lsqlite3 -lws2_32 -lcrypt32 -ladvapi32 -O2 -o server.exe
if errorlevel 1 (
    echo.
    echo [FAILED] Compile error. Check that SQLite, nlohmann-json and httplib.h are installed.
    exit /b 1
)

echo.
echo [OK] server.exe created successfully.
echo Run: server.exe   or   run.bat
echo.
endlocal
exit /b 0
