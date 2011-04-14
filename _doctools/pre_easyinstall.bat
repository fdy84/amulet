@ECHO OFF

set PYTHON=python
CALL :IF_EXIST python.exe || set PYTHON=

%PATHON% pre_easyinstall.py
pause


goto :EOF

:IF_EXIST
SETLOCAL&PATH %PATH%;%~dp0;%cd%
if "%~$PATH:1"=="" exit /b 1
exit /b 0

:end
