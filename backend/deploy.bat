@echo off
REM ScareCrowWeb Cloud Run Deployment Script
REM 
REM Prerequisites:
REM 1. Install Google Cloud SDK: https://cloud.google.com/sdk/docs/install
REM 2. Login: gcloud auth login
REM 3. Set project: gcloud config set project YOUR_PROJECT_ID
REM

echo ========================================
echo ScareCrowWeb Cloud Run Deployment
echo ========================================

REM Change to backend directory
cd /d "%~dp0"

REM Check if gcloud is installed
where gcloud >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: gcloud CLI not found. Please install Google Cloud SDK.
    echo https://cloud.google.com/sdk/docs/install
    exit /b 1
)

REM Build and deploy
echo.
echo Building and deploying to Cloud Run...
echo This may take a few minutes on first deployment.
echo.

gcloud run deploy scarecrow-api ^
    --source . ^
    --region asia-southeast1 ^
    --allow-unauthenticated ^
    --set-env-vars "SUPABASE_URL=%SUPABASE_URL%,SUPABASE_KEY=%SUPABASE_KEY%,STORAGE_BUCKET=scarecrow-images"

echo.
echo ========================================
echo Deployment complete!
echo.
echo Next steps:
echo 1. Copy your Cloud Run URL from above
echo 2. Update frontend/js/config.js with your Cloud Run URL
echo 3. Update ESP32 device SERVER_URL to your Cloud Run URL
echo ========================================
