# ============================================================================
# setup_android.ps1 - Snadder Game Android Build Setup (Windows/PowerShell)
# ============================================================================
# This script prepares the Android project for building by:
#   1. Downloading/cloning SDL2, SDL2_image, SDL2_ttf, SDL2_mixer from GitHub
#   2. Copying SDLActivity.java and Java bridge files
#   3. Copying C++ game sources into the JNI directory
#   4. Copying assets (fonts, sounds, questions.csv)
#   5. Setting up the Gradle wrapper if missing
#   6. Creating local.properties from environment/defaults
# ============================================================================

$ErrorActionPreference = "Stop"

# --- Configuration ---
$SDL2_VERSION = "release-2.30.12"
$SDL2_IMAGE_VERSION = "release-2.8.4"
$SDL2_TTF_VERSION = "release-2.22.0"
$SDL2_MIXER_VERSION = "release-2.8.1"

# --- Paths ---
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_ROOT = Resolve-Path "$SCRIPT_DIR\.."
$ANDROID_DIR = "$PROJECT_ROOT\android"
$APP_DIR = "$ANDROID_DIR\app"
$JNI_DIR = "$APP_DIR\src\main\jni"
$JAVA_DIR = "$APP_DIR\src\main\java"
$ASSETS_DIR = "$APP_DIR\src\main\assets"
$RES_DIR = "$APP_DIR\src\main\res"
$SDL_VENDOR_DIR = "$PROJECT_ROOT\android\SDL2-vendor"

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "  Snadder Game - Android Setup (Windows)" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "Project root: $PROJECT_ROOT"
Write-Host "Android dir:  $ANDROID_DIR"
Write-Host ""

# --- Step 1: Create directory structure ---
Write-Host "[1/7] Creating directory structure..." -ForegroundColor Yellow
$Dirs = @(
    "$JNI_DIR\src",
    $JAVA_DIR,
    "$ASSETS_DIR\font",
    "$ASSETS_DIR\sound",
    "$RES_DIR\values",
    "$RES_DIR\mipmap-hdpi",
    $SDL_VENDOR_DIR
)
foreach ($Dir in $Dirs) {
    if (-not (Test-Path $Dir)) {
        New-Item -ItemType Directory -Path $Dir -Force | Out-Null
    }
}
Write-Host "  [OK] Directories created" -ForegroundColor Green

# --- Step 2: Download/Clone SDL2 repositories ---
Write-Host ""
Write-Host "[2/7] Fetching SDL2 libraries..." -ForegroundColor Yellow

function Get-SDLRepo {
    param(
        [string]$RepoName,
        [string]$Version,
        [string]$TargetFolderName
    )

    $TargetDir = "$SDL_VENDOR_DIR\$TargetFolderName"
    if ((Test-Path $TargetDir) -and ((Test-Path "$TargetDir\Android.mk") -or (Test-Path "$TargetDir\android-project"))) {
        Write-Host "  [OK] $TargetFolderName already exists, skipping fetch" -ForegroundColor Green
        return
    }

    # Check if Git is available
    $GitAvailable = Get-Command git -ErrorAction SilentlyContinue
    if ($GitAvailable) {
        Write-Host "  -> Cloning $TargetFolderName via Git ($Version)..." -ForegroundColor Gray
        if (Test-Path $TargetDir) { Remove-Item -Path $TargetDir -Recurse -Force }
        git clone --depth 1 --branch $Version "https://github.com/libsdl-org/$RepoName.git" $TargetDir
        Write-Host "  [OK] $TargetFolderName cloned successfully" -ForegroundColor Green
    } else {
        # Fallback to ZIP download
        Write-Host "  -> Git not found. Downloading $TargetFolderName zip ($Version)..." -ForegroundColor Gray
        $ZipUrl = "https://github.com/libsdl-org/$RepoName/archive/refs/tags/$Version.zip"
        $ZipFile = "$SDL_VENDOR_DIR\$TargetFolderName.zip"
        
        Write-Host "    Downloading from $ZipUrl..." -ForegroundColor Gray
        Invoke-WebRequest -Uri $ZipUrl -OutFile $ZipFile -UseBasicParsing

        Write-Host "    Extracting $TargetFolderName..." -ForegroundColor Gray
        $TempExtractDir = "$SDL_VENDOR_DIR\temp_$TargetFolderName"
        if (Test-Path $TempExtractDir) { Remove-Item -Path $TempExtractDir -Recurse -Force }
        New-Item -ItemType Directory -Path $TempExtractDir -Force | Out-Null
        
        Expand-Archive -Path $ZipFile -DestinationPath $TempExtractDir -Force
        
        # Move the inner folder to TargetDir
        $InnerDir = Get-ChildItem -Path $TempExtractDir | Select-Object -First 1
        if (Test-Path $TargetDir) { Remove-Item -Path $TargetDir -Recurse -Force }
        Move-Item -Path $InnerDir.FullName -Destination $TargetDir -Force

        # Clean up
        Remove-Item -Path $ZipFile -Force
        Remove-Item -Path $TempExtractDir -Recurse -Force
        Write-Host "  [OK] $TargetFolderName downloaded and extracted successfully" -ForegroundColor Green
    }
}

