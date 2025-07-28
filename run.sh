#!/bin/zsh

BUILD_DIR="build"
APP_EXEC="App"
LOG_FILE="logs"

cd "$(dirname "$0")"

if [[ -d $BUILD_DIR ]]; then
    echo "Removing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuration with make generator..."
if ! cmake -G make .. > "$LOG_FILE" 2>&1; then
    echo "CMake configuration failed. Check $LOG_FILE for details."
    exit 1
fi

echo "Building with make..."
if ! make >> "$LOG_FILE" 2>&1; then
    echo "Build failed. Check $LOG_FILE for details."
    exit 1
fi
echo "-----------Build Complete-----------"

if [[ -f ./$APP_EXEC ]]; then
    echo "Error: $APP_EXEC executable not found!" | tee -a "$LOG_FILE"
    exit 1
fi