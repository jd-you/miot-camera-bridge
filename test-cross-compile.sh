#!/bin/bash
# Quick test for ARM64 cross-compilation

echo "╔═══════════════════════════════════════════════════════════════════╗"
echo "║  ARM64 Cross-Compilation Quick Test                               ║"
echo "╚═══════════════════════════════════════════════════════════════════╝"
echo ""

# Test 1: Docker availability
echo "[1/5] Checking Docker..."
if docker info > /dev/null 2>&1; then
    echo "  ✅ Docker is running"
else
    echo "  ❌ Docker is not running"
    echo "  Please start Docker Desktop"
    exit 1
fi

# Test 2: ARM64 emulation
echo ""
echo "[2/5] Testing ARM64 emulation..."
if docker run --rm --platform linux/arm64 ubuntu:22.04 uname -m 2>/dev/null | grep -q "aarch64"; then
    echo "  ✅ ARM64 emulation working"
else
    echo "  ⚠️  ARM64 emulation may not be available"
    echo "  Trying to set up QEMU..."
    docker run --rm --privileged multiarch/qemu-user-static --reset -p yes 2>/dev/null || true
fi

# Test 3: Build simple ARM64 binary
echo ""
echo "[3/5] Testing ARM64 compilation..."
cat > /tmp/test_arm64.c << 'EOF'
#include <stdio.h>
int main() {
    printf("Hello from ARM64!\n");
    return 0;
}
EOF

if docker run --rm --platform linux/arm64 \
    -v /tmp:/tmp \
    ubuntu:22.04 \
    bash -c "apt-get update -qq && apt-get install -y -qq gcc && gcc /tmp/test_arm64.c -o /tmp/test_arm64 && /tmp/test_arm64" 2>&1 | grep -q "Hello from ARM64"; then
    echo "  ✅ ARM64 compilation successful"
else
    echo "  ❌ ARM64 compilation failed"
    exit 1
fi

# Test 4: Check library
echo ""
echo "[4/5] Checking for ARM64 library..."
LIB_PATH="/Users/jiadiy/Workspace/miot_camera_bridge/libs/linux/arm64/libmiot_camera_lite.so"
if [ -f "$LIB_PATH" ]; then
    echo "  ✅ Library found"
    SIZE=$(ls -lh "$LIB_PATH" | awk '{print $5}')
    echo "     Size: $SIZE"
else
    echo "  ⚠️  Library not found at: $LIB_PATH"
    echo "     Will try to copy from original project during build"
fi

# Test 5: Check dependencies
echo ""
echo "[5/5] Checking build dependencies..."
MISSING=""
if ! command -v cmake > /dev/null 2>&1; then
    MISSING="$MISSING cmake"
fi

if [ -n "$MISSING" ]; then
    echo "  ⚠️  Missing on host:$MISSING"
    echo "     (These will be installed in Docker container)"
else
    echo "  ✅ All host tools available"
fi

# Summary
echo ""
echo "╔═══════════════════════════════════════════════════════════════════╗"
echo "║  Test Summary                                                      ║"
echo "╚═══════════════════════════════════════════════════════════════════╝"
echo ""
echo "Your system is ready for ARM64 cross-compilation!"
echo ""
echo "To build:"
echo "  cd /Users/jiadiy/Workspace/miot_camera_bridge"
echo "  ./build-arm64-docker.sh"
echo ""
echo "The script will:"
echo "  1. Create an ARM64 Ubuntu container"
echo "  2. Install build tools (gcc, cmake, etc.)"
echo "  3. Compile your project for ARM64 Linux"
echo "  4. Generate binaries in build-arm64/"
echo ""
echo "Estimated time: 5-10 minutes (first run may take longer)"
echo ""

