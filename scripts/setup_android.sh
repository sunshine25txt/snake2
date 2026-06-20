#!/usr/bin/env bash
# ============================================================================
# setup_android.sh — Snadder Game Android Build Setup
# ============================================================================
# This script prepares the Android project for building by:
#   1. Cloning SDL2, SDL2_image, SDL2_ttf, SDL2_mixer from GitHub
#   2. Copying SDLActivity.java and Java bridge files
#   3. Copying C++ game sources into the JNI directory
#   4. Copying assets (fonts, sounds, questions.csv)
#   5. Generating the Gradle wrapper if missing
#   6. Creating local.properties from environment
#
# Usage:
#   chmod +x scripts/setup_android.sh
#   ./scripts/setup_android.sh
#
# Environment variables (optional):
#   ANDROID_SDK_ROOT — Path to Android SDK (auto-detected if not set)
#   ANDROID_NDK_HOME — Path to Android NDK (auto-detected if not set)
# ============================================================================

set -euo pipefail

# --- Configuration ---
SDL2_VERSION="release-2.30.12"
SDL2_IMAGE_VERSION="release-2.8.4"
SDL2_TTF_VERSION="release-2.22.0"
SDL2_MIXER_VERSION="release-2.8.1"

# --- Paths ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ANDROID_DIR="$PROJECT_ROOT/android"
APP_DIR="$ANDROID_DIR/app"
JNI_DIR="$APP_DIR/src/main/jni"
JAVA_DIR="$APP_DIR/src/main/java"
ASSETS_DIR="$APP_DIR/src/main/assets"
RES_DIR="$APP_DIR/src/main/res"
SDL_VENDOR_DIR="$PROJECT_ROOT/android/SDL2-vendor"

echo "=============================================="
echo "  Snadder Game — Android Setup"
echo "=============================================="
echo "Project root: $PROJECT_ROOT"
echo "Android dir:  $ANDROID_DIR"
echo ""

# --- Step 1: Create directory structure ---
echo "[1/7] Creating directory structure..."
mkdir -p "$JNI_DIR/src"
mkdir -p "$JAVA_DIR"
mkdir -p "$ASSETS_DIR/font"
mkdir -p "$ASSETS_DIR/sound"
mkdir -p "$RES_DIR/values"
mkdir -p "$RES_DIR/mipmap-hdpi"
mkdir -p "$RES_DIR/mipmap-mdpi"
mkdir -p "$RES_DIR/mipmap-xhdpi"
mkdir -p "$RES_DIR/mipmap-xxhdpi"
mkdir -p "$SDL_VENDOR_DIR"
echo "  ✓ Directories created"

# --- Step 2: Clone SDL2 repositories ---
echo ""
echo "[2/7] Cloning SDL2 libraries..."

clone_sdl_repo() {
    local repo_url="$1"
    local target_dir="$2"
    local branch="$3"
    local name="$(basename "$target_dir")"

    if [ -d "$target_dir" ] && [ -f "$target_dir/Android.mk" ] || [ -d "$target_dir/android-project" ]; then
        echo "  ✓ $name already exists, skipping clone"
    else
        echo "  → Cloning $name ($branch)..."
        rm -rf "$target_dir"
        git clone --depth 1 --branch "$branch" "$repo_url" "$target_dir" 2>&1 | tail -1
        echo "  ✓ $name cloned"
    fi
}

clone_sdl_repo "https://github.com/libsdl-org/SDL.git" \
    "$SDL_VENDOR_DIR/SDL2" "$SDL2_VERSION"

clone_sdl_repo "https://github.com/libsdl-org/SDL_image.git" \
    "$SDL_VENDOR_DIR/SDL2_image" "$SDL2_IMAGE_VERSION"

clone_sdl_repo "https://github.com/libsdl-org/SDL_ttf.git" \
    "$SDL_VENDOR_DIR/SDL2_ttf" "$SDL2_TTF_VERSION"

clone_sdl_repo "https://github.com/libsdl-org/SDL_mixer.git" \
    "$SDL_VENDOR_DIR/SDL2_mixer" "$SDL2_MIXER_VERSION"

# --- Step 3: Copy SDLActivity.java and Java bridge files ---
echo ""
echo "[3/7] Copying SDL Java bridge files..."

