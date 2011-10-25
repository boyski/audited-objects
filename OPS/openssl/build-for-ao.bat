@echo on

REM Usage: cmd /c "build-for-ao.bat <srcdir>
REM Currently confgured for MSVC 8, -MTx.
REM May require Netwide Assembler (nasm) - see OpenSSL docs.

cd %1%

del *.obj *.exe *.lib *.pdb

perl Configure VC-WIN32 no-idea no-rc5 no-mdc2 %*

ms\do_nasm

nmake -f ms\nt.mak
