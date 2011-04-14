rem @ECHO OFF

set PYTHON=python
CALL :IF_EXIST python.exe || set PYTHON=

%PYTHON% readver.py .\ build\ver

cd scripts
CALL compile.bat
cd ..

cd build
set VER=""
@for /f %%i in ('%PYTHON% getver.py') do @set VER=-%%i
CALL make.bat html
CALL make.bat htmlhelp
cd ..

@ECHO ON

%PYTHON% makezip.py amulet-html%VER%.zip build/_build/html

set CHMBUILDPATH=tools\htmlhelpworkshop
set BUILDCHM=/b 0
IF EXIST %CHMBUILDPATH%\hhc.exe set BUILDCHM=/b 1


set CHMBUILDPATH=tools\htmlhelpworkshop
set BUILDCHM=/b 0
IF EXIST %CHMBUILDPATH%\hhc.exe set BUILDCHM=/b 1


IF "%BUILDCHM" == /b 1 (
    SET SC=%cd%
    SET HHPFILE=%cd%\build\_build\htmlhelp\Amulet.hhp
    cd %CHMBUILDPATH% 
    hhc.exe %HHPFILE%
    cd %SC%
)

IF EXIST build\_build\htmlhelp\Amulet.chm set BUILDCHM=/b 0

IF "%BUILDCHM" == /b 1 (
    echo. chmpackage
    copy build\_build\htmlhelp\Amulet.chm .\Amulet%VER%.chm
    %PYTHON% makezip.py amulet-chm%VER%.zip ./Amulet%VER%.chm 
)

goto :EOF

:IF_EXIST
SETLOCAL&PATH %PATH%;%~dp0;%cd%
if "%~$PATH:1"=="" exit /b 1
exit /b 0

:end
