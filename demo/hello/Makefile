CC		= cl.exe
CFLAGS		= /nologo
LFLAGS		= /nologo

.SUFFIXES: .c .o .obj

all: hello.exe

.c.obj:
	$(CC) $(CFLAGS) /c /D _CRT_SECURE_NO_WARNINGS $?

hello.exe: hello.obj goodbye.obj
	$(CC) $(LFLAGS) hello.obj goodbye.obj

clean: ; del /F/Q *.obj hello.exe

rt: clean all
