# Design Rationale

## 1. Per-task instead of global watchdog
Hardware watchdogs detect total system freeze. microwdt detects per-task stalls — you know WHICH task hung, not just THAT something hung.

## 2. Three-state model (OK → LATE → STARVED)
LATE = missed one deadline (warning). STARVED = missed max_misses (critical). This gives gradual escalation — log a warning first, reset only if it doesn't recover.

## 3. Kick-based (not timer-callback based)
Tasks report liveness by calling mwdt_kick(). This is simpler and more natural than registering timer callbacks. The task knows when it completed meaningful work.

## 4. Edge-triggered callbacks
Like microhealth: fires only on state transitions. A task stuck at LATE for 100 checks = 1 event, not 100.

## 5. max_misses = 0 for warn-only
Non-critical tasks (display, LED) can miss deadlines without triggering a reset. Set max_misses=0 and they stay LATE forever without escalating to STARVED.

## 6. auto_reset is per-task
A sensor task stalling might just need a warning. An MQTT task stalling might need a full system reset. The decision is per-task, not global.

| Decision | Gains | Costs |
|----------|-------|-------|
| Per-task | Know which task stalled | One kick call per task per cycle |
| Three states | Gradual escalation | Slightly more complex than bool |
| Kick-based | Natural for task loops | Task must remember to kick |
| Edge-triggered | No spam | Misses sub-check oscillations |
| Per-task auto_reset | Granular control | One more config per task |
