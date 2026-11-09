@echo off
chcp 65001 >nul
net session >nul 2>&1
if %errorlevel% neq 0 (
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
cd /d "%~dp0"
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%DEST%" mkdir "%DEST%"
for /d %%B in (*.vst3) do (
  if exist "%DEST%\%%B" rmdir /s /q "%DEST%\%%B"
  xcopy "%%B" "%DEST%\%%B\" /E /I /Q /Y >nul && echo OK  %%B
)
echo.
echo Installation terminee dans %DEST%
echo Dans FL Studio : Options ^> Manage plugins ^> Find installed plugins.
pause
