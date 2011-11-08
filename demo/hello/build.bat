@echo off

REM del /f/q hello.obj goodbye.obj hello.exe

echo cl.exe /nologo /c /D _CRT_SECURE_NO_WARNINGS hello.c
cl.exe /nologo /c /D _CRT_SECURE_NO_WARNINGS hello.c

echo cl.exe /nologo /c /D _CRT_SECURE_NO_WARNINGS goodbye.c
cl.exe /nologo /c /D _CRT_SECURE_NO_WARNINGS goodbye.c

echo cl.exe /nologo hello.obj goodbye.obj
cl.exe /nologo hello.obj goodbye.obj
