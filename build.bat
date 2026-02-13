@echo off
echo Building Fast File Explorer...
cl /EHsc /O2 /W4 FileExplorer.cpp /link /LIBPATH:C:\Windows\System32 comctl32.lib shell32.lib user32.lib gdi32.lib advapi32.lib ole32.lib
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)
echo.
echo Build successful! Run FileExplorer.exe
pause
