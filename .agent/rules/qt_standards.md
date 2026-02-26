# Qt6 Coding Standards

## CMake Configuration
- **Standard**: Start `CMakeLists.txt` with `cmake_minimum_required(VERSION 3.16)` and `project()`.
- **Qt Modules**: Use `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)`.
- **Auto-Moc**: Ensure `set(CMAKE_AUTOMOC ON)` and `set(CMAKE_AUTOUIC ON)` are set.
- **Resources**: If using images/icons, use strict Qt Resource System (`.qrc`).

## Class Structure (UI Pattern)
- **Header**:
  - Forward declare `namespace Ui { class MyClass; }`
  - Private member: `Ui::MyClass *ui;`
  - Destructor: `delete ui;`
- **Source**:
  - `#include "ui_MyClass.h"`
  - Constructor: `ui->setupUi(this);`

## Naming
- **Variables**: `camelCase`.
- **Classes**: `PascalCase`.