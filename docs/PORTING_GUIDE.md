# Porting Guide

Two files: `mwdt.h` + `mwdt.c`. C99. Provide a clock function.

```cmake
add_library(microwdt STATIC lib/microwdt/src/mwdt.c)
target_include_directories(microwdt PUBLIC lib/microwdt/include)
```

For auto_reset, provide a reset callback: NVIC_SystemReset(), esp_restart(), etc.
