@echo off
chcp 65001 > nul
echo ============================================================
echo   KHỞI ĐỘNG MẠNG XÃ HỘI WEB C++ (CONNECTSPHERE)
echo ============================================================

if not exist "server.exe" (
    echo Chưa tìm thấy server.exe. Đang tự động biên dịch...
    call build.bat
)

echo Đang mở trình duyệt tại: http://localhost:8080
start http://localhost:8080

echo Đang khởi chạy máy chủ C++...
server.exe
pause
