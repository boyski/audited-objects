/* Allow 32 and 64 bit headers to coexist */
#if defined __amd64 || defined __x86_64 || defined __sparcv9 || defined __arch64
#include "curlbuild-64.h"
#else
#include "curlbuild-32.h"
#endif
