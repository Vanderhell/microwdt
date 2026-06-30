#include "mwdt.h"

int mtu_a(void)
{
    mwdt_t watchdog = {0};
    return watchdog.initialized ? 1 : 0;
}
