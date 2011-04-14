@ECHO OFF

set PYTHON=python
CALL :IF_EXIST python.exe || set PYTHON=

IF NOT EXIST build mkdir build
IF NOT EXIST publish mkdir publish

%PYTHON% readver.py .\ build\ver force

cd scripts
CALL compile.bat
cd ..

cd build
set VER=""
@for /f %%i in ('%PYTHON% getver.py') do @set VER=-%%i
CALL make.bat html
CALL make.bat htmlhelp
cd ..

%PYTHON% makezip.py publish/amulet-html%VER%.zip build/_build/html

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
    copy build\_build\htmlhelp\Amulet.chm publish\Amulet%VER%.chm
    %PYTHON% makezip.py publish/amulet-chm%VER%.zip public/Amulet%VER%.chm 
)

goto :EOF

:IF_EXIST
SETLOCAL&PATH %PATH%;%~dp0;%cd%
if "%~$PATH:1"=="" exit /b 1
exit /b 0

:end
