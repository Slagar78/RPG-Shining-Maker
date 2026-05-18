@echo off
chcp 65001 >nul
setlocal

:: Relaunch via cmd /k to keep window open
if "%1" neq "inside" (
    cmd /k "%~f0" inside
    exit
)

:START
cls
echo ========================================
echo      Portable Build RPG-Shinzo
echo ========================================
echo.

set "BASE=%~dp0"
set "PORTABLE_ROOT=%BASE%PortableRuby"
set "PORTABLE_BIN=%PORTABLE_ROOT%\bin"
set "RUBY_EXE=%PORTABLE_BIN%\ruby.exe"
set "GEM_HOME=%PORTABLE_ROOT%\gems"
set "GEM_PATH=%PORTABLE_ROOT%\gems"
set "PATH=%PORTABLE_BIN%;%PATH%"

set "RELEASE=%BASE%Release"
set "LOG=%RELEASE%\build_log.txt"

:: Check portable Ruby
if not exist "%RUBY_EXE%" (
    echo [ERROR] Portable Ruby not found: %RUBY_EXE%
    echo Run portable_setup.bat first.
    goto END
)

:: Prepare Release folder
if exist "%RELEASE%" rmdir /s /q "%RELEASE%"
mkdir "%RELEASE%"

echo Build started: %DATE% %TIME% > "%LOG%"

:: ---------- Step 1: Copy resources ----------
echo [1/3] Copying game files...

xcopy "%BASE%data"   "%RELEASE%\data\"   /E /I /Y
xcopy "%BASE%assets" "%RELEASE%\assets\" /E /I /Y
xcopy "%BASE%lib"    "%RELEASE%\lib\"    /E /I /Y

copy "%BASE%game.rb" "%RELEASE%\"

copy "%PORTABLE_ROOT%\dll\libraylib.dll" "%RELEASE%\"
copy "%PORTABLE_ROOT%\dll\zlib1.dll" "%RELEASE%\"
copy "%PORTABLE_ROOT%\dll\libwinpthread-1.dll" "%RELEASE%\"

if errorlevel 1 (
    echo [ERROR] Copy failed
    goto END
)

echo [OK] Files copied

:: ---------- Step 2: Build EXE ----------
echo [2/3] Building autonomous EXE...

cd /d "%RELEASE%"

"%RUBY_EXE%" -S ocran game.rb --output Game.exe --no-enc --gem-full --no-lzma --windows --dll libraylib.dll

if errorlevel 1 (
    echo [ERROR] Ocran failed
    goto END
)

echo [OK] EXE created

:: ---------- Step 3: Done ----------
echo [3/3] BUILD SUCCESSFUL
echo File: %RELEASE%\Shinzo.exe

:END
echo.
echo ========================================
echo Press:
echo [R] - Rebuild
echo [Q] - Quit
echo ========================================

choice /c RQ /n /m "Choose: "

if errorlevel 2 exit
if errorlevel 1 goto START