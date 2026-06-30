#include "mwdt.h"

static void wrong_reset(void *ctx)
{
    (void)ctx;
}

int main(void)
{
    mwdt_reset_fn fn = wrong_reset;
    return fn != 0;
}
