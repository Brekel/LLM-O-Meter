# C++ Coding Style
- **Naming**: 
  - Classes: `PascalCase`.
  - Member Variables: `camelCase` (Note: `bLogger` uses legacy `snake_case`; do not refactor existing common code).
- **Braces**: Allman style (braces on new lines).
- **Headers**: `#pragma once`.
- **PCH**: Always `#include "stdafx.h"` at the top of `.cpp` files.

# Formatting & Layout
- **Line Length**: Use a **200 character limit** (or unlimited). Do not wrap lines aggressively.
- **Preference**: Keep function arguments, long conditional statements, and chained method calls on a single line if they fit within 200 characters. 
- **Verticality**: Avoid splitting code into multiple lines unnecessarily; utilize your wide monitor space.
- Follow the rules defined in the .clang-format file at the project root.