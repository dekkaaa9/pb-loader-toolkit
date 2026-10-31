@echo off
setlocal EnableDelayedExpansion

echo ============================================
echo  PBCheat Build Script
echo  Target: i686-w64-mingw32 (32-bit)
echo ============================================
echo.

set TOOLDIR=D:\llvm-mingw\llvm-mingw-20261020-msvcrt-x86_64
set COMPILER=%TOOLDIR%\bin\i686-w64-mingw32-clang++.exe
set LIBUNWIND=%TOOLDIR%\i686-w64-mingw32\lib\libunwind.a

if not exist "%COMPILER%" (
    echo [!] Compiler not found at %COMPILER%
    echo [!] Trying fallback path...
    if exist "D:\llvm-mingw\bin\i686-w64-mingw32-clang++.exe" (
        set COMPILER=D:\llvm-mingw\bin\i686-w64-mingw32-clang++.exe
        set LIBUNWIND=D:\llvm-mingw\i686-w64-mingw32\lib\libunwind.a
        echo [+] Using fallback: !COMPILER!
    ) else (
        echo [-] No 32-bit compiler found.
        echo [-] Download llvm-mingw from:
        echo     https://github.com/mstorsjo/llvm-mingw/releases
        pause
        exit /b 1
    )
)

rem Static link everything, no exceptions/rtti, explicit libunwind.a
rem to avoid libunwind.dll runtime dependency
set CFLAGS=-shared -static -fno-exceptions -fno-rtti -O2 -std=c++17 -Wall -Wno-unused-variable -Wno-unused-function
set DEFINES=-D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN
set INCLUDES=-IPBCheat
set LIBS=-ld3d9 -lkernel32 -luser32 -lgdi32 -lpsapi -ladvapi32 "%LIBUNWIND%"

echo [*] Compiling PBCheat.dll...
"%COMPILER%" %CFLAGS% %DEFINES% %INCLUDES% ^
    PBCheat\dllmain.cpp ^
    PBCheat\bypass.cpp ^
    PBCheat\hooks.cpp ^
    PBCheat\entities.cpp ^
    PBCheat\esp.cpp ^
    PBCheat\gui.cpp ^
    PBCheat\menu.cpp ^
    %LIBS% ^
    -o PBCheat.dll

if errorlevel 1 (
    echo [-] DLL build FAILED
    pause
    exit /b 1
)
echo [+] PBCheat.dll built successfully

echo.
echo [*] Compiling PBInjector.exe ^(32-bit^)...
"%COMPILER%" -static -fno-exceptions -fno-rtti -O2 -std=c++17 -Wall -mconsole -municode ^
    PBInjector\main.cpp ^
    -o PBInjector.exe -lpsapi "%LIBUNWIND%"

if errorlevel 1 (
    echo [-] Injector build FAILED
    pause
    exit /b 1
)
echo [+] PBInjector.exe built successfully

echo.
echo [*] Compiling PBInjectorGUI.exe ^(32-bit GUI^)...
"%COMPILER%" -static -fno-exceptions -fno-rtti -O2 -std=c++17 -Wall -mwindows -municode ^
    PBInjectorGUI\main.cpp ^
    -o PBInjectorGUI.exe -luser32 -lgdi32 -lkernel32 -lpsapi -ladvapi32 -lshell32 "%LIBUNWIND%"

if errorlevel 1 (
    echo [-] GUI build FAILED
    pause
    exit /b 1
)
echo [+] PBInjectorGUI.exe built successfully

echo.
echo ============================================
echo  BUILD COMPLETE
echo ============================================
echo  Files:
echo    PBCheat.dll       — inject into PointBlank.exe
echo    PBInjector.exe    — manual map injector (run as Admin while game is running)
echo    PBInjectorGUI.exe — GUI injector harian (double-click, klik INJECT)
echo.
echo  Usage (daily):
echo    1. Launch Point Blank ^(wait for lobby^)
echo    2. Double-click PBInjectorGUI.exe
echo    3. Klik INJECT ^(klik YES kalau UAC muncul^)
echo    4. Menu muncul otomatis, hotkey INSERT buat toggle
echo ============================================
pause
