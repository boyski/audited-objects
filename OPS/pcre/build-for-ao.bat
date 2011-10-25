@echo on

REM Usage: cmd /c "build-for-ao.bat <srcdir>
REM Currently confgured for MSVC 8, -MTx.

cd %1%

del *.obj *.exe *.lib *.pdb

copy pcre.h.generic pcre.h

set cflags=/TC /Zl /GF /EHsc /W3

set dflags=-DSUPPORT_UTF8 -DLINK_SIZE=2 -DNEWLINE=3338 -DMATCH_LIMIT=10000000 -DMATCH_LIMIT_RECURSION=MATCH_LIMIT -DMAX_NAME_COUNT=10000 -DMAX_NAME_SIZE=32 -DPOSIX_MALLOC_THRESHOLD=10

cl -nologo %dflags% dftables.c
dftables.exe pcre_chartables.c
del dftables.exe dftables.lib

REM Release build
cl -c -nologo %dflags% -DNDEBUG -MT /O2 %cflags% pcre_*.c
lib /out:pcreMT.lib pcre_*.obj

del pcre_*.obj

REM Debug build
cl -c -nologo %dflags% -D_DEBUG -MTd /Od %cflags% /Zi /GS /RTC1 /FdpcreMTd pcre_*.c
lib /out:pcreMTd.lib pcre_*.obj
REM ren pcremtd.pdb pcremtd.pdbx
REM ren pcremtd.pdbx pcreMTd.pdb

del *.obj
