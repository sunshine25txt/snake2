# ==============================================================================
# Android.mk — Master NDK build file for Snadder Game
# ==============================================================================
#
# Module load order is CRITICAL. SDL2 must be defined before any SDL2_* modules,
# and all SDL2_* modules must be defined before the game's "main" module.
#
# Build graph:
#   SDL2 → SDL2_image → SDL2_ttf → SDL2_mixer → main (75.cpp)
#
# ==============================================================================

LOCAL_PATH := $(call my-dir)

# ------------------------------------------------------------------------------
# Point NDK_MODULE_PATH to the SDL2 vendor directory so that
# $(call import-module, ...) can find each library's Android.mk.
# ------------------------------------------------------------------------------
SDL_VENDOR_PATH := $(LOCAL_PATH)/../../../../SDL2-vendor

# ==============================================================================
# Module: main (the game itself)
# ==============================================================================
include $(CLEAR_VARS)

LOCAL_MODULE := main

# Game source files
LOCAL_SRC_FILES := src/75.cpp

# Include paths for SDL2 and all extensions
LOCAL_C_INCLUDES := \
    $(SDL_VENDOR_PATH)/SDL2/include \
    $(SDL_VENDOR_PATH)/SDL2_image/include \
    $(SDL_VENDOR_PATH)/SDL2_ttf/include \
    $(SDL_VENDOR_PATH)/SDL2_mixer/include

# Link against SDL2 shared libraries
LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_ttf SDL2_mixer

# Android system libraries needed by SDL2
LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

# C++ flags
LOCAL_CPPFLAGS := -std=c++17 -frtti -fexceptions -Wall -Wextra

# Optimization for release (ndk-build handles debug/release via APP_OPTIM)
LOCAL_CFLAGS := -O2

include $(BUILD_SHARED_LIBRARY)

# ==============================================================================
# Import SDL2 modules (ORDER MATTERS — dependencies first)
# ==============================================================================
# The import-module calls must come AFTER all LOCAL module definitions.
# Each import reads the library's own Android.mk which defines its module.
# ==============================================================================

$(call import-add-path, $(SDL_VENDOR_PATH))

# 1. SDL2 core (no dependencies)
$(call import-module, SDL2)

# 2. SDL2_image (depends on SDL2)
$(call import-module, SDL2_image)

# 3. SDL2_ttf (depends on SDL2)
$(call import-module, SDL2_ttf)

# 4. SDL2_mixer (depends on SDL2)
$(call import-module, SDL2_mixer)
