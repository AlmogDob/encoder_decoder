@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================================================
REM Config
REM ============================================================

REM ---- Warnings (roughly -W -Wall -Wextra) ----
set "CWARN=/nologo /W4 /wd4996"

REM Optional: treat warnings as errors
REM set "CWARN=%CWARN% /WX"

REM ---- Build mode flags are selected later ----
REM Debug defaults:
REM   /Zi /Od /MDd /DDEBUG
REM Release defaults:
REM   /O2 /DNDEBUG /MD

REM ---- C standard ----
set "CSTD=/std:c11"

REM Extra linker libs, if needed:
REM set "CLINKS=/link user32.lib"

set "BUILDDIR=build"

REM ============================================================
REM Parse args
REM ============================================================
set "NOCLEAN=0"
set "BUILDONLY=0"
set "CLEANONLY=0"
set "RELEASE=0"
set "HELP=0"
set "FILES="
set "RUN_ARGS="
set "INPUT_SEEN=0"

:parse
if "%~1"=="" goto doneparse

if "%INPUT_SEEN%"=="1" (
  set "RUN_ARGS=!RUN_ARGS! "%~1""
  shift
  goto parse
)
if /i "%~1"=="--no-clean"   (set "NOCLEAN=1"   & shift & goto parse)
if /i "%~1"=="-nc"          (set "NOCLEAN=1"   & shift & goto parse)
if /i "%~1"=="--build-only" (set "BUILDONLY=1" & shift & goto parse)
if /i "%~1"=="-b"           (set "BUILDONLY=1" & shift & goto parse)
if /i "%~1"=="--clean-only" (set "CLEANONLY=1" & shift & goto parse)
if /i "%~1"=="-c"           (set "CLEANONLY=1" & shift & goto parse)
if /i "%~1"=="--release"    (set "RELEASE=1"   & shift & goto parse)
if /i "%~1"=="-r"           (set "RELEASE=1"   & shift & goto parse)
if /i "%~1"=="--time"       (set "TIMEIT=1"    & shift & goto parse)
if /i "%~1"=="-t"           (set "TIMEIT=1"    & shift & goto parse)
if /i "%~1"=="--help"       (set "HELP=1"      & shift & goto parse)
if /i "%~1"=="-h"           (set "HELP=1"      & shift & goto parse)
if /i "%~1"=="--input" (
  set "INPUT_SEEN=1"
  shift
  goto parse
)
if /i "%~1"=="-i" (
  set "INPUT_SEEN=1"
  shift
  goto parse
)

REM Otherwise treat as file
set "FILES=!FILES! "%~1""
shift
goto parse

:doneparse

if "%INPUT_SEEN%"=="1" if not defined RUN_ARGS (
  set "FAIL_RC=1"
  set "FAIL_MSG=-i/--input requires at least one argument."
  goto fail
)

REM ============================================================
REM Select build mode flags
REM ============================================================
if "%RELEASE%"=="1" (
  set "CCHECK=/O2 /GL /Gy /MT /DNDEBUG /Zi"
  set "CLINKS=/link /LTCG /OPT:REF /OPT:ICF /DEBUG"
) else (
  REM /RTC1 only works with /Od (debug-ish); using /MDd for debug CRT
  set "CCHECK=/FC /Zi /Od /MDd /DDEBUG /RTC1"

  REM Optional static analysis (slower)
  REM set "CCHECK=%CCHECK% /analyze"

  REM Optional AddressSanitizer (only if your MSVC supports it)
  @REM set "CCHECK=%CCHECK% /fsanitize=address"
)

if "%HELP%"=="1" goto usage

if not defined FILES (
  goto usage
  set "FAIL_RC=1"
  set "FAIL_MSG=Usage: %~nx0 <file1.c> [file2.c ...] [--input^|-i ""program args""] [--no-clean^|-nc] [--build-only^|-b] [--clean-only^|-c] [--release^|-r]"
  goto fail
)

if "%BUILDONLY%"=="1" if "%CLEANONLY%"=="1" (
  set "FAIL_RC=1"
  set "FAIL_MSG=--build-only and --clean-only cannot be used together."
  goto fail
)

REM ============================================================
REM Ensure build dir exists
REM ============================================================
mkdir "%BUILDDIR%" 2>nul

REM ============================================================
REM Setup MSVC environment once (unless we're only cleaning)
REM ============================================================
if "%CLEANONLY%"=="0" (
  call :setup_msvc
  if errorlevel 1 goto fail
)

REM ============================================================
REM Execute
REM ============================================================
if "%CLEANONLY%"=="1" (
  for %%F in (%FILES%) do (
    call :clean "%%~fF"
    if errorlevel 1 goto fail
  )
  echo [INFO] done!
  exit /b 0
)

for %%F in (%FILES%) do (
  call :build "%%~fF"
  if errorlevel 1 goto fail
)

if "%BUILDONLY%"=="1" (
  echo [INFO] done!
  exit /b 0
)

for %%F in (%FILES%) do (
  call :run "%%~fF"
  if errorlevel 1 goto fail
)

if "%NOCLEAN%"=="0" (
  for %%F in (%FILES%) do (
    call :clean "%%~fF"
    REM clean failures are non-fatal by default; change if you want
  )
)

echo [INFO] done!
exit /b 0

REM ============================================================
REM Subroutines
REM ============================================================

:setup_msvc

REM Find VS install via vswhere (ships with VS Installer)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  set "FAIL_RC=1"
  set "FAIL_MSG=vswhere not found at: %ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  exit /b 1
)

for /f "usebackq delims=" %%I in (`
  "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
`) do set "VSINSTALL=%%I"

if not defined VSINSTALL (
  set "FAIL_RC=1"
  set "FAIL_MSG=No Visual Studio installation with C++ tools found (via vswhere)."
  exit /b 1
)

