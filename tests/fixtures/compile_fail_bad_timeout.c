#include "mwdt.h"

static void wrong_timeout(mwdt_timeout_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
}

int main(void)
{
    mwdt_timeout_fn fn = wrong_timeout;
    return fn != 0;
}
