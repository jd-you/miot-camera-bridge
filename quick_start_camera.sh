#!/bin/bash
# Quick Start Script for Camera Frame Testing

echo "═══════════════════════════════════════════════════════"
echo "  MIoT Camera - First Frame Quick Start"
echo "═══════════════════════════════════════════════════════"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the project root directory"
    echo "  cd /Users/jiadiy/Workspace/miot_camera_bridge"
    exit 1
fi

# Step 1: Check for libs
echo "Step 1: Checking for libmiot_camera_lite..."
if [ ! -d "libs/darwin" ]; then
    echo "❌ Library not found!"
    echo "Copying from original project..."
    if [ -d "/Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/libs/darwin" ]; then
        mkdir -p libs
        cp -r /Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/libs/darwin libs/
        echo "✅ Library copied successfully"
    else
        echo "❌ Source library not found. Please copy manually."
        exit 1
    fi
else
    echo "✅ Library found"
fi

# Step 2: Check for token
echo ""
echo "Step 2: Checking for access token..."
if [ ! -f "token.txt" ]; then
    echo "⚠️  token.txt not found"
    echo "Please create token.txt with your access_token:"
    echo "  echo 'your_access_token_here' > token.txt"
    echo ""
    read -p "Do you want to continue anyway? (y/n) " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "✅ token.txt found"
fi

# Step 3: Build
echo ""
echo "Step 3: Building project..."
if [ -d "build" ]; then
    echo "Cleaning old build..."
    rm -rf build
fi

mkdir build
cd build

echo "Running CMake..."
if cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl 2>/dev/null || echo "/usr/local/opt/openssl") .. > /dev/null 2>&1; then
    echo "✅ CMake configuration successful"
else
    echo "❌ CMake configuration failed"
    echo "Try running: brew install openssl cmake curl"
    exit 1
fi

echo "Compiling..."
if make -j4 > /dev/null 2>&1; then
    echo "✅ Compilation successful"
else
    echo "❌ Compilation failed"
    echo "Check the error messages above"
    exit 1
fi

# Step 4: Show next steps
echo ""
echo "═══════════════════════════════════════════════════════"
echo "  ✅ Setup Complete!"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "Next steps:"
echo ""
echo "1. Get device information:"
echo "   cd build"
echo "   ./miot_discovery_with_cloud -f ../token.txt -i en0"
echo ""
echo "2. Note the DID and Model from the output"
echo ""
echo "3. Run camera test:"
echo "   ./test_first_frame \\"
echo "       -f ../token.txt \\"
echo "       -d YOUR_DEVICE_ID \\"
echo "       -m YOUR_MODEL \\"
echo "       -p YOUR_PIN_CODE"
echo ""
echo "Example:"
echo "   ./test_first_frame \\"
echo "       -f ../token.txt \\"
echo "       -d 123456789012345678 \\"
echo "       -m xiaomi.camera.082ac1 \\"
echo "       -p 1234"
echo ""
echo "For more information, see:"
echo "  - CAMERA_TEST_GUIDE.md"
echo "  - CAMERA_IMPLEMENTATION.md"
echo ""

