CCS
prop_get_str(CCS prop)
{
    return _prop_getenv(prop);
}

CCS
prop_put_str(CCS prop, CCS newval)
{
    CS oldval = _prop_getenv(prop);
    size_t oldlen = strlen(oldval);
    size_t newlen = strlen(newval);
    assert(newlen <= oldlen);
    (void)strncpy(oldval, newval, oldlen);
    oldval[newlen] = '\0';
    return prop_get_str(prop);
}

long
prop_get_long(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtol(strval, NULL, 0);
}

long
prop_put_long(CCS prop, long newval)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval);
    char newbuf[256];  // way longer than any stringified integral value
    (void)snprintf(newbuf, sizeof(newbuf), "%*ld", (int)strlen(oldval), newval);
    prop_put_str(prop, newbuf);
    return prop_get_long(prop);
}

unsigned long
prop_get_ulong(CCS prop)
{
    CCS strval = prop_get_str(prop);
    return strtoul(strval, NULL, 0);
}

unsigned long
prop_put_ulong(CCS prop, unsigned long newval)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval);
    char newbuf[256];  // way longer than any stringified integral value
    (void)snprintf(newbuf, sizeof(newbuf), "%*lu", (int)strlen(oldval), newval);
    prop_put_str(prop, newbuf);
    return prop_get_ulong(prop);
}

bool
prop_is_true(CCS prop)
{
    CS oldval = _prop_getenv(prop);
    assert(oldval);
    return *oldval && strtoul(oldval, NULL, 0) != 0;
}

// vim: sw=4 ts=4 et tw=100 cc=+1
