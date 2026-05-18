@echo off
chcp 65001 >nul
setlocal

:: ===== перезапуск через cmd /k (чтобы не закрывался) =====
if "%1" neq "inside" (
    cmd /k "%~f0" inside
    exit
)

:START
cls
echo ========================================
echo      Сборка RPG-Shinzo
echo ========================================
echo.

set "BASE=%~dp0"
set "RELEASE=%BASE%Release"
set "LOG=%RELEASE%\build_log.txt"

:: ---------- подготовка ----------
if exist "%RELEASE%" rmdir /s /q "%RELEASE%"
mkdir "%RELEASE%"

echo Сборка: %DATE% %TIME% > "%LOG%"

:: ---------- шаг 1 ----------
echo [1/3] Копирование...

xcopy "%BASE%data"   "%RELEASE%\data\"   /E /I /Y
xcopy "%BASE%assets" "%RELEASE%\assets\" /E /I /Y
xcopy "%BASE%lib"    "%RELEASE%\lib\"    /E /I /Y

copy "%BASE%game.rb" "%RELEASE%\"
copy "%BASE%PortableRuby\dll\libraylib.dll" "%RELEASE%\"
copy "%BASE%PortableRuby\dll\zlib1.dll" "%RELEASE%\"
copy "%BASE%PortableRuby\dll\libwinpthread-1.dll" "%RELEASE%\"

if errorlevel 1 (
    echo.
    echo ? Ошибка копирования
    goto END
)

echo ? Готово

:: ---------- шаг 2 ----------
echo [2/3] Сборка EXE...

cd /d "%RELEASE%"

ocran game.rb --output MyGame.exe --no-enc --gem-full --no-lzma --windows --dll libraylib.dll --dll zlib1.dll --dll libwinpthread-1.dll

if errorlevel 1 (
    echo.
    echo ? Ошибка Ocran
    goto END
)

echo ? EXE создан

:: ---------- шаг 3 ----------
echo [3/3] ГОТОВО
echo Файл: %RELEASE%\MyGame.exe

:END
echo.
echo ========================================
echo Нажми:
echo [R] — пересобрать
echo [Q] — выйти
echo ========================================

choice /c RQ /n /m "Выбор: "

if errorlevel 2 exit
if errorlevel 1 goto START