#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=============================================="
echo "  Snadder Game - Automated Android APK Build"
echo "=============================================="
echo ""

# Run Setup Script
echo ">>> Running Android Setup and Bootstrap..."
chmod +x "$SCRIPT_DIR/scripts/setup_android.sh"
"$SCRIPT_DIR/scripts/setup_android.sh"

# Run Gradle Build
echo ""
echo ">>> Running Gradle Build..."
cd "$SCRIPT_DIR/android"
chmod +x gradlew
./gradlew assembleDebug

# Verify Output
APK_PATH="$SCRIPT_DIR/android/app/build/outputs/apk/debug/app-debug.apk"
if [ -f "$APK_PATH" ]; then
    echo ""
    echo "=============================================="
    echo "  Build Successful!"
    echo "  APK Location:"
    echo "    $APK_PATH"
    echo "=============================================="
else
    echo "Build failed: APK not found at $APK_PATH" >&2
    exit 1
fi
