// gcc -o expat -g -W -Wall expat.c -lexpat

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "expat.h"

static char *document = "<doc>\n<elem a1='v1' a2='v2' />\n</doc>\n";

static XML_Parser xp;

#if 0
static const char *
getattr(const char **attrs, const char *name)
{
    int i;

    for (i = 0; attrs[i]; i += 2) {
	if (!strcmp(attrs[i], name))
	    return attrs[i + 1];
    }

    return NULL;
}
#endif

static void
enter_elem(void *data, const char *el, const char **attrs)
{
    data = data;
    int i;

    fprintf(stderr, "ENTER %s\n", el);

    for (i = 0; attrs[i]; i += 2)
	fprintf(stderr, "%p=%p stack=%p heap=%p\n",
	    attrs[i], attrs[i+1], &i, malloc(32));
}

static void
leave_elem(void *data, const char *el)
{
    data = data;

    fprintf(stderr, "LEAVE %s\n", el);
}

int
main(void)
{
    fprintf(stderr, "%p <--> %p\n", document, document + strlen(document));

    xp = XML_ParserCreate(NULL);

    XML_SetElementHandler(xp, enter_elem, leave_elem);

    if (!XML_Parse(xp, document, strlen(document), 1)) {
	if (XML_GetErrorCode(xp) == XML_ERROR_ABORTED) {
	    // The parse was ended early; generally a good sign.
	    (void)XML_ParserFree(xp);
	} else {
	    fprintf(stderr, "Parse error at line %ld:\n%s\n",
		    XML_GetCurrentLineNumber(xp),
		    XML_ErrorString(XML_GetErrorCode(xp)));
	}
    }

    fprintf(stderr, "%s", document);

    return 0;
}