Get-SDLRepo -RepoName "SDL" -Version $SDL2_VERSION -TargetFolderName "SDL2"
Get-SDLRepo -RepoName "SDL_image" -Version $SDL2_IMAGE_VERSION -TargetFolderName "SDL2_image"
Get-SDLRepo -RepoName "SDL_ttf" -Version $SDL2_TTF_VERSION -TargetFolderName "SDL2_ttf"
Get-SDLRepo -RepoName "SDL_mixer" -Version $SDL2_MIXER_VERSION -TargetFolderName "SDL2_mixer"

# --- Step 3: Copy SDLActivity.java and Java bridge files ---
Write-Host ""
Write-Host "[3/7] Copying SDL Java bridge files..." -ForegroundColor Yellow

$SDL_JAVA_SRC = "$SDL_VENDOR_DIR\SDL2\android-project\app\src\main\java\org\libsdl\app"
$DEST_JAVA_DIR = "$JAVA_DIR\org\libsdl\app"
if (-not (Test-Path $DEST_JAVA_DIR)) {
    New-Item -ItemType Directory -Path $DEST_JAVA_DIR -Force | Out-Null
}

if (Test-Path $SDL_JAVA_SRC) {
    $FilesToCopy = @(
        'SDLActivity.java',
        'SDLAudioManager.java',
        'SDLControllerManager.java',
        'SDL.java',
        'SDLDummyEdit.java',
        'SDLSurface.java'
    )
    foreach ($File in $FilesToCopy) {
        $SrcFile = "$SDL_JAVA_SRC\$File"
        if (Test-Path $SrcFile) {
            Copy-Item -Path $SrcFile -Destination $DEST_JAVA_DIR -Force
        }
    }
    Copy-Item -Path "$SDL_JAVA_SRC\HIDDevice*.java" -Destination $DEST_JAVA_DIR -Force -ErrorAction SilentlyContinue

    $JavaCount = (Get-ChildItem -Path $DEST_JAVA_DIR -Filter *.java).Count
    Write-Host "  [OK] Copied $JavaCount Java bridge files" -ForegroundColor Green
} else {
    Write-Error "SDL Java source not found at $SDL_JAVA_SRC. Verify SDL2 download."
}

# --- Step 4: Create the game's main Activity ---
Write-Host ""
Write-Host "[4/7] Creating SnadderActivity.java..." -ForegroundColor Yellow

$GAME_JAVA_DIR = "$JAVA_DIR\com\snadder\game"
if (-not (Test-Path $GAME_JAVA_DIR)) {
    New-Item -ItemType Directory -Path $GAME_JAVA_DIR -Force | Out-Null
}

$ActivityCode = @(
    'package com.snadder.game;',
    '',
    'import org.libsdl.app.SDLActivity;',
    '',
    '/**',
    ' * SnadderActivity - Main entry point for the Snadder Game on Android.',
    ' * Extends SDLActivity which handles the SDL lifecycle, EGL surface,',
    ' * and JNI bridge to the native C++ code.',
    ' */',
    'public class SnadderActivity extends SDLActivity {',
    '',
    '    /**',
    '     * Returns the name of the native shared library.',
    '     * This must match LOCAL_MODULE in Android.mk (which produces libmain.so).',
    '     */',
    '    @Override',
    '    protected String[] getLibraries() {',
    '        return new String[]{',
    '            "SDL2",',
    '            "SDL2_image",',
    '            "SDL2_ttf",',
    '            "SDL2_mixer",',
    '            "main"',
    '        };',
    '    }',
    '}'
) -join [Environment]::NewLine

$ActivityCode | Out-File -FilePath "$GAME_JAVA_DIR\SnadderActivity.java" -Encoding utf8 -Force
Write-Host "  [OK] SnadderActivity.java created" -ForegroundColor Green

# --- Step 5: Copy C++ sources ---
Write-Host ""
Write-Host "[5/7] Copying C++ game sources..." -ForegroundColor Yellow

