# Changelog

## Unreleased

### Changed

- Replaced macro-sized embedded task storage with caller-owned storage supplied through `mwdt_config_t`.
- Switched registration, lookup, checks, and queries to explicit status-plus-output APIs.
- Added explicit uninitialized, busy, disabled, reset-latched, and state error results.
- Defined modulo-32-bit clock semantics, exact deadline handling, direct `OK` to `STARVED` transitions, and non-decreasing miss-count behavior until explicit recovery.
- Added same-instance reentrancy blocking during checks and callbacks.
- Added a latched reset-request model with query and clear APIs.
- Added saturating public counters for successful checks, transition events, and reset requests.
- Added compile-success, compile-fail, C++ consumer, and installable CMake package coverage.

### Fixed

- Rejected duplicate task names, empty names, zero deadlines, invalid auto-reset combinations, and uninitialized watchdog use.
- Removed fail-open query behavior for null and invalid inputs.
- Prevented same-instance callback mutation from changing committed task state after an event callback fires.
- Prevented repeated reset callback dispatch after a reset request is latched.

### Documentation

- Removed unsupported release-version claims that were not backed by repository tags.
- Updated README and API documentation to describe ownership, lifetime, callback ordering, thread/ISR limits, and software-watchdog blind spots precisely.
- Clarified that no repository tag currently proves a `1.0.0` release, and that public struct field stability does not make direct state mutation supported.
