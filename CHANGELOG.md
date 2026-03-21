# Changelog

## [1.0.0] — 2026-03-20

### Added

- Per-task software watchdog with configurable deadlines.
- Three-state model: OK → LATE → STARVED.
- Edge-triggered timeout callbacks.
- Configurable max_misses (0 = warn only, never starve).
- Per-task auto_reset option.
- Kick by index or by name.
- Remaining time query.
- Enable/disable per task.
- 33 tests covering all states, transitions, recovery, edge cases.
