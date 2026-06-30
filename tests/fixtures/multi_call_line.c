#include "mwdt.h"

int main(void)
{
    mwdt_t watchdog = {0}; uint32_t a = 0U, b = 0U; (void)mwdt_get_check_count(&watchdog, &a); (void)mwdt_get_transition_event_count(&watchdog, &b);
    return 0;
}
