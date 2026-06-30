#include "mwdt.h"

static int wrong_timeout(int event)
{
    (void)event;
    return 0;
}

int main(void)
{
    mwdt_timeout_fn fn = wrong_timeout;
    return fn != 0;
}
