rd /s /q "%1"
mkdir "%1"
@if errorlevel 1 exit /b 1
@exit /b 0
