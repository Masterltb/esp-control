@echo off
title ESP32-S3 Real-Time Web Monitor
color 0b
echo ===================================================================
echo       ESP32-S3 SERVO TELEMETRY ^& REAL-TIME CONTROL CENTER
echo ===================================================================
echo.
echo [*] Dang chuyen ve thu muc du an: %~dp0
cd /d "%~dp0"
echo [*] Dang tu dong mo trinh duyet Web...
start http://localhost:8000
echo [*] Web Server dang chay tai: http://localhost:8000
echo [*] LUU Y: Cua so nay phai luon mo de duy tri ket noi voi ESP32!
echo [*] Nhan CTRL+C neu muon dung server.
echo.
python server.py
pause
