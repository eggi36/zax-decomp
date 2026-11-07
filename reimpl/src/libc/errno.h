#ifndef ZAX_LIBC_ERRNO_H
#define ZAX_LIBC_ERRNO_H
/* See zax_libc.h: errno of the exe's C runtime (per thread, _errno 0x5d1014) */
#include "zax_libc.h"
#define errno (*_errno())
#endif
