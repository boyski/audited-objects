@echo on

REM Usage: cmd /c "build-for-ao.bat <srcdir>
REM Currently confgured for MSVC 8, -MTx.

cd %1%

del *.obj *.exe *.lib *.pdb

REM Release build
del *.obj
cl -c -nologo -DWIN32 -DUSE_LONGLONG -D_CRT_SECURE_NO_WARNINGS -DNDEBUG /Zl -MT /O2 /GF /EHsc /W3 trio*.c
lib /out:trioMT.lib trio*.obj

REM Debug build
del *.obj
cl -c -nologo -DWIN32 -DUSE_LONGLONG -D_CRT_SECURE_NO_WARNINGS -DDEBUG -D_DEBUG /Zi /Zl -MTd /Od /GS /RTC1 /TC /GF /EHsc /W3 /FdtrioMTd trio*.c
lib /out:trioMTd.lib trio*.obj

del *.obj
