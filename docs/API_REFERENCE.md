# API Reference

> **Header:** `#include "mwdt.h"` · **Version:** 1.0.0

## Task lifecycle
1. `mwdt_register()` — set name, deadline, max_misses, auto_reset
2. `mwdt_kick()` — task reports alive (resets deadline timer)
3. `mwdt_check()` — detect violations (call from main loop)
4. Callback fires on state transitions (LATE, STARVED)

## States: OK → LATE → STARVED
- **OK**: last kick within deadline
- **LATE**: missed 1+ deadlines (miss_count < max_misses)
- **STARVED**: miss_count >= max_misses → optional auto_reset

## Thread safety
Not thread-safe. kick() from task context, check() from one thread.
For ISR→task patterns, use micoring to buffer kicks.
