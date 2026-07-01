# Contributing

In scope:

- bug fixes
- tests
- documentation
- build and packaging improvements that keep the runtime dependency-free

Out of scope:

- dynamic allocation
- RTOS wrappers
- hardware watchdog drivers
- scheduler, logging, panic, or recovery frameworks

Before opening a pull request:

1. Run the runtime test suite.
2. Run the CMake build and CTest suite if the change touches public headers, build files, or packaging.
3. Keep the library portable C with caller-owned storage and caller-owned callback contexts.
4. Use GitHub issue templates for bug reports and feature requests when the change starts from a tracked problem statement.
5. Keep README examples, cookbook snippets, and fixtures in sync when public usage changes.