SDL_JAVA_SRC="$SDL_VENDOR_DIR/SDL2/android-project/app/src/main/java/org/libsdl/app"
DEST_JAVA_DIR="$JAVA_DIR/org/libsdl/app"
mkdir -p "$DEST_JAVA_DIR"

if [ -d "$SDL_JAVA_SRC" ]; then
    cp -f "$SDL_JAVA_SRC"/SDLActivity.java "$DEST_JAVA_DIR/" 2>/dev/null || true
    cp -f "$SDL_JAVA_SRC"/SDLAudioManager.java "$DEST_JAVA_DIR/" 2>/dev/null || true
    cp -f "$SDL_JAVA_SRC"/SDLControllerManager.java "$DEST_JAVA_DIR/" 2>/dev/null || true
    cp -f "$SDL_JAVA_SRC"/HIDDevice*.java "$DEST_JAVA_DIR/" 2>/dev/null || true
    cp -f "$SDL_JAVA_SRC"/SDL.java "$DEST_JAVA_DIR/" 2>/dev/null || true
    cp -f "$SDL_JAVA_SRC"/SDLDummyEdit.java "$DEST_JAVA_DIR/" 2>/dev/null || true
    cp -f "$SDL_JAVA_SRC"/SDLSurface.java "$DEST_JAVA_DIR/" 2>/dev/null || true

    # Count copied files
    JAVA_COUNT=$(find "$DEST_JAVA_DIR" -name "*.java" | wc -l)
    echo "  ✓ Copied $JAVA_COUNT Java bridge files"
else
    echo "  ✗ ERROR: SDL Java source not found at $SDL_JAVA_SRC"
    echo "    Expected after cloning SDL2. Check the clone step above."
    exit 1
fi

# --- Step 4: Create the game's main Activity ---
echo ""
echo "[4/7] Creating SnadderActivity.java..."

GAME_JAVA_DIR="$JAVA_DIR/com/snadder/game"
mkdir -p "$GAME_JAVA_DIR"

cat > "$GAME_JAVA_DIR/SnadderActivity.java" << 'JAVA_EOF'
package com.snadder.game;

import org.libsdl.app.SDLActivity;

/**
 * SnadderActivity — Main entry point for the Snadder Game on Android.
 * Extends SDLActivity which handles the SDL lifecycle, EGL surface,
 * and JNI bridge to the native C++ code.
 */
public class SnadderActivity extends SDLActivity {

    /**
     * Returns the name of the native shared library.
     * This must match LOCAL_MODULE in Android.mk (which produces libmain.so).
     */
    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "SDL2_image",
            "SDL2_ttf",
            "SDL2_mixer",
            "main"
        };
    }
}
JAVA_EOF
echo "  ✓ SnadderActivity.java created"

# --- Step 5: Copy C++ sources ---
echo ""
echo "[5/7] Copying C++ game sources..."

cp -f "$PROJECT_ROOT/75.cpp" "$JNI_DIR/src/75.cpp"
cp -f "$PROJECT_ROOT/features.hpp" "$JNI_DIR/src/features.hpp"

# Also copy the SDL2 include headers that the game depends on
# (the NDK build will use SDL2's own headers, but we keep inc/ as fallback)
echo "  ✓ Copied 75.cpp and features.hpp"

# --- Step 6: Copy assets ---
echo ""
echo "[6/7] Copying game assets..."