Copy-Item -Path "$PROJECT_ROOT\75.cpp" -Destination "$JNI_DIR\src\75.cpp" -Force
Copy-Item -Path "$PROJECT_ROOT\features.hpp" -Destination "$JNI_DIR\src\features.hpp" -Force
Write-Host "  [OK] Copied 75.cpp and features.hpp" -ForegroundColor Green

# --- Step 6: Copy assets ---
Write-Host ""
Write-Host "[6/7] Copying game assets..." -ForegroundColor Yellow

# Fonts
if (Test-Path "$PROJECT_ROOT\assets\font") {
    Copy-Item -Path "$PROJECT_ROOT\assets\font\*.ttf" -Destination "$ASSETS_DIR\font\" -Force
    $FontCount = (Get-ChildItem -Path "$ASSETS_DIR\font" -Filter *.ttf).Count
    Write-Host "  [OK] Copied $FontCount font files" -ForegroundColor Green
} else {
    Write-Host "  [WARN] Warning: assets/font/ not found" -ForegroundColor Yellow
}

# Sounds
if (Test-Path "$PROJECT_ROOT\assets\sound") {
    Copy-Item -Path "$PROJECT_ROOT\assets\sound\*.wav" -Destination "$ASSETS_DIR\sound\" -Force
    $SoundCount = (Get-ChildItem -Path "$ASSETS_DIR\sound" -Filter *.wav).Count
    Write-Host "  [OK] Copied $SoundCount sound files" -ForegroundColor Green
} else {
    Write-Host "  [WARN] Warning: assets/sound/ not found" -ForegroundColor Yellow
}

# Questions CSV
if (Test-Path "$PROJECT_ROOT\questions.csv") {
    Copy-Item -Path "$PROJECT_ROOT\questions.csv" -Destination "$ASSETS_DIR\questions.csv" -Force
    Write-Host "  [OK] Copied questions.csv" -ForegroundColor Green
} else {
    Write-Host "  [WARN] Warning: questions.csv not found (game will use fallback questions)" -ForegroundColor Yellow
}

# --- Step 7: Setup Gradle wrapper ---
Write-Host ""
Write-Host "[7/7] Setting up Gradle wrapper..." -ForegroundColor Yellow

$WRAPPER_DIR = "$ANDROID_DIR\gradle\wrapper"
if (-not (Test-Path $WRAPPER_DIR)) {
    New-Item -ItemType Directory -Path $WRAPPER_DIR -Force | Out-Null
}

$WrapperProps = @(
    'distributionBase=GRADLE_USER_HOME',
    'distributionPath=wrapper/dists',
    'distributionUrl=https\://services.gradle.org/distributions/gradle-8.10.2-bin.zip',
    'networkTimeout=10000',
    'validateDistributionUrl=true',
    'zipStoreBase=GRADLE_USER_HOME',
    'zipStorePath=wrapper/dists'
) -join [Environment]::NewLine

$WrapperProps | Out-File -FilePath "$WRAPPER_DIR\gradle-wrapper.properties" -Encoding utf8 -Force

if (-not (Test-Path "$WRAPPER_DIR\gradle-wrapper.jar")) {
    Write-Host "  -> Downloading gradle-wrapper.jar..." -ForegroundColor Gray
    $JarUrl = "https://raw.githubusercontent.com/gradle/gradle/v8.10.2/gradle/wrapper/gradle-wrapper.jar"
    Invoke-WebRequest -Uri $JarUrl -OutFile "$WRAPPER_DIR\gradle-wrapper.jar" -UseBasicParsing
    Write-Host "  [OK] gradle-wrapper.jar downloaded" -ForegroundColor Green
} else {
    Write-Host "  [OK] gradle-wrapper.jar already exists" -ForegroundColor Green
}
Write-Host "  [OK] Gradle wrapper configuration completed" -ForegroundColor Green

# --- Create local.properties ---
Write-Host ""
Write-Host "Setting up local.properties..." -ForegroundColor Yellow

