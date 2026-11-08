@echo off
chcp 65001 > nul
echo ============================================================
echo   BIÊN DỊCH DỰ ÁN MẠNG XÃ HỘI WEB C++ (G++ / UCRT64)
echo ============================================================
echo Đang biên dịch mã nguồn C++ với SQLite3 và Winsock...

g++ -std=c++17 src\main.cpp -Iinclude -Isrc -lsqlite3 -lws2_32 -lcrypt32 -O2 -o server.exe

if %ERRORLEVEL% EQU 0 (
    echo [THÀNH CÔNG] File thực thi server.exe đã được tạo thành công!
) else (
    echo [LỖI] Quá trình biên dịch thất bại. Vui lòng kiểm tra lại trình biên dịch g++.
)
pause
