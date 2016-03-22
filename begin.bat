@echo off

setlocal enabledelayedexpansion

IF NOT [%1] == [] (

	set name=%1

	call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x64
	echo vc13 x64

	IF EXIST bin\!name!.exe (
		start devenv bin\!name!.exe
		echo devenv bin\!name!.exe
	) else (
		echo Devenv not started, executable bin\!name!.exe does not exist.
	)

	IF EXIST !name!.sublime-project (
		start E:\Programs\sublimetext\sublime_text.exe --project !name!.sublime-project 
		echo sublime !name!
	) else (
		echo Sublime not started, project !name!.sublime-project does not exist.
	)
)