if (-not (Test-Path "$ANDROID_DIR\local.properties")) {
    $SDK_PATH = ""
    # Try to auto-detect SDK path
    if ($env:ANDROID_SDK_ROOT -and (Test-Path $env:ANDROID_SDK_ROOT)) {
        $SDK_PATH = $env:ANDROID_SDK_ROOT
    } elseif ($env:ANDROID_HOME -and (Test-Path $env:ANDROID_HOME)) {
        $SDK_PATH = $env:ANDROID_HOME
    } elseif (Test-Path "$env:USERPROFILE\AppData\Local\Android\Sdk") {
        $SDK_PATH = "$env:USERPROFILE\AppData\Local\Android\Sdk"
    } elseif (Test-Path "$ANDROID_DIR\android-sdk") {
        $SDK_PATH = "$ANDROID_DIR\android-sdk"
    }

    if ($SDK_PATH -eq "") {
        Write-Host "  -> Android SDK not found. Bootstrapping a local Android SDK in $ANDROID_DIR\android-sdk..." -ForegroundColor Cyan
        $SDK_PATH = "$ANDROID_DIR\android-sdk"
        $SdkToolsDir = "$SDK_PATH\cmdline-tools"
        if (-not (Test-Path "$SdkToolsDir\latest\bin\sdkmanager.bat")) {
            Write-Host "  -> Downloading Android cmdline-tools..." -ForegroundColor Gray
            $ZipUrl = "https://dl.google.com/android/repository/commandlinetools-win-11076708_latest.zip"
            $ZipFile = "$ANDROID_DIR\cmdline-tools.zip"
            Invoke-WebRequest -Uri $ZipUrl -OutFile $ZipFile -UseBasicParsing
            
            Write-Host "  -> Extracting cmdline-tools..." -ForegroundColor Gray
            $TempExtractDir = "$ANDROID_DIR\temp_sdk_extract"
            if (Test-Path $TempExtractDir) { Remove-Item -Path $TempExtractDir -Recurse -Force }
            New-Item -ItemType Directory -Path $TempExtractDir -Force | Out-Null
            Expand-Archive -Path $ZipFile -DestinationPath $TempExtractDir -Force
            
            $LatestDir = "$SdkToolsDir\latest"
            if (Test-Path $LatestDir) { Remove-Item -Path $LatestDir -Recurse -Force }
            New-Item -ItemType Directory -Path $LatestDir -Force | Out-Null
            
            Move-Item -Path "$TempExtractDir\cmdline-tools\*" -Destination $LatestDir -Force
            
            # Clean up
            Remove-Item -Path $ZipFile -Force
            Remove-Item -Path $TempExtractDir -Recurse -Force
            Write-Host "  [OK] cmdline-tools installed" -ForegroundColor Green
        }
        
        $SdkManager = "$SdkToolsDir\latest\bin\sdkmanager.bat"
        if (Test-Path $SdkManager) {
            Write-Host "  -> Accepting Android SDK licenses..." -ForegroundColor Gray
            $TempInput = [System.IO.Path]::GetTempFileName()
            "y`ny`ny`ny`ny`ny`ny`ny`ny`ny`n" | Out-File -FilePath $TempInput -Encoding ascii -Force
            Get-Content $TempInput | & $SdkManager --licenses | Out-Null
            Remove-Item $TempInput -Force
            Write-Host "  [OK] Licenses accepted" -ForegroundColor Green
            
            Write-Host "  -> Installing SDK packages (platform-tools, build-tools 35.0.0, platforms;android-35, ndk;27.2.12479018)..." -ForegroundColor Gray
            Write-Host "     This download is ~1.5 GB and can take several minutes." -ForegroundColor Cyan
            & $SdkManager --install "platform-tools" "build-tools;35.0.0" "platforms;android-35" "ndk;27.2.12479018"
            Write-Host "  [OK] SDK packages and NDK installed" -ForegroundColor Green
        } else {
            Write-Error "Failed to find sdkmanager at $SdkManager"
        }
    }

    if ($SDK_PATH -ne "") {
        # Escape backslashes for properties file format
        $SDK_PATH_ESCAPED = $SDK_PATH.Replace("\", "\\").Replace(":", "\:")
        $LocalProperties = @(
            "## This file is automatically generated by setup_android.ps1",
            "## Do NOT commit this file to version control.",
            "sdk.dir=$SDK_PATH_ESCAPED"
        ) -join [Environment]::NewLine
        $LocalProperties | Out-File -FilePath "$ANDROID_DIR\local.properties" -Encoding utf8 -Force
        Write-Host "  [OK] local.properties created (sdk.dir=$SDK_PATH)" -ForegroundColor Green
    }
} else {
    Write-Host "  [OK] local.properties already exists" -ForegroundColor Green
}

# --- Summary ---
Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "  Setup Complete!" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "SDL2 vendor directory: $SDL_VENDOR_DIR"
Write-Host ""
Write-Host "To build on Windows (with Android SDK/NDK installed):"
Write-Host "  cd android"
Write-Host "  .\gradlew assembleDebug"
Write-Host ""
Write-Host "APK output:"
Write-Host "  $APP_DIR\build\outputs\apk\debug\app-debug.apk"
Write-Host ""
