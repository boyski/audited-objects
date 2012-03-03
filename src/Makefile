####################################################################
## THIS IS NOT THE UNIX MAKEFILE - IT'S FOR USE BY
## MS NMAKE ON WINDOWS. THE UNIX VERSION IS IN 'GNUmakefile'.
# Usage: nmake [Release | Debug]
#      (default is Debug)
####################################################################

## THIS FILE IS NOT NECESSARILY UP TO DATE. Although it worked fine when last
## checked I tend to use the solution file, largely because this forces
## the solution to be kept up to date. The GNUmakefile has a clause to
## support this, so if you have GNU make installed for Windows you can just
## run "make". Whereas if you want to use this file, use "nmake".
## In all cases this must be done in a VC-enabled shell.

## Note: not all the compiler flags here are carefully thought out and
## and analyzed - some were just copied from random other projects.
## There may be room for improvement there.
## In general you should assume flags are present as a result of flailing
## rather than exhaustive research.
## An exception: /MT[d] is used deliberately to avoid complex interactions
## between the CRT of the host program and the auditor.

.SUFFIXES:
.SUFFIXES:	.cpp .c

include App.properties

CRT		= MT
MSCVER		= 8

OS_CPU		= Windows_i386
P		= .$(OS_CPU)

BUILTON		= Windows $(COMPUTERNAME) $(PROCESSOR_ARCHITECTURE)

INSTALL		= C:\Windows

AOTOOL		= $P\ao.exe
LIBAO		= $P\LibAO.dll
AO2MAKE		= pl\ao2make

## Debug
CC		= cl /nologo
LINK		= link /nologo

STATICFLAGS	= /D CURL_STATICLIB /D PCRE_STATIC # /D XML_STATIC
WARNFLAGS	= /W3 /D _CRT_SECURE_NO_DEPRECATE /D _CRT_NONSTDC_NO_DEPRECATE

CFLAGS		= /c /TC\
		  /GF\
		  $(WARNFLAGS)\
		  $(STATICFLAGS)\
		  $(CRYPTOFLAGS)\
		  /D TRIO_REPLACE_STDIO\
		  /D WIN32 /D _WINDOWS /D _CONSOLE\
		  /D PUTIL_BUILTON="\"$(BUILTON)\""\
		  /D APPLICATION_VERSION="\"$(APPLICATION_VERSION)\""\
		  /I ../OPS/$(OS_CPU)/include\
		  /Fo$P/ /Fd$P/AO

CXXFLAGS	= /c /TP\
		  /GF\
		  /EHsc\
		  $(WARNFLAGS)\
		  $(STATICFLAGS)\
		  $(CRYPTOFLAGS)\
		  /D TRIO_REPLACE_STDIO\
		  /D WIN32 /D _WINDOWS /D _CONSOLE\
		  /D APPLICATION_VERSION="\"$(APPLICATION_VERSION)\""\
		  /I ../OPS/$(OS_CPU)/include\
		  /Fo$P/ /Fd$P/AO

#CXXFLAGS	+=  /D _USRDLL /D _WINDLL

!IF "$(CFG)" == "Release"
DBG		=
CFG_CFLAGS	= /$(CRT) /O2 /Gy /D _STATIC_CPPLIB /D NDEBUG
CFG_LFLAGS	= /debug
!ELSE
CFG		= Debug
DBG		= d
CFG_CFLAGS	= /$(CRT)d /Od /Zi /GS- /RTC1
CFG_LFLAGS	= /debug /nodefaultlib:libcmt.lib
!ENDIF

CPCI		= $P\$(CFG)\CPCI.dll

DLL_LIBS	= pcre$(CRT)$(DBG).lib\
		  zlib$(CRT).lib\
		  kazlib$(CRT)$(DBG)$(MSCVER).lib\
		  unstamp$(CRT)$(DBG)$(MSCVER).lib\
		  trio$(CRT)$(DBG).lib\
		  git2$(CRT)$(DBG).lib\
		  advapi32.lib winmm.lib ws2_32.lib

EXE_LIBS	= libcurl$(CRT)$(DBG).lib\
		  cdb$(CRT)$(DBG).lib\
		  $(DLL_LIBS)

LFLAGS		= /machine:X86 /libpath:../OPS/$(OS_CPU)/lib

# This prevents Address Space Layout Randomization (ASLR) from occurring
# on Windows Vista and up. Unfortunately it only affects the current
# executable, not the ones we're injecting into, so it does no good here.
# Kept around for documentation as it's not well documented in MSDN.
#LFLAGS		+= /dynamicbase:no

OBJS	=\
	$P\aotool.obj\
	$P\bsd_getopt.obj\
	$P\ca.obj\
	$P\code.obj\
	$P\down.obj\
	$P\git.obj\
	$P\http.obj\
	$P\make.obj\
	$P\moment.obj\
	$P\mon.obj\
	$P\pa.obj\
	$P\pn.obj\
	$P\prefs.obj\
	$P\prop.obj\
	$P\ps.obj\
	$P\putil.obj\
	$P\re.obj\
	$P\shop.obj\
	$P\tee.obj\
	$P\up.obj\
	$P\util.obj\
	$P\vb.obj\
	$P\win.obj

{.\}.cpp{$P\}.obj:
	$(CC) $(CFG_CFLAGS) $(CXXFLAGS) $<

{.\}.c{$P\}.obj:
	$(CC) $(CFG_CFLAGS) $(CFLAGS) $<
 
install: all
	copy $(AOTOOL)  $(INSTALL)
	copy $(LIBAO)   $(INSTALL)
	copy $(AO2MAKE) $(INSTALL)
	copy $P\$(CFG)\CPCI.dll $(INSTALL)

all: $(AOTOOL) $(LIBAO) $(CPCI)

$P\putil.obj $P\LibWin.obj: Putil\putil.h

$P\aotool.obj $P\http.obj: About\about.c

COMMINCS	=  libcommon.c ca.c code.c moment.c \
		   pa.c pn.c prefs.c prop.c ps.c \
		   re.c util.c vb.c

$P\LibWin.obj: libcommon.c $(COMMINCS)

$(AOTOOL): $(OBJS)
	$(LINK) /out:$(AOTOOL) $(CFG_LFLAGS) /subsystem:console $(LFLAGS) $(OBJS) setargv.obj $(EXE_LIBS)

$(LIBAO): $P\LibWin.obj $P\APIHook.obj
	$(LINK) /out:$P/LibAO.dll $(CFG_LFLAGS) /dll /pdb:$P\LibAO.pdb /subsystem:WINDOWS $(LFLAGS) $P\LibWin.obj $P\APIHook.obj $(DLL_LIBS)

$(CPCI): CPCI\CPCI.vcproj CPCI\cpci.cpp
	msbuild CPCI\CPCI.vcproj /nologo /t:build /p:Configuration=$(CFG)

_relink:
	-del /q $P\ao.exe $P\*.dll

relink: _relink install

clean:
	-del /q $(AOTOOL) $P\*.obj $P\*.dll $P\*.*db $P\*.ilk $P\*.sbr $P\*.manifest

roundtrip rt fi: clean install
