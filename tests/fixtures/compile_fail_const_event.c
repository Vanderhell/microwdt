#include "mwdt.h"

static void bad_timeout(const mwdt_timeout_t *event, void *ctx)
{
    (void)ctx;
    event->miss_count = 1U;
}

int main(void)
{
    return 0;
}
