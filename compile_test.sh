#!/bin/bash
# Quick compilation test script

echo "=== MIoT Camera Bridge - Compilation Test ==="
echo ""

# Check for required tools
echo "Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Please install: brew install cmake"
    exit 1
fi
echo "✅ CMake found"

if ! command -v make &> /dev/null; then
    echo "❌ Make not found. Please install build tools"
    exit 1
fi
echo "✅ Make found"

# Check OpenSSL
if [ -d "$(brew --prefix openssl 2>/dev/null)" ]; then
    echo "✅ OpenSSL found at $(brew --prefix openssl)"
    export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
else
    echo "⚠️  OpenSSL not found via Homebrew"
    echo "   You may need to install it: brew install openssl"
fi

# Check libcurl
if ! pkg-config --exists libcurl 2>/dev/null && ! [ -f "/usr/lib/libcurl.dylib" ]; then
    echo "⚠️  libcurl may not be installed"
    echo "   You may need to install it: brew install curl"
else
    echo "✅ libcurl found"
fi

echo ""
echo "=== Starting compilation ===" 
echo ""

cd "$(dirname "$0")"

# Clean build
if [ -d "build" ]; then
    echo "Cleaning old build..."
    rm -rf build
fi

mkdir -p build
cd build

# Configure
echo "Configuring with CMake..."
if [ -n "$OPENSSL_ROOT_DIR" ]; then
    cmake -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" .. || exit 1
else
    cmake .. || exit 1
fi

# Build
echo ""
echo "Building..."
make -j$(sysctl -n hw.ncpu) || exit 1

echo ""
echo "=== Compilation Complete! ==="
echo ""
echo "Generated files:"
ls -lh miot_* 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""
echo "To run:"
echo "  Basic version:    ./miot_lan_discovery_demo"
echo "  With cloud info:  ./miot_discovery_with_cloud -f ../token.txt"
echo ""

