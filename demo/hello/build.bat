@echo off

REM echo del /f/q hello.obj hello.exe
REM del /f/q hello.obj hello.exe

echo cl.exe /nologo /c /D _CRT_SECURE_NO_WARNINGS hello.c
cl.exe /nologo /c /D _CRT_SECURE_NO_WARNINGS hello.c

echo cl.exe /nologo hello.obj
cl.exe /nologo hello.obj
