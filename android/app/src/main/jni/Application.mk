# ==============================================================================
# Application.mk — NDK application-level configuration for Snadder Game
# ==============================================================================

# Target ABIs
APP_ABI := arm64-v8a armeabi-v7a

# C++ Standard Library (shared, required by SDL2)
APP_STL := c++_shared

# C++ compiler flags
APP_CPPFLAGS := -std=c++17 -frtti -fexceptions

# Minimum Android API level
APP_PLATFORM := android-21

# Build optimization level (debug or release)
# This is overridden by Gradle based on build type
# APP_OPTIM := release

# Enable short commands to avoid command-line-too-long errors on Windows
APP_SHORT_COMMANDS := true
