$ErrorActionPreference = "Stop"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "  Snadder Game - Automated Android APK Build" -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

# Run Setup Script
Write-Host ">>> Running Android Setup and Bootstrap..." -ForegroundColor Yellow
& "$SCRIPT_DIR\scripts\setup_android.ps1"

# Run Gradle Build
Write-Host ""
Write-Host ">>> Running Gradle Build..." -ForegroundColor Yellow
Push-Location "$SCRIPT_DIR\android"
try {
    .\gradlew assembleDebug
} finally {
    Pop-Location
}

# Verify Output
$APK_PATH = "$SCRIPT_DIR\android\app\build\outputs\apk\debug\app-debug.apk"
if (Test-Path $APK_PATH) {
    Write-Host ""
    Write-Host "==============================================" -ForegroundColor Green
    Write-Host "  Build Successful!" -ForegroundColor Green
    Write-Host "  APK Location:" -ForegroundColor Green
    Write-Host "    $APK_PATH" -ForegroundColor White
    Write-Host "==============================================" -ForegroundColor Green
} else {
    Write-Error "Build failed: APK not found at $APK_PATH"
}
