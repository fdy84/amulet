@ECHO OFF

set PYTHON=python
CALL :IF_EXIST python.exe || set PYTHON=

set SPHINXBUILD=sphinx-build

set PREPARED=/b 0
CALL :IF_EXIST %SPHINXBUILD%.exe || set PREPARED=/b 1

IF "%PREPARED" == /b 1 ( 
    @for /f %%i in ('%PYTHON% getpytoolpath.py') do @set SPHINXBUILD=%%i\%SPHINXBUILD% 
)

IF EXIST %SPHINXBUILD%.exe set PREPARED=/b 0

if "%PREPARED" == /b 1 (
    set EASYINSTALL=easy_install
    set PREEI=/b 0
    CALL :IF_EXIST %SPHINXBUILD%.exe || set PREPARED=/b 1
    if "%PREEI" == /b 1 (
        @for /f %%i in ('%PYTHON% getpytoolpath.py') do @set EASYINSTALL=%%i\%EASYINSTALL%
        IF EXIST %EASYINSTALL%.exe set PREPARED=/b 0
    )
    if "%PREEI" == /b 1 (
        %PYTHON% pre_easyinstall.py
    )
    
    %EASYINSTALL% -U Sphinx
)

goto :EOF

:IF_EXIST
SETLOCAL&PATH %PATH%;%~dp0;%cd%
if "%~$PATH:1"=="" exit /b 1
exit /b 0

:end
