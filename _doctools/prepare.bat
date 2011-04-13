@ECHO OFF

set PYTHON="python"
CALL :IF_EXIST python.exe || set PYTHON=""

%PYTHON% pre_easyinstall.py

goto :EOF

:IF_EXIST BY chenall QQ:366840202 2009-09-29 http://www.chenall.com
SETLOCAL&PATH %PATH%;%~dp0;%cd%
if "%~$PATH:1"=="" exit /b 1
exit /b 0

:end
