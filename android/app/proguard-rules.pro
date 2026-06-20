# ProGuard rules for Snadder Game
# ================================
# Since this is a native SDL2 game, there's minimal Java code to optimize.
# These rules ensure the SDL bridge classes are preserved.

# Keep SDLActivity and all SDL Java bridge classes
-keep class org.libsdl.app.** { *; }

# Keep the game's main activity
-keep class com.snadder.game.SnadderActivity { *; }

# Keep JNI methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Don't warn about SDL internal classes
-dontwarn org.libsdl.app.**