# Fonts
if [ -d "$PROJECT_ROOT/assets/font" ]; then
    cp -f "$PROJECT_ROOT/assets/font"/*.ttf "$ASSETS_DIR/font/" 2>/dev/null || true
    FONT_COUNT=$(find "$ASSETS_DIR/font" -name "*.ttf" | wc -l)
    echo "  ✓ Copied $FONT_COUNT font files"
else
    echo "  ⚠ Warning: assets/font/ not found"
fi

# Sounds
if [ -d "$PROJECT_ROOT/assets/sound" ]; then
    cp -f "$PROJECT_ROOT/assets/sound"/*.wav "$ASSETS_DIR/sound/" 2>/dev/null || true
    SOUND_COUNT=$(find "$ASSETS_DIR/sound" -name "*.wav" | wc -l)
    echo "  ✓ Copied $SOUND_COUNT sound files"
else
    echo "  ⚠ Warning: assets/sound/ not found"
fi

# Questions CSV
if [ -f "$PROJECT_ROOT/questions.csv" ]; then
    cp -f "$PROJECT_ROOT/questions.csv" "$ASSETS_DIR/questions.csv"
    echo "  ✓ Copied questions.csv"
else
    echo "  ⚠ Warning: questions.csv not found (game will use fallback questions)"
fi

# --- Step 7: Generate Gradle wrapper ---
echo ""
echo "[7/7] Setting up Gradle wrapper..."

WRAPPER_DIR="$ANDROID_DIR/gradle/wrapper"
mkdir -p "$WRAPPER_DIR"

if [ ! -f "$ANDROID_DIR/gradlew" ]; then
    # Create gradle-wrapper.properties
    cat > "$WRAPPER_DIR/gradle-wrapper.properties" << 'WRAPPER_PROPS'
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-8.10.2-bin.zip
networkTimeout=10000
validateDistributionUrl=true
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
WRAPPER_PROPS

    # Download gradle-wrapper.jar if not present
    if [ ! -f "$WRAPPER_DIR/gradle-wrapper.jar" ]; then
        echo "  → Downloading gradle-wrapper.jar..."
        GRADLE_WRAPPER_URL="https://raw.githubusercontent.com/gradle/gradle/v8.10.2/gradle/wrapper/gradle-wrapper.jar"
        if command -v curl &>/dev/null; then
            curl -sL "$GRADLE_WRAPPER_URL" -o "$WRAPPER_DIR/gradle-wrapper.jar"
        elif command -v wget &>/dev/null; then
            wget -q "$GRADLE_WRAPPER_URL" -O "$WRAPPER_DIR/gradle-wrapper.jar"
        else
            echo "  ⚠ Neither curl nor wget available. Trying alternative method..."
            # Alternative: use Gradle init to generate wrapper
            if command -v gradle &>/dev/null; then
                (cd "$ANDROID_DIR" && gradle wrapper --gradle-version 8.10.2)
            else
                echo "  ✗ ERROR: Cannot download gradle-wrapper.jar"
                echo "    Install curl, wget, or gradle, then re-run this script."
                exit 1
            fi
        fi
    fi

    # Create gradlew script
    cat > "$ANDROID_DIR/gradlew" << 'GRADLEW_EOF'
#!/bin/sh

#
# Gradle start up script for POSIX generated by setup_android.sh
#

# Attempt to set APP_HOME
# Resolve links: $0 may be a link
app_path=$0
while
    APP_HOME=${app_path%"${app_path##*/}"}  # leaves a trailing /; empty if no leading path
    [ -h "$app_path" ]
