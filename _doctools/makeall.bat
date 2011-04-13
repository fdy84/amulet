@ECHO OFF

cd scripts
CALL compile.bat
cd ..

cd build
CALL make.bat html
cd ..

:end
