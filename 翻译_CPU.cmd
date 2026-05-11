@echo off
setlocal

set "DESKTOP_TRANSLATE_LLAMA_FORCE_CPU=1"

set "APP="
if exist "%~dp0Release\desktop_translate.exe" set "APP=%~dp0Release\desktop_translate.exe"
if not defined APP if exist "%~dp0Debug\desktop_translate.exe" set "APP=%~dp0Debug\desktop_translate.exe"
if not defined APP if exist "%~dp0desktop_translate.exe" set "APP=%~dp0desktop_translate.exe"

if not defined APP (
  echo desktop_translate.exe not found under "%~dp0".
  pause
  exit /b 1
)

start "" "%APP%"
exit /b 0
