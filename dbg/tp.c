// gcc -o tp -W -Wall -g tp.c

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

int
main(int argc, char *argv[])
{
    int fd;
    int rc = 0;
    int style;
    long generation;
    char N[8];
    char buf[PATH_MAX];
    char *vpid;

    if (!(vpid = getenv("TP_VPID"))) {
	vpid = "0";
    }
    snprintf(buf, sizeof(buf), "TP_VPID=%ld", atol(vpid) + 1);
    putenv(strdup(buf));

    fprintf(stderr, "In main() [%ld=%s] %s %s %s\n",
	(long)getpid(), vpid, argv[0], argv[1], argv[2]);

    if (argc != 3 || !(generation = atol(argv[2]))) {
	fprintf(stderr, "Usage: %s [-[EF]] <N>\t", argv[0]);
	fprintf(stderr, "(where <N> is an integer)\n");
	exit(1);
    }

    // A file read by every command
    fd = open("/etc/passwd", O_RDONLY);
    close(fd);

    style = argv[1][1];

    // A different file is read by each command.
    snprintf(buf, sizeof(buf), "R%s", vpid);
    if ((fd = open(buf, O_RDONLY)) < 0) {
	fprintf(stderr, "Error: %s: %s\n", buf, strerror(errno));
	exit(2);
    }

    // And a different file is written by each command.
    char written[PATH_MAX];
    snprintf(written, sizeof(written), "W%s", vpid);
    fd = creat(written, 0644);
    write(fd, written, bytelen(written));
    write(fd, "\n", 1);
    close(fd);

    if (--generation) {
	snprintf(N, sizeof(N), "%ld", generation);
	argv[2] = N;
    } else {
	exit(0);
    }

    usleep(200000);

    if (style == 'F') {
	pid_t pid;

	if ((pid = fork()) == 0) {
	    fprintf(stderr, "Fork %s %s %s ...\n", argv[0], argv[1], argv[2]);
	    execvp(argv[0], argv);
	    _exit(6);
	} else if (pid > 0) {
	    /* parent process */
	    wait(&rc);
	    rc = rc >> 8;
	}
    } else {
	fprintf(stderr, "Exec %s %s %s ...\n", argv[0], argv[1], argv[2]);
	execvp(argv[0], argv);
    }

    return rc;
}
