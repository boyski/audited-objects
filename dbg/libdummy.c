#include "prop.c"

static unsigned long
_init_auditlib(CCS call, CCS exe, CCS cmdstr)
{
    UNUSED(call);
    UNUSED(exe);
    UNUSED(cmdstr);
    return 0;
}

static void
_audit_end(CCS call, int64_t status)
{
    UNUSED(call);
    UNUSED(status);
}

#define util_is_tmp(dir)				0
#define _pa_record(call, path, extra, fd, op)
#define re_match__(IgnorePathRE, path)			0
#define IgnorePathRE					0
