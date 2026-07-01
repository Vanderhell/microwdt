# Security Policy

## Supported scope

`microwdt` is a small C library with no hosted networking surface and no
built-in privilege boundary. Security issues are still relevant when they can
cause memory corruption, undefined behavior, or misleading watchdog state in
consumer applications.

Report issues such as:

- memory safety bugs
- undefined behavior with valid API inputs
- public API contracts that can silently corrupt caller-owned state
- build or packaging issues that can ship the wrong artifacts

## Reporting

Please do not open a public issue first for a suspected security-sensitive bug.

Instead:

1. Open a GitHub security advisory if repository settings allow it.
2. If not, contact the maintainer privately before publishing a public report.

Include:

- affected commit, branch, or tag if known
- compiler and platform
- minimal reproducer
- observed behavior and expected behavior

## Response expectations

The repository does not promise formal SLA timing. Reports are handled as
maintainer time permits.

## Out of scope

- requests for RTOS wrappers or hardware watchdog drivers
- platform-specific reset implementation policy
- bugs in downstream application synchronization around one shared `mwdt_t`
