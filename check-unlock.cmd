@echo off
setlocal
cd /d "%~dp0"
python -u tools\check_unlock.py %*
set "CHECK_RESULT=%errorlevel%"
echo.
if not defined CHECK_UNLOCK_NO_PAUSE pause
exit /b %CHECK_RESULT%
