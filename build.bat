@echo off
setlocal enabledelayedexpansion enableextensions

REM Possible command line options:
REM -v|--verbose
REM -d|--debug
REM -r|--release
REM -w|--rel-with-debug-info
REM -c|--conn sqlite|mysql|postgresql|kafka
REM -i|--install-dir <INSTALL_DIR>
REM -c|--clean
REM -r|--rebuild (no reconfigure)
REM -g|--reconfigure

REM set default values
SET PLATFORM=WINDOWS
SET BUILD_TYPE=Debug
SET INSTALL_DIR=C:\install\dbgutil
SET CLEAN=0
SET REBUILD=0
SET RE_CONFIG=0

echo [DEBUG] Parsing args
:GET_OPTS
echo [DEBUG] processing option "%1" "%2"
IF /I "%1" == "-v" SET VERBOSE=1
IF /I "%1" == "--verbose" SET VERBOSE=1
IF /I "%1" == "-d" SET BUILD_TYPE=Debug
IF /I "%1" == "--debug" SET BUILD_TYPE=Debug
IF /I "%1" == "-r" SET BUILD_TYPE=Release
IF /I "%1" == "--release" SET BUILD_TYPE=Release
IF /I "%1" == "-w" SET BUILD_TYPE=RelWithDebInfo
IF /I "%1" == "--rel-with-debug-info" SET BUILD_TYPE=RelWithDebInfo
IF /I "%1" == "-i" SET INSTALL_DIR=%2 & shift
IF /I "%1" == "--install-dir" SET INSTALL_DIR=%2 & shift
IF /I "%1" == "-c" SET CLEAN=1
IF /I "%1" == "--clean" SET CLEAN=1
IF /I "%1" == "-r" SET REBUILD=1 & SET CLEAN=1
IF /I "%1" == "--rebuild" SET REBUILD=1 & SET CLEAN=1
IF /I "%1" == "-g" SET RE_CONFIG=1 & SET REBUILD=1 & SET CLEAN=1
IF /I "%1" == "--reconfigure" SET RE_CONFIG=1 & SET REBUILD=1  & SET CLEAN=1
REM TODO: not checking for illegal parameters
shift
IF "%1" == "--" GOTO SET_OPTS
IF NOT "%1" == "" GOTO GET_OPTS

:SET_OPTS
echo [DEBUG] Args parsed, options left: %*

REM set normal options
echo [INFO] Build type: %BUILD_TYPE%
echo [INFO] Install dir: %INSTALL_DIR%
SET OPTS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR%
IF "%VERBOSE%" == "1" SET OPTS=%OPTS% -DCMAKE_VERBOSE_MAKEFILE=ON
echo [DEBUG] Current options: %OPTS%

REM prepare build directory
SET BUILD_DIR=cmake_build\%PLATFORM%-%BUILD_TYPE%
echo [INFO] Using build directory: %BUILD_DIR%
if not exist %BUILD_DIR% (
    mkdir %BUILD_DIR%
)
if errorlevel 1 (
    echo [ERROR] failed to create build directory %BUILD_DIR%
    goto HANDLE_ERROR
)

pushd %BUILD_DIR% > NUL

REM print cmake info
echo [INFO] CMake version:
cmake --version

IF "%CLEAN%" == "1" (
    echo [INFO] Running target clean
    cmake --build . -j --verbose --target clean
    if errorlevel 1 (
        echo [ERROR] Clean failed, see errors above, aborting
        popd > NUL
        GOTO HANDLE_ERROR
    )
    IF "%REBUILD%" == "0" (
        popd > NUL
        exit /b 0
    )
)

REM Run fresh if re-configure is requested
IF "%RE_CONFIG%" == "1" (
    echo [INFO] Forcing fresh configuration
    OPTS=--fresh %OPTS%
)

REM configure phase
echo [INFO] Executing build command cmake %OPTS% ..\..\
echo [INFO] Configuring project
cmake %OPTS% ..\..\ -G "Ninja"
if errorlevel 1 (
    echo [ERROR] Configure phase failed, see errors above, aborting
    popd > NUL
    goto HANDLE_ERROR
)

REM build phase
REM NOTE: On windows MSVC (multi-config system), the configuration to build should be specified in build command
echo [INFO] Building
echo [INFO] Executing command: cmake --build . -j --verbose --config %BUILD_TYPE%
cmake --build . -j --verbose --config %BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] Build phase failed, see errors above, aborting
    popd > NUL
    goto HANDLE_ERROR
)

REM install phase
echo [INFO] Installing
cmake --install . --verbose --config %BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] Install phase failed, see errors above, aborting
    popd > NUL
    goto HANDLE_ERROR
)

popd > NUL
exit /b 0

:HANDLE_ERROR
echo [ERROR] Build failed, see errors above, aborting
exit /b 1