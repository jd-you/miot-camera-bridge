#!/bin/bash
# Cross-compile for Linux ARM64 using Docker

set -e

echo "=================================================="
echo "  Building for Linux ARM64 using Docker"
echo "=================================================="
echo ""

# Check Docker
if ! docker info > /dev/null 2>&1; then
    echo "❌ Error: Docker is not running"
    echo "Please start Docker Desktop first"
    exit 1
fi
echo "✅ Docker is running"

# Check buildx
if ! docker buildx version > /dev/null 2>&1; then
    echo "❌ Error: Docker buildx not available"
    echo "Please install: docker buildx install"
    exit 1
fi
echo "✅ Docker buildx available"

PROJECT_DIR="/Users/jiadiy/Workspace/miot_camera_bridge"
cd "$PROJECT_DIR"

# Check for ARM64 library
if [ ! -f "libs/linux/arm64/libmiot_camera_lite.so" ]; then
    echo "⚠️  Warning: ARM64 library not found"
    echo "Copying from original project..."
    
    SRC_LIB="/Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/libs/linux/arm64"
    if [ -d "$SRC_LIB" ]; then
        mkdir -p libs/linux/arm64
        cp -r "$SRC_LIB"/* libs/linux/arm64/
        echo "✅ Library copied"
    else
        echo "❌ Error: Source library not found at $SRC_LIB"
        echo "Please copy manually"
        exit 1
    fi
else
    echo "✅ ARM64 library found"
fi

# Create Dockerfile
echo ""
echo "Creating Dockerfile..."
cat > Dockerfile.cross-arm64 << 'EOF'
FROM --platform=linux/arm64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    pkg-config \
    file \
    nlohmann-json3-dev\
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-rtsp \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
EOF

# Build Docker image
echo "Building Docker image for ARM64..."
docker buildx build --platform linux/arm64 \
    -t miot-cross-arm64:latest \
    -f Dockerfile.cross-arm64 \
    --load . || {
        echo ""
        echo "❌ Failed to build Docker image"
        echo "Trying to create builder..."
        docker buildx create --name arm64-builder --use 2>/dev/null || true
        docker buildx build --platform linux/arm64 \
            -t miot-cross-arm64:latest \
            -f Dockerfile.cross-arm64 \
            --load .
    }
echo "✅ Docker image built"

# Compile
echo ""
echo "Compiling project for ARM64..."
docker run --rm \
    --platform linux/arm64 \
    -v "$PROJECT_DIR":/workspace \
    -w /workspace \
    miot-cross-arm64:latest \
    bash -c '
        set -e
        
        echo "Cleaning old build..."
        rm -rf build-arm64
        mkdir build-arm64
        cd build-arm64
        
        echo "Running CMake..."
        cmake .. || exit 1
        
        echo "Building..."
        make -j$(nproc) || exit 1
        
        echo ""
        echo "Verifying binaries..."
        for bin in test_first_frame miot_discovery_with_cloud miot_lan_discovery_demo; do
            if [ -f "$bin" ]; then
                echo "  ✅ $bin: $(file $bin | grep -o "ELF.*")"
            fi
        done
        
        echo ""
        echo "Binary sizes:"
        ls -lh test_first_frame miot_discovery_with_cloud miot_lan_discovery_demo 2>/dev/null || true
    ' || {
        echo "❌ Compilation failed"
        exit 1
    }

echo ""
echo "=================================================="
echo "  ✅ Build Complete!"
echo "=================================================="
echo ""
echo "Built binaries in: build-arm64/"
echo ""
echo "Next steps:"
echo ""
echo "1. Create deployment package:"
echo "   tar czf miot-camera-arm64.tar.gz \\"
echo "       build-arm64/test_first_frame \\"
echo "       build-arm64/miot_discovery_with_cloud \\"
echo "       libs/linux/arm64/libmiot_camera_lite.so \\"
echo "       token.txt"
echo ""
echo "2. Copy to your ARM64 device:"
echo "   scp miot-camera-arm64.tar.gz user@your-device:~/"
echo ""
echo "3. On the ARM64 device:"
echo "   tar xzf miot-camera-arm64.tar.gz"
echo "   export LD_LIBRARY_PATH=\$PWD/libs/linux/arm64:\$LD_LIBRARY_PATH"
echo "   ./build-arm64/test_first_frame -f token.txt -d DID -m MODEL -p PIN"
echo ""

