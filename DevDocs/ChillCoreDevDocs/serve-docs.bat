@echo off
REM Double-click launcher for serve-docs.sh.
REM
REM Only needed if .sh files are not associated with Git Bash on this machine.
REM If double-clicking serve-docs.sh already works, this file can be deleted.
REM
REM Deliberately locates Git Bash by path rather than calling "bash", because
REM C:\Windows\System32\bash.exe is the WSL shim and usually comes first on PATH.

title ChillCore Dev Docs

set "BASH_EXE="
if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH_EXE=%ProgramFiles%\Git\bin\bash.exe"
if not defined BASH_EXE if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH_EXE=%ProgramFiles(x86)%\Git\bin\bash.exe"
if not defined BASH_EXE if exist "%LocalAppData%\Programs\Git\bin\bash.exe" set "BASH_EXE=%LocalAppData%\Programs\Git\bin\bash.exe"
if not defined BASH_EXE if exist "C:\msys64\usr\bin\bash.exe" set "BASH_EXE=C:\msys64\usr\bin\bash.exe"

if not defined BASH_EXE (
    echo ERROR: No bash shell found.
    echo Install Git for Windows from https://git-scm.com/download/win
    echo.
    pause
    exit /b 1
)

REM Open the browser after a short delay, so the server has time to bind.
start "" /min cmd /c "timeout /t 6 /nobreak >NUL & start http://127.0.0.1:8000"

cd /d "%~dp0"
"%BASH_EXE%" -c "./serve-docs.sh"

if errorlevel 1 (
    echo.
    echo The docs server exited with an error.
    pause
)
