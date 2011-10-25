@echo on

REM Usage: cmd /c .\build-for-ao.bat
REM Requires Cmake
REM Requires that libgit2 repo already exist.

rd /s/q %OS_CPU%
mkdir %OS_CPU%
cd %OS_CPU%
cmake -DBUILD_SHARED_LIBS=NO ..\repo
msbuild libgit2.sln /p:Configuration=Release /v:n
msbuild libgit2.sln /p:Configuration=Debug /v:n
