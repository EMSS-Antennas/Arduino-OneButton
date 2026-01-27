OneButton library debug

To enable lightweight debug messages for the OneButton library, define the
`__ONEBTN_DEBUG__` macro at build time or before including `OneButton.h`.

Examples:
- platformio.ini (per-environment):

  [env:program_DiamexISP]
  build_flags = -D__ONEBTN_DEBUG__=1

- In a project header (included before library headers):

  #define __ONEBTN_DEBUG__ 1
  #include <OneButton.h>

Notes:
- Debug prints use `F("string")` to keep strings in flash (saves RAM).
- When disabled, debug macros expand to nothing and incur no runtime cost.
