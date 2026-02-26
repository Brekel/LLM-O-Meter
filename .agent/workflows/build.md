# Build Workflow

## Windows
Run `build_win.bat` from the project root. It performs all steps automatically:
1. Adds `C:\Qt\6.10.1\mingw_64\bin`, `C:\Qt\Tools\mingw1310_64\bin`, and `C:\Qt\Tools\Ninja` to PATH.
2. Cleans `build_win/` (or deletes it if clean fails).
3. Configures: `cmake -S . -B build_win -GNinja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64`
4. Builds: `cmake --build build_win`
5. Stages output to `dist_win/` and runs `windeployqt6` with `--no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw` arguments.

## macOS
Run `build_mac` from the project root (equivalent script for macOS).
- Build output goes to `build_mac/`.
- Stages output to `dist_mac/` and runs `windeployqt6` with `--no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw` arguments.

## Validation
- If the build succeeds, the project is valid.
- **Note**: Do not attempt to run the executable if targeting Android/iOS; just verify compilation.