do
    ls=$( ls -ld -- "$app_path" )
    link=${ls#*' -> '}
    case $link in
      /*)   app_path=$link ;;
      *)    app_path=$APP_HOME$link ;;
    esac
done

APP_HOME=$( cd "${APP_HOME:-./}" > /dev/null && pwd -P ) || exit

# Use the maximum available, or set MAX_FD != -1 to use that value.
MAX_FD=maximum

warn () {
    echo "$*"
} >&2

die () {
    echo
    echo "$*"
    echo
    exit 1
} >&2

# OS specific support
cygwin=false
darwin=false
nonstop=false
case "$( uname )" in
  CYGWIN* )         cygwin=true  ;;
  Darwin* )         darwin=true  ;;
  NONSTOP* )        nonstop=true ;;
esac

CLASSPATH=$APP_HOME/gradle/wrapper/gradle-wrapper.jar

# Determine the Java command to use to start the JVM.
if [ -n "$JAVA_HOME" ] ; then
    if [ -x "$JAVA_HOME/jre/sh/java" ] ; then
        JAVACMD=$JAVA_HOME/jre/sh/java
    else
        JAVACMD=$JAVA_HOME/bin/java
    fi
    if [ ! -x "$JAVACMD" ] ; then
        die "ERROR: JAVA_HOME is set to an invalid directory: $JAVA_HOME"
    fi
else
    JAVACMD=java
    if ! command -v java >/dev/null 2>&1 ; then
        die "ERROR: JAVA_HOME is not set and no 'java' command could be found in your PATH."
    fi
fi

# Increase the maximum file descriptors if we can.
if ! "$cygwin" && ! "$darwin" && ! "$nonstop" ; then
    case $MAX_FD in
      max*)
        MAX_FD=$( ulimit -H -n ) ||
            warn "Could not query maximum file descriptor limit"
      ;;
    esac
    case $MAX_FD in
      '' | soft) :;;
      *)
        ulimit -n "$MAX_FD" ||
            warn "Could not set maximum file descriptor limit to $MAX_FD"
      ;;
    esac
fi

# Collect all arguments for the java command, stracks://gnu.org/Argument-Syntax
DEFAULT_JVM_OPTS='"-Xmx64m" "-Xms64m"'

# Collect all arguments for the java command
set -- \
    -classpath "$CLASSPATH" \
    org.gradle.wrapper.GradleWrapperMain \
    "$@"

# Use "exec" only on non-cygwin systems, as cygwin has a known issue with exec.
if "$cygwin" ; then
    eval "set -- $DEFAULT_JVM_OPTS $JAVA_OPTS $GRADLE_OPTS"
    "$JAVACMD" "$@"
else
    eval "set -- $DEFAULT_JVM_OPTS $JAVA_OPTS $GRADLE_OPTS"
    exec "$JAVACMD" "$@"
fi
GRADLEW_EOF
    chmod +x "$ANDROID_DIR/gradlew"

    # Create gradlew.bat for Windows
    cat > "$ANDROID_DIR/gradlew.bat" << 'GRADLEW_BAT_EOF'
@rem
@rem Gradle startup script for Windows
@rem

@if "%DEBUG%"=="" @echo off

@rem Set local scope for the variables with windows NT shell
if "%OS%"=="Windows_NT" setlocal

set DIRNAME=%~dp0
if "%DIRNAME%"=="" set DIRNAME=.
@rem This is normally unused
set APP_BASE_NAME=%~n0
set APP_HOME=%DIRNAME%

@rem Resolve any "." and ".." in APP_HOME to make it shorter.
for %%i in ("%APP_HOME%") do set APP_HOME=%%~fi

@rem Add default JVM options here.
set DEFAULT_JVM_OPTS="-Xmx64m" "-Xms64m"

@rem Find java.exe
if defined JAVA_HOME goto findJavaFromJavaHome

set JAVA_EXE=java.exe
%JAVA_EXE% -version >NUL 2>&1
if %ERRORLEVEL% equ 0 goto execute

echo. 1>&2
echo ERROR: JAVA_HOME is not set and no 'java' command could be found in your PATH. 1>&2
goto fail

:findJavaFromJavaHome
set JAVA_HOME=%JAVA_HOME:"=%
set JAVA_EXE=%JAVA_HOME%/bin/java.exe

if exist "%JAVA_EXE%" goto execute

echo. 1>&2
echo ERROR: JAVA_HOME is set to an invalid directory: %JAVA_HOME% 1>&2
goto fail

:execute
@rem Setup the command line
set CLASSPATH=%APP_HOME%\gradle\wrapper\gradle-wrapper.jar

@rem Execute Gradle
"%JAVA_EXE%" %DEFAULT_JVM_OPTS% %JAVA_OPTS% %GRADLE_OPTS% "-Dorg.gradle.appname=%APP_BASE_NAME%" -classpath "%CLASSPATH%" org.gradle.wrapper.GradleWrapperMain %*

:end
@rem End local scope for the variables with windows NT shell
if %OS%==Windows_NT endlocal

:omega
GRADLEW_BAT_EOF
    echo "  ✓ Gradle wrapper created"
else
    echo "  ✓ Gradle wrapper already exists"
fi

# --- Create local.properties ---
echo ""
echo "Setting up local.properties..."

if [ ! -f "$ANDROID_DIR/local.properties" ]; then
    SDK_PATH=""
    # Try to auto-detect SDK path
    if [ -n "${ANDROID_SDK_ROOT:-}" ] && [ -d "$ANDROID_SDK_ROOT" ]; then
        SDK_PATH="$ANDROID_SDK_ROOT"
    elif [ -n "${ANDROID_HOME:-}" ] && [ -d "$ANDROID_HOME" ]; then
        SDK_PATH="$ANDROID_HOME"
    elif [ -d "$HOME/Android/Sdk" ]; then
        SDK_PATH="$HOME/Android/Sdk"
    elif [ -d "/usr/local/lib/android/sdk" ]; then
        SDK_PATH="/usr/local/lib/android/sdk"
    elif [ -d "$ANDROID_DIR/android-sdk" ]; then
        SDK_PATH="$ANDROID_DIR/android-sdk"
    fi

    if [ -z "$SDK_PATH" ]; then
        echo "  → Android SDK not found. Bootstrapping a local Android SDK in $ANDROID_DIR/android-sdk..."
        SDK_PATH="$ANDROID_DIR/android-sdk"
        SDK_TOOLS_DIR="$SDK_PATH/cmdline-tools"
        
        if [ ! -f "$SDK_TOOLS_DIR/latest/bin/sdkmanager" ]; then
            echo "  → Downloading Android cmdline-tools..."
            
            # Detect platform
            if [[ "$OSTYPE" == "darwin"* ]]; then
                SDK_URL="https://dl.google.com/android/repository/commandlinetools-mac-11076708_latest.zip"
            else
                SDK_URL="https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip"
            fi
            
            ZIP_FILE="$ANDROID_DIR/cmdline-tools.zip"
            if command -v curl &>/dev/null; then
                curl -sL "$SDK_URL" -o "$ZIP_FILE"
            elif command -v wget &>/dev/null; then
                wget -q "$SDK_URL" -O "$ZIP_FILE"
            else
                echo "  ✗ ERROR: Neither curl nor wget found. Cannot download commandline-tools."
                exit 1
            fi
            
            echo "  → Extracting cmdline-tools..."
            TEMP_EXTRACT="$ANDROID_DIR/temp_sdk_extract"
            rm -rf "$TEMP_EXTRACT"
            mkdir -p "$TEMP_EXTRACT"
            unzip -q "$ZIP_FILE" -d "$TEMP_EXTRACT"
            
            mkdir -p "$SDK_TOOLS_DIR/latest"
            mv "$TEMP_EXTRACT/cmdline-tools"/* "$SDK_TOOLS_DIR/latest/"
            
            rm -f "$ZIP_FILE"
            rm -rf "$TEMP_EXTRACT"
            echo "  ✓ cmdline-tools installed"
        fi
        
        SDK_MANAGER="$SDK_TOOLS_DIR/latest/bin/sdkmanager"
        if [ -f "$SDK_MANAGER" ]; then
            chmod +x "$SDK_MANAGER"
            echo "  → Accepting Android SDK licenses..."
            yes | "$SDK_MANAGER" --licenses > /dev/null 2>&1 || true
            echo "  ✓ Licenses accepted"
            
            echo "  → Installing SDK packages (platform-tools, build-tools 35.0.0, platforms;android-35, ndk;27.2.12479018)..."
            echo "     This download is ~1.5 GB and can take several minutes."
            "$SDK_MANAGER" --install "platform-tools" "build-tools;35.0.0" "platforms;android-35" "ndk;27.2.12479018" > /dev/null
            echo "  ✓ SDK packages and NDK installed"
        else
            echo "  ✗ ERROR: Failed to find sdkmanager at $SDK_MANAGER"
            exit 1
        fi
    fi

    if [ -n "$SDK_PATH" ]; then
        # Escape backslashes for properties file format
        SDK_PATH_ESCAPED=$(echo "$SDK_PATH" | sed 's/\\/\\\\/g')
        cat > "$ANDROID_DIR/local.properties" << EOF
## This file is automatically generated by setup_android.sh
## Do NOT commit this file to version control.
sdk.dir=$SDK_PATH_ESCAPED
EOF
        echo "  ✓ local.properties created (sdk.dir=$SDK_PATH)"
    fi
else
    echo "  ✓ local.properties already exists"
fi

# --- Summary ---
echo ""
echo "=============================================="
echo "  Setup Complete!"
echo "=============================================="
echo ""
echo "SDL2 vendor directory: $SDL_VENDOR_DIR"
echo ""
echo "To build:"
echo "  cd $ANDROID_DIR"
echo "  ./gradlew assembleDebug"
echo ""
echo "APK output:"
echo "  $APP_DIR/build/outputs/apk/debug/app-debug.apk"
echo ""
