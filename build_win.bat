@echo off
set PATH=C:\Qt\6.10.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;%PATH%

echo Closing app if running...
taskkill /f /im LLM-O-Meter.exe >nul 2>&1

echo Cleaning build_win folder...
if exist build_win (
    cmake --build build_win --target clean 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo Clean failed, removing build_win folder entirely...
        rmdir /s /q build_win
    )
)

cmake -S . -B build_win -GNinja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo CMAKE CONFIGURE FAILED
    exit /b %ERRORLEVEL%
)

cmake --build build_win 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo BUILD FAILED
    exit /b %ERRORLEVEL%
)

echo Staging dist_win folder...
if exist dist_win rmdir /s /q dist_win
mkdir dist_win
copy /y build_win\LLM-O-Meter.exe dist_win\LLM-O-Meter.exe >nul

echo Running windeployqt6...
windeployqt6 --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw dist_win\LLM-O-Meter.exe 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WINDEPLOYQT6 FAILED
    exit /b %ERRORLEVEL%
)

echo Done. Output is in dist_win\

echo Launching LLM-O-Meter...
start "" dist_win\LLM-O-Meter.exe
