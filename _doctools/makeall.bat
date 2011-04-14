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
@for /f %%i in ('%PYTHON% getconfvar.py release') do @set VER=-%%i
set CHMFILE=""
@for /f %%i in ('%PYTHON% getconfvar.py htmlhelp_basename') do @set CHMFILE=%%i
set PRJNAME=""
@for /f %%i in ('%PYTHON% getconfvar.py project') do @set PRJNAME=%%i
CALL make.bat html
CALL make.bat htmlhelp
cd ..

%PYTHON% makezip.py publish/%PRJNAME%-html%VER%.zip build/_build/html

set CHMBUILDPATH=tools\htmlhelpworkshop
set BUILDCHM=/b 0
IF EXIST %CHMBUILDPATH%\hhc.exe set BUILDCHM=/b 1


set CHMBUILDPATH=tools\htmlhelpworkshop
set BUILDCHM=/b 0
IF EXIST %CHMBUILDPATH%\hhc.exe set BUILDCHM=/b 1


IF "%BUILDCHM" == /b 1 (
    SET SC=%cd%
    SET HHPFILE=%cd%\build\_build\htmlhelp\%CHMFILE%.hhp
    cd %CHMBUILDPATH% 
    hhc.exe %HHPFILE%
    cd %SC%
)

IF EXIST build\_build\htmlhelp\%CHMFILE%.chm set BUILDCHM=/b 0

IF "%BUILDCHM" == /b 1 (
    echo. chmpackage
    copy build\_build\htmlhelp\%CHMFILE%.chm publish\%CHMFILE%%VER%.chm
    %PYTHON% makezip.py publish/%CHMFILE%-chm%VER%.zip public/%CHMFILE%%VER%.chm 
)

goto :EOF

:IF_EXIST
SETLOCAL&PATH %PATH%;%~dp0;%cd%
if "%~$PATH:1"=="" exit /b 1
exit /b 0

:end
