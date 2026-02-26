# Qt6 Environment
- **Qt Version**: 6.10.1
- **Compiler**: MinGW 13.1.0 64-bit (`C:\Qt\Tools\mingw1310_64`)
- **Qt Path**: `C:\Qt\6.10.1\mingw_64`
- **Tools**:
  - `cmake`
  - `ninja` (`C:\Qt\Tools\Ninja`)

# Cross-Platform Mandate
- **Target Platforms**: Windows, Android, iOS, MacOS.
- **Constraints**: 
  - **ABSOLUTELY NO** platform-specific headers (e.g., `<windows.h>`, `<unistd.h>`) unless wrapped in `#ifdef Q_OS_WIN` etc.
  - Rely entirely on Qt abstractions (e.g., use `QFile` not `fstream`, `QThread` not `CreateThread`).