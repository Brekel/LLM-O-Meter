# Architectural Guardrails

## Project Structure
- **Build System**: CMake (`CMakeLists.txt`).
- **Source files**: Reside in `src/`.
- **UI files** (`.ui`): Reside in `src/ui/`.
- **Build output**: `build_win/` (Windows), `build_mac/` (macOS).
- **Distribution output**: `dist_win/` (Windows), `dist_mac/` (macOS).
- **Build scripts**: `build_win.bat` (Windows), `build_mac` (macOS).
- **Entry Point**: Standard `main.cpp` creating `QApplication` or `QGuiApplication`.

## Logging & Debugging
- **System**: Use Qt's native logging category system.
- **Forbidden**: 
  - Do NOT use `std::cout` or `printf`.

## UI Philosophy
- **Designer First**: Visual elements must be defined in `.ui` files.
- **Logic Separation**: C++ handles logic; `.ui` handles layout.