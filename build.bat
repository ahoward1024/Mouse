@echo off
REM /F2147483648
set cmp=/Zi /FC
set inc=-I..\inc
set lnk=/SUBSYSTEM:CONSOLE ..\lib\*.lib opengl32.lib ..\mouse.res

IF NOT EXIST bin\ mkdir bin\
pushd bin\
cl %cmp% ..\mouse.cpp %inc% /link %lnk%
popd