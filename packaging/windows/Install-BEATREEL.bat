@echo off
setlocal
title BEATREEL installer

rem --- needs admin rights to write into Program Files\Common Files\VST3 ---
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Asking for administrator rights...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set "HERE=%~dp0"
set "VST3DIR=%CommonProgramFiles%\VST3"
set "DATADIR=%APPDATA%\BEATREEL"

echo.
echo  Installing BEATREEL...
echo.

if not exist "%VST3DIR%" mkdir "%VST3DIR%"
if exist "%VST3DIR%\BEATREEL.vst3" rmdir /s /q "%VST3DIR%\BEATREEL.vst3"
xcopy /e /i /q /y "%HERE%BEATREEL.vst3" "%VST3DIR%\BEATREEL.vst3" >nul || goto :fail
echo  [ok] VST3   -^> %VST3DIR%\BEATREEL.vst3

if not exist "%DATADIR%" mkdir "%DATADIR%"
if exist "%HERE%ffmpeg.exe" (
    copy /y "%HERE%ffmpeg.exe" "%DATADIR%\ffmpeg.exe" >nul || goto :fail
    echo  [ok] ffmpeg -^> %DATADIR%\ffmpeg.exe
)

if not exist "%ProgramFiles%\BEATREEL" mkdir "%ProgramFiles%\BEATREEL"
copy /y "%HERE%BEATREEL.exe" "%ProgramFiles%\BEATREEL\BEATREEL.exe" >nul
echo  [ok] Standalone app -^> %ProgramFiles%\BEATREEL\BEATREEL.exe

echo.
echo  Done. Rescan plugins in your DAW (FL Studio: Options ^> Manage plugins ^> Find plugins)
echo  and put BEATREEL on your master track.
echo.
pause
exit /b 0

:fail
echo.
echo  Something went wrong. Close your DAW and run this installer again.
pause
exit /b 1