set "VSDEVCMD=%VSINSTALL%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
  set "FAIL_RC=1"
  set "FAIL_MSG=VsDevCmd.bat not found at: %VSDEVCMD%"
  exit /b 1
)

call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul 2>nul
if errorlevel 1 (
  set "FAIL_RC=1"
  set "FAIL_MSG=Failed to initialize MSVC environment (VsDevCmd.bat)."
  exit /b 1
)

exit /b 0


:build
setlocal EnableExtensions

if "%~1"=="" (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=build: missing input file."
  exit /b 1
)

if /i not "%~x1"==".c" (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=build: expected a .c file, got "%~1"."
  exit /b 1
)

if not exist "%~1" (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=build: file not found: "%~1"."
  exit /b 1
)

set "SRC=%~1"
set "NAME=%~n1"
set "NAME_AND_EXTENSION=%~nx1"

if "%RELEASE%"=="1" (
  echo [INFO] building release:
) else (
  echo [INFO] building debug:
)

cl.exe %CWARN% %CCHECK% %CSTD% "%SRC%" ^
  /Fe:"%BUILDDIR%\%NAME%.exe" ^
  /Fo:"%BUILDDIR%\%NAME%.obj" ^
  /Fd:"%BUILDDIR%\%NAME%.pdb" %CLINKS%

set "RC=%errorlevel%"

REM Copy ASan runtime DLL if present (path comes from the MSVC env)
@REM set "ASAN_DLL=%VCToolsInstallDir%bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll"
@REM if exist "%ASAN_DLL%" (
@REM   copy /y "%ASAN_DLL%" "%BUILDDIR%" >nul
@REM )

if not "%RC%"=="0" (
  endlocal
  set "FAIL_RC=%RC%"
  set "FAIL_MSG=build failed for %NAME% (cl.exe returned %RC%)."
  exit /b 1
)

endlocal & exit /b 0


:run
setlocal EnableExtensions

if "%~1"=="" (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=run: missing input file."
  exit /b 1
)

set "NAME=%~n1"
set "EXE=%NAME%.exe"

if not exist "%BUILDDIR%\%EXE%" (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=run: "%BUILDDIR%\%EXE%" not found. Build it first."
  exit /b 1
)

echo [INFO] running "%EXE%"
echo.

pushd "%BUILDDIR%" || (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=run: failed to change directory to "%BUILDDIR%"."
  exit /b 1
)

call :invoke_exe "%EXE%"
set "RC=%errorlevel%"
popd

if not "%RC%"=="0" (
  endlocal
  set "FAIL_RC=%RC%"
  set "FAIL_MSG=%NAME% exited with status %RC%."
  exit /b 1
)

echo.
endlocal & exit /b 0

:invoke_exe
if "%TIMEIT%"=="1" goto invoke_timed
goto invoke_normal

:invoke_normal
if not defined RUN_ARGS goto invoke_no_arg
"%~1" %RUN_ARGS%
exit /b %errorlevel%

:invoke_no_arg
"%~1"
exit /b %errorlevel%

:invoke_timed
if not defined RUN_ARGS goto invoke_timed_no_arg

powershell -NoProfile -Command ^
  "$sw = [System.Diagnostics.Stopwatch]::StartNew(); " ^
  "& '.\%~1' %RUN_ARGS%; " ^
  "$rc = $LASTEXITCODE; " ^
  "$sw.Stop(); " ^
  "Write-Host ''; " ^
  "Write-Host ('[TIME] {0:N3} ms ({1:N6} s)' -f $sw.Elapsed.TotalMilliseconds, $sw.Elapsed.TotalSeconds); " ^
  "exit $rc"
exit /b %errorlevel%

:invoke_timed_no_arg
powershell -NoProfile -Command ^
  "$sw = [System.Diagnostics.Stopwatch]::StartNew(); " ^
  "& '.\%~1'; " ^
  "$rc = $LASTEXITCODE; " ^
  "$sw.Stop(); " ^
  "Write-Host ''; " ^
  "Write-Host ('[TIME] {0:N3} ms ({1:N6} s)' -f $sw.Elapsed.TotalMilliseconds, $sw.Elapsed.TotalSeconds); " ^
  "exit $rc"
exit /b %errorlevel%

:clean
setlocal EnableExtensions

if "%~1"=="" (
  endlocal
  set "FAIL_RC=1"
  set "FAIL_MSG=clean: missing input file."
  exit /b 1
)

set "NAME=%~n1"
echo [INFO] removing %NAME% build files.
del /q "%BUILDDIR%\%NAME%.*" >nul 2>nul

REM Also remove the AddressSanitizer runtime DLL we may have copied into build\
del /q "%BUILDDIR%\clang_rt.asan_dynamic-x86_64.dll" >nul 2>nul

endlocal & exit /b 0


REM ============================================================
REM Centralized failure handler
REM ============================================================
:fail
if not defined FAIL_RC set "FAIL_RC=1"
if not defined FAIL_MSG set "FAIL_MSG=Unknown error."

1>&2 echo.
1>&2 echo [ERROR] %FAIL_MSG%
exit /b %FAIL_RC%

:usage
echo Usage: %~nx0 ^<file1.c^> [file2.c ...] [options]
echo.
echo Options:
echo   --help, -h                   Show this help message
echo   --input, -i program_args...  Pass all remaining arguments to the built executable. Everything after -i or --input is forwarded as-is.
echo   --no-clean, -nc              Do not remove build artifacts after running
echo   --build-only, -b             Build only, do not run
echo   --clean-only, -c             Remove build artifacts only
echo   --release, -r                Build in release mode
echo   --time, -t                   Time the program run
exit /b 1