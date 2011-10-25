REM @echo off

REM Usage: cmd /c "build-for-ao.bat <curl-dir> <zlib-dir>"
REM The <curl-dir> and <zlib-dir> areas must be fresh from the can!!!!

for /f %%i in ('cd') do set BASEDIR=%%i
set ZLIB_PATH=%BASEDIR%\%2%

cd %1\lib

REM Overriding -MD to -MT and setting the PDB name.
REM Check %ZLIB_PATH%\win32\Makefile.msc to see if default CFLAGS have changed.
cmd /c "cd %ZLIB_PATH% && nmake /f win32\Makefile.msc CFLAGS=^"-nologo -MT -W3 -O2 -Oy- -Zi -FdzlibMTd^" STATICLIB=zlibMT.lib"

REM We either add c-ares *or* turn off the threaded resolver.
REM See Daniel Stenberg response 1/1/08.

nmake -f Makefile.vc9 LIB_NAME=libcurlMT LIB_NAME_DEBUG=libcurlMTd RTLIBCFG=static ZLIB_PATH=%ZLIB_PATH% ZLIBLIBS=zlibMT.lib CCDEBUG="cl.exe /Od /Gm /Zi /D_DEBUG /RTC1 /FdlibcurlMTd" CFLAGS="/I. /I../include /nologo /W3 /EHsc /DWIN32 /FD /c /DBUILDING_LIBCURL /DCURL_DISABLE_LDAP /U USE_THREADING_GETHOSTBYNAME" CFG=release-zlib

nmake -f Makefile.vc9 LIB_NAME=libcurlMT LIB_NAME_DEBUG=libcurlMTd RTLIBCFG=static ZLIB_PATH=%ZLIB_PATH% ZLIBLIBS=zlibMT.lib CCDEBUG="cl.exe /Od /Gm /Zi /D_DEBUG /RTC1 /FdlibcurlMTd" CFLAGS="/I. /I../include /nologo /W3 /EHsc /DWIN32 /FD /c /DBUILDING_LIBCURL /DCURL_DISABLE_LDAP /U USE_THREADING_GETHOSTBYNAME" CFG=debug-zlib
