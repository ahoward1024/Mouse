@echo off

set cmp=/Zi /FC
set inc=-I..\inc
set lnk=/SUBSYSTEM:CONSOLE ..\lib\*.lib opengl32.lib

IF NOT EXIST bin\ mkdir bin\
pushd bin\
cl %cmp% ..\mouse.cpp %inc% /link %lnk%
popd