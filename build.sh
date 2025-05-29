#!/bin/bash

PLATFORM=$(uname -s)

# Possible options:
# -v|--verbose
# -d|--debug
# -r|--release
# -w|--rel-with-debug-info
# -i|--install-dir <INSTALL_DIR>

# set default values
BUILD_TYPE=Debug
OS=`uname -o`
if [ "$OS" = "Msys" ]; then
    INSTALL_DIR=/c/install/dbgutil
else
    INSTALL_DIR=~/install/dbgutil
fi

# parse options
TEMP=$(getopt -o vdrwc:i: -l verbose,debug,release,rel-with-debug-info,conn:,install-dir: -- "$@")
eval set -- "$TEMP"

declare -a CONNS=()
while true; do
  case "$1" in
    -v | --verbose ) VERBOSE=1; shift ;;
    -d | --debug ) BUILD_TYPE=Debug; shift ;;
    -r | --release ) BUILD_TYPE=Release; shift ;;
    -w | --rel-with-debug-info ) BUILD_TYPE=RelWithDebInfo; shift ;;
    -i | --install-dir) INSTALL_DIR="$2"; shift 2 ;;
    -- ) shift; break ;;
    * ) echo "ERROR: Invalid option $1, aborting"; exit 1; break ;;
  esac
done

# set normal options
echo "[INFO] Build type: $BUILD_TYPE"
echo "[INFO] Install dir: $INSTALL_DIR"
OPTS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"
if [ $VERBOSE==1 ]; then
    OPTS+=" -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

# prepare build directory
BUILD_DIR=cmake_build/${PLATFORM}-${BUILD_TYPE}
echo "[INFO] Using build directory: '$BUILD_DIR'"
mkdir -p $BUILD_DIR
pushd $BUILD_DIR > /dev/null

# configure phase
echo "[INFO] Configuring project"
echo "[INFO] Executing: cmake $OPTS $* ../../"
cmake $OPTS $* ../../
if [ $? -ne 0 ]; then
    echo "ERROR: Configure phase failed, see errors above, aborting"
    popd > /dev/null
    exit 1
fi

# build phase
echo "[INFO] Building project"
cmake --build . -j
if [ $? -ne 0 ]; then
    echo "ERROR: Build phase failed, see errors above, aborting"
    popd > /dev/null
    exit 1
fi

# install phase
echo "[INFO] Installing"
cmake --install .
if [ $? -ne 0 ]; then
    echo "ERROR: Install phase failed, see errors above, aborting"
    popd > /dev/null
    exit 1
fi

popd > /dev/null