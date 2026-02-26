# LLM-O-Meter — Claude Code Instructions

## Project Structure

- Source files: `src/`
- UI files (`.ui`): `src/ui/`
- Build output: `build_win/` (Windows), `build_mac/` (macOS)
- Distribution output: `dist_win/` (Windows), `dist_mac/` (macOS)
- Build scripts: `build_win.bat` (Windows), `build_mac` (macOS)
- Entry point: `src/main.cpp`

## Build

### Windows
Run `build_win.bat` from the project root. It handles everything automatically:
1. Adds `C:\Qt\6.10.1\mingw_64\bin`, `C:\Qt\Tools\mingw1310_64\bin`, and `C:\Qt\Tools\Ninja` to PATH.
2. Cleans `build_win/` (deletes it entirely if clean fails).
3. Configures: `cmake -S . -B build_win -GNinja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64`
4. Builds: `cmake --build build_win`
5. Stages output to `dist_win/` and runs `windeployqt6 --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw`.

### macOS
Run `build_mac` from the project root. Build output goes to `build_mac/`, distribution to `dist_mac/`.

### Validation
A successful build is the definition of correctness. Do not attempt to run the executable when targeting Android/iOS — compilation alone is sufficient.

## Environment

- **Qt**: 6.10.1
- **Compiler**: MinGW 13.1.0 64-bit — `C:\Qt\Tools\mingw1310_64`
- **Qt path**: `C:\Qt\6.10.1\mingw_64`
- **Build tool**: Ninja (`C:\Qt\Tools\Ninja`)

## Cross-Platform Rules

Target platforms: Windows, Android, iOS, macOS.

- **NEVER** use platform-specific headers (`<windows.h>`, `<unistd.h>`, etc.) outside of an `#ifdef Q_OS_WIN` / `#ifdef Q_OS_MAC` guard.
- Always prefer Qt abstractions: `QFile` over `fstream`, `QThread` over `CreateThread`, etc.

## CMake Standards

- Minimum version: `cmake_minimum_required(VERSION 3.16)`
- Qt modules: `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)`
- Always set `CMAKE_AUTOMOC ON`, `CMAKE_AUTOUIC ON`, `CMAKE_AUTORCC ON`
- UI files live in `src/ui/`; set `CMAKE_AUTOUIC_SEARCH_PATHS` accordingly
- Resources (images/icons) use the Qt Resource System (`.qrc`)

## Qt UI Pattern

**Header:**
```cpp
namespace Ui { class MyClass; }

class MyClass : public QWidget {
    Q_OBJECT
    ...
    Ui::MyClass *ui;  // private
    ~MyClass() { delete ui; }
};
```

**Source:**
```cpp
#include "ui_MyClass.h"
// In constructor:
ui->setupUi(this);
```

## C++ Style

- **Classes**: `PascalCase`
- **Member variables**: `camelCase`
- **Braces**: Allman style (opening brace on its own line)
- **Headers**: `#pragma once`
- **PCH**: Every `.cpp` file must begin with `#include "stdafx.h"`
- **Line length**: 200 characters — do not wrap lines aggressively; keep arguments and chained calls on one line where they fit
- **Formatting**: Follow `.clang-format` at the project root

## Logging

Use Qt's logging macros exclusively:
```cpp
qDebug()   << "...";
qInfo()    << "...";
qWarning() << "...";
```
Never use `std::cout` or `printf`.
