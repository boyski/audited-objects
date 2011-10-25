// gcc -o requote -W -Wall -g requote.c

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static char *
requote(char **argv)
{
    char *word, *cmdline, *ptr;
    int quote, argc;
    size_t cmdlen;

    // Figure out the worst-case scenario for how much space the cmdline
    // will need and allocate that much. This worst case is when each
    // char needs an escape, plus quotes, plus the trailing space or null.
    for (cmdlen =  0, argc = 0; argv[argc]; argc++) {
	cmdlen += (strlen(argv[argc]) * 2) + 3;
    }
    cmdline = malloc(cmdlen);

    // There are 4 viable POSIX quoting strategies: (1) use only
    // backslashes (no quotes), (2) use double quotes and escape
    // as needed, (3) use single quotes and special-case
    // embedded single quotes, (4) use a hybrid strategy of examining
    // the word and choosing the simplest of single, double, or none.
    // The following represents a hybrid approach.

    for (ptr = cmdline; (word = *argv); argv++) {

	// If the word needs quoting at all, use single quotes unless
	// it _contains_ a single quote, in which case use double
	// quotes and escape special chars.
	if (*word) {
	    char *p;

	    for (quote = 0, p = word; *p; p++) {
		if (!isalnum((int)*p) && !strchr("!%+,-./=:@_", *p)) {
		    if (*p == '\'') {
			quote = '"';
			break;
		    } else {
			quote = '\'';
		    }
		}
	    }
	} else {
	    quote = '"';
	}

	// Add opening quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Copy the word into the output buffer with appropriate escapes
	// if using double-quotes.
	for (; *word; word++) {
	    if (quote == '"') {
		if (*word == '\\') {
		    if (*(word + 1) == '\n') {
			continue;
		    } else {
			*ptr++ = '\\';
		    }
		} else if (strchr("$\"\n`", *word)) {
		    *ptr++ = '\\';
		}
	    }

	    *ptr++ = *word;
	}

	// Add closing quote if required.
	if (quote) {
	    *ptr++ = quote;
	}

	// Append space or terminating null.
	if (argv[1]) {
	    *ptr++ = ' ';
	} else {
	    *ptr = '\0';
	}
    }

    // Trim to fit
    cmdline = realloc(cmdline, strlen(cmdline) + 1);

    return cmdline;
}

int
main(int argc, char **argv)
{
    char *cmd;
    int rc;

    argv[0] = "echo";
    cmd = requote(argv);

    fprintf(stderr, "==[%d]%s==\n", argc, cmd);

    rc = system(cmd);

    free(cmd);

    return rc;
}
