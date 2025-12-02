#!/bin/bash
# 在 ARM64 Docker 容器中测试二进制文件

set -e

echo "🚀 Starting ARM64 Docker test environment..."
echo ""

# 检查 Docker
if ! docker info > /dev/null 2>&1; then
    echo "❌ Docker is not running"
    exit 1
fi

# 检查二进制文件是否存在
if [ ! -f "build-arm64/miot_lan_discovery_demo" ]; then
    echo "❌ ARM64 binary not found. Please build first:"
    echo "   ./build-arm64-docker.sh"
    exit 1
fi

# 运行 ARM64 容器
docker run --rm -it \
  --platform linux/arm64 \
  -v "$(pwd)":/workspace \
  -w /workspace/build-arm64 \
  ubuntu:22.04 bash -c '
    echo "📦 Installing runtime dependencies..."
    apt-get update -qq && apt-get install -y -qq \
      libssl3 \
      libcurl4 \
      file \
      > /dev/null 2>&1
    
    echo ""
    echo "✅ ARM64 environment ready!"
    echo ""
    echo "Available binaries:"
    ls -lh miot_* test_* 2>/dev/null || true
    echo ""
    echo "Architecture verification:"
    file miot_lan_discovery_demo
    echo ""
    echo "════════════════════════════════════════"
    echo "  You are now in an ARM64 container"
    echo "════════════════════════════════════════"
    echo ""
    echo "Try running:"
    echo "  ./miot_lan_discovery_demo"
    echo "  ./test_first_frame"
    echo ""
    
    # 启动交互式 shell
    exec bash
'