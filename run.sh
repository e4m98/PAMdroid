#!/bin/zsh

BUILD_DIR="build"
APP_EXEC="App"
LOG_FILE="logs"

cd "$(dirname "$0")"

# 기존 빌드 디렉토리 삭제
if [[ -d $BUILD_DIR ]]; then
    echo "Removing build directory..."
    rm -rf "$BUILD_DIR"
fi

# 새 빌드 디렉토리 생성
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# CMake 및 빌드 실행
echo "Configuration"
if ! cmake .. > "$LOG_FILE" 2>&1; then
    echo "CMake configuration failed. Check $LOG_FILE for details."
    exit 1
fi

echo "Building with Make..."
if ! make >> "$LOG_FILE" 2>&1; then
    echo "Build failed. Check $LOG_FILE for details."
    exit 1
fi
echo "-----------Build Complete-----------"

# 실행 파일 확인 및 실행
if [[ -f ./$APP_EXEC ]]; then
    ./$APP_EXEC
else
    echo "Error: $APP_EXEC executable not found!" | tee -a "$LOG_FILE"
    exit 1
fi