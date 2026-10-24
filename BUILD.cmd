@echo off
if exist "%LOCALAPPDATA%\Programs\Python\Python312-embed\python.exe" (
  "%LOCALAPPDATA%\Programs\Python\Python312-embed\python.exe" -B "%~dp0build\build_player_runtime.py" %*
  exit /b
)
py -3 "%~dp0build\build_player_runtime.py" %*
