@echo off
rem Starts the LMS from the data folder so the database and submission_files\ always land in the same place.
rem Usage: lms-start.bat [data-folder]   (default: C:\LMS\data)
set "DATA_DIR=%~1"
if "%DATA_DIR%"=="" set "DATA_DIR=C:\LMS\data"
if not exist "%DATA_DIR%" mkdir "%DATA_DIR%"
cd /d "%DATA_DIR%" || exit /b 1
"%~dp0lms_app.exe" "%DATA_DIR%\school_lms.sqlite3"
