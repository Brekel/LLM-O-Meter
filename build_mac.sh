#!/bin/bash
set -e

QT_DIR="/Users/brekel/Qt/6.10.1/macos"
export PATH="$QT_DIR/bin:$PATH"

echo "Closing app if running..."
pkill -f "LLM-O-Meter" 2>/dev/null || true

echo "Cleaning build_mac folder..."
if [ -d build_mac ]; then
	cmake --build build_mac --target clean 2>&1 || {
		echo "Clean failed, removing build_mac folder entirely..."
		rm -rf build_mac
	}
fi

cmake -S . -B build_mac -GNinja -DCMAKE_PREFIX_PATH="$QT_DIR" 2>&1
if [ $? -ne 0 ]; then
	echo "CMAKE CONFIGURE FAILED"
	exit 1
fi

cmake --build build_mac 2>&1
if [ $? -ne 0 ]; then
	echo "BUILD FAILED"
	exit 1
fi

echo "Staging dist_mac folder..."
rm -rf dist_mac
mkdir -p dist_mac
cp -R build_mac/LLM-O-Meter.app dist_mac/LLM-O-Meter.app

echo "Running macdeployqt6..."
macdeployqt6 dist_mac/LLM-O-Meter.app 2>&1
if [ $? -ne 0 ]; then
	echo "MACDEPLOYQT6 FAILED"
	exit 1
fi

echo "Creating dist_mac/LLM-O-Meter.dmg..."
rm -f dist_mac/LLM-O-Meter.dmg
create-dmg \
	--volname "LLM-O-Meter" \
	--window-pos 200 120 \
	--window-size 500 320 \
	--icon-size 128 \
	--icon "LLM-O-Meter.app" 130 160 \
	--app-drop-link 370 160 \
	"dist_mac/LLM-O-Meter.dmg" \
	"dist_mac/LLM-O-Meter.app"
if [ $? -ne 0 ]; then
	echo "DMG CREATION FAILED"
	exit 1
fi

echo "Done. Output is in dist_mac/ and dist_mac/LLM-O-Meter.dmg"

echo "Launching LLM-O-Meter..."
open dist_mac/LLM-O-Meter.app
