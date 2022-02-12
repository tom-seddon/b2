@echo off
if exist "%1" rd /s /q "%1"
if exist "%1" goto delete_failed
mkdir "%1"
if errorlevel 1 goto create_failed
exit /b 0

:delete_failed
echo Failed to delete folder: %1
exit /b 1

:create_failed
echo Failed to create folder: %1
exit /b 1
