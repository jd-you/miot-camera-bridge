#!/bin/bash

# MIoT LAN Discovery - Build Script
# Copyright (C) 2025

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "╔════════════════════════════════════════════════════════════════════════╗"
echo "║         MIoT LAN Device Discovery - Build Script                       ║"
echo "╚════════════════════════════════════════════════════════════════════════╝"
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: CMake is not installed${NC}"
    echo "Please install CMake first:"
    echo "  macOS:  brew install cmake"
    echo "  Linux:  sudo apt-get install cmake"
    exit 1
fi

# Check for compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}ERROR: No C++ compiler found${NC}"
    echo "Please install a C++ compiler first:"
    echo "  macOS:  xcode-select --install"
    echo "  Linux:  sudo apt-get install build-essential"
    exit 1
fi

# Parse arguments
BUILD_TYPE="Release"
CLEAN_BUILD=false
INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  -c, --clean     Clean build directory before building"
            echo "  -i, --install   Install after building"
            echo "  -h, --help      Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                    # Release build"
            echo "  $0 -d                 # Debug build"
            echo "  $0 -c -d              # Clean Debug build"
            echo "  $0 --install          # Build and install"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Create or clean build directory
if [ "$CLEAN_BUILD" = true ] && [ -d "build" ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

if [ ! -d "build" ]; then
    echo -e "${GREEN}Creating build directory...${NC}"
    mkdir build
fi

cd build

# Configure
echo ""
echo -e "${GREEN}Configuring project (${BUILD_TYPE} mode)...${NC}"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Build
echo ""
echo -e "${GREEN}Building project...${NC}"
cmake --build . -- -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}✓ Build succeeded!${NC}"
    echo ""
    echo "Executable location:"
    echo "  $(pwd)/miot_lan_discovery_demo"
    echo ""
    
    # Show binary size
    if command -v ls &> /dev/null; then
        echo "Binary size:"
        ls -lh miot_lan_discovery_demo | awk '{print "  " $5}'
        echo ""
    fi
    
    # Install if requested
    if [ "$INSTALL" = true ]; then
        echo -e "${YELLOW}Installing...${NC}"
        if [ "$(uname)" == "Darwin" ] || [ "$(id -u)" -ne 0 ]; then
            sudo cmake --install .
        else
            cmake --install .
        fi
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ Installation succeeded!${NC}"
        else
            echo -e "${RED}✗ Installation failed${NC}"
            exit 1
        fi
    fi
    
    echo ""
    echo "To run the demo:"
    echo "  cd $(pwd)"
    echo "  ./miot_lan_discovery_demo"
    echo ""
    echo "Or with options:"
    echo "  ./miot_lan_discovery_demo -i en0"
    echo "  ./miot_lan_discovery_demo --help"
    echo ""
else
    echo ""
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

