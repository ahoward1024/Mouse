@echo off
REM /F2147483648
set cmp=/Zi /FC /F2147483648
set inc=-I..\inc
set lnk=/SUBSYSTEM:CONSOLE ..\lib\*.lib opengl32.lib

IF NOT EXIST bin\ mkdir bin\
pushd bin\
cl %cmp% ..\mouse.cpp %inc% /link %lnk%
popd