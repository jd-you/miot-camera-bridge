# macOS交叉编译到Linux ARM64指南

本指南说明如何在macOS上交叉编译出可在Linux ARM64设备上运行的可执行文件。

## 🎯 目标平台

- **源平台**: macOS (x86_64 或 ARM64)
- **目标平台**: Linux ARM64 (aarch64)
- **适用设备**: 
  - Raspberry Pi 3/4/5
  - NVIDIA Jetson (Nano/TX2/Xavier/Orin)
  - AWS Graviton
  - 其他ARM64 Linux设备

## 📦 方法一：使用Docker（推荐）

这是最简单可靠的方法，使用Docker容器进行交叉编译。

### 1. 安装Docker Desktop

```bash
# 从官网下载安装
# https://www.docker.com/products/docker-desktop/

# 或使用Homebrew
brew install --cask docker
```

### 2. 创建Docker交叉编译环境

创建 `Dockerfile.cross-arm64`:

```dockerfile
FROM --platform=linux/arm64 ubuntu:22.04

# 设置非交互模式
ENV DEBIAN_FRONTEND=noninteractive

# 安装基础工具和依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /workspace

CMD ["/bin/bash"]
```

### 3. 构建Docker镜像

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge

# 构建ARM64镜像
docker buildx create --name arm64-builder --use
docker buildx build --platform linux/arm64 \
    -t miot-cross-arm64:latest \
    -f Dockerfile.cross-arm64 \
    --load .
```

### 4. 在Docker中编译

```bash
# 启动容器并挂载源码
docker run --rm -it \
    --platform linux/arm64 \
    -v "$(pwd)":/workspace \
    -w /workspace \
    miot-cross-arm64:latest \
    bash

# 在容器内编译
mkdir -p build-arm64
cd build-arm64
cmake ..
make -j$(nproc)

# 查看生成的文件
file test_first_frame
# 输出: test_first_frame: ELF 64-bit LSB executable, ARM aarch64, ...
```

### 5. 复制到目标设备

```bash
# 退出容器后，在macOS上执行
scp build-arm64/test_first_frame user@your-arm-device:/path/to/destination
scp build-arm64/miot_discovery_with_cloud user@your-arm-device:/path/to/destination
```

## 📦 方法二：使用QEMU + Cross Compiler

这种方法更传统，但配置较复杂。

### 1. 安装交叉编译工具链

```bash
# 安装QEMU（用于测试）
brew install qemu

# 下载aarch64交叉编译器
# 从 https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# 下载: AArch64 GNU/Linux target (aarch64-none-linux-gnu)

# 或使用预编译版本
cd ~/Downloads
curl -LO https://developer.arm.com/-/media/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-darwin-x86_64-aarch64-none-linux-gnu.tar.xz

# 解压
tar xf arm-gnu-toolchain-12.2.rel1-darwin-x86_64-aarch64-none-linux-gnu.tar.xz
sudo mv arm-gnu-toolchain-12.2.rel1-darwin-x86_64-aarch64-none-linux-gnu /opt/arm-gnu-toolchain

# 添加到PATH
export PATH="/opt/arm-gnu-toolchain/bin:$PATH"
```

### 2. 准备sysroot

您需要从目标Linux系统获取库文件：

```bash
# 在目标ARM64设备上
mkdir -p ~/sysroot
cd ~/sysroot
tar czf sysroot.tar.gz \
    /lib/aarch64-linux-gnu \
    /usr/lib/aarch64-linux-gnu \
    /usr/include

# 传输到Mac
scp sysroot.tar.gz your-mac:~/
```

```bash
# 在Mac上
mkdir -p ~/arm64-sysroot
cd ~/arm64-sysroot
tar xzf ~/sysroot.tar.gz
```

### 3. 创建交叉编译CMake工具链文件

创建 `toolchain-aarch64.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 交叉编译器路径
set(CMAKE_C_COMPILER /opt/arm-gnu-toolchain/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /opt/arm-gnu-toolchain/bin/aarch64-none-linux-gnu-g++)

# sysroot路径
set(CMAKE_SYSROOT $ENV{HOME}/arm64-sysroot)

# 搜索路径设置
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 设置编译标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --sysroot=${CMAKE_SYSROOT}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --sysroot=${CMAKE_SYSROOT}")
```

### 4. 使用工具链编译

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge
mkdir build-arm64
cd build-arm64

cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake ..
make -j4
```

## 📦 方法三：使用GitHub Actions（自动化）

创建 `.github/workflows/build-arm64.yml`:

```yaml
name: Build ARM64

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  build-arm64:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up QEMU
      uses: docker/setup-qemu-action@v2
      with:
        platforms: arm64
    
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v2
    
    - name: Build in ARM64 container
      run: |
        docker run --rm --platform linux/arm64 \
          -v ${{ github.workspace }}:/workspace \
          -w /workspace \
          ubuntu:22.04 \
          bash -c "
            apt-get update && \
            apt-get install -y build-essential cmake libssl-dev libcurl4-openssl-dev && \
            mkdir build && cd build && \
            cmake .. && \
            make -j$(nproc)
          "
    
    - name: Upload artifacts
      uses: actions/upload-artifact@v3
      with:
        name: arm64-binaries
        path: build/test_first_frame
```

## 🔧 处理依赖库

### OpenSSL

```bash
# 在Docker容器内
apt-get install -y libssl-dev

# 或在ARM64设备上编译
git clone https://github.com/openssl/openssl.git
cd openssl
./Configure linux-aarch64 --prefix=/usr/local
make -j4
sudo make install
```

### libcurl

```bash
# 在Docker容器内
apt-get install -y libcurl4-openssl-dev

# 或从源码编译
git clone https://github.com/curl/curl.git
cd curl
./buildconf
./configure --host=aarch64-linux-gnu
make -j4
sudo make install
```

## 📝 完整的Docker编译脚本

创建 `build-arm64-docker.sh`:

```bash
#!/bin/bash
set -e

echo "=================================================="
echo "  Building for Linux ARM64 using Docker"
echo "=================================================="
echo ""

# 检查Docker是否运行
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker is not running"
    exit 1
fi

PROJECT_DIR="/Users/jiadiy/Workspace/miot_camera_bridge"
cd "$PROJECT_DIR"

# 创建Dockerfile
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
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
EOF

echo "Building Docker image..."
docker buildx build --platform linux/arm64 \
    -t miot-cross-arm64:latest \
    -f Dockerfile.cross-arm64 \
    --load .

echo ""
echo "Compiling project..."
docker run --rm \
    --platform linux/arm64 \
    -v "$PROJECT_DIR":/workspace \
    -w /workspace \
    miot-cross-arm64:latest \
    bash -c "
        set -e
        
        # 检查库文件
        echo 'Checking for libmiot_camera_lite...'
        if [ ! -d 'libs/linux/arm64' ]; then
            echo 'Error: libs/linux/arm64 not found'
            echo 'Please copy the ARM64 library first'
            exit 1
        fi
        
        # 清理旧构建
        rm -rf build-arm64
        mkdir build-arm64
        cd build-arm64
        
        # 配置
        echo 'Running CMake...'
        cmake ..
        
        # 编译
        echo 'Building...'
        make -j\$(nproc)
        
        # 验证
        echo ''
        echo 'Built binaries:'
        file test_first_frame miot_discovery_with_cloud 2>/dev/null | grep -o 'ELF.*' || echo 'No binaries found'
        
        echo ''
        echo 'Binary sizes:'
        ls -lh test_first_frame miot_discovery_with_cloud 2>/dev/null || true
    "

echo ""
echo "=================================================="
echo "  Build Complete!"
echo "=================================================="
echo ""
echo "Binaries location: build-arm64/"
echo ""
echo "To copy to your ARM64 device:"
echo "  scp build-arm64/test_first_frame user@device:/path/"
echo "  scp build-arm64/miot_discovery_with_cloud user@device:/path/"
echo ""
echo "Don't forget to also copy:"
echo "  - token.txt"
echo "  - libs/linux/arm64/libmiot_camera_lite.so"
echo ""
```

赋予执行权限：

```bash
chmod +x build-arm64-docker.sh
```

运行：

```bash
./build-arm64-docker.sh
```

## ✅ 验证编译结果

```bash
# 检查文件类型
file build-arm64/test_first_frame

# 应该输出类似:
# test_first_frame: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), dynamically linked, ...

# 检查依赖
docker run --rm --platform linux/arm64 \
    -v "$(pwd)/build-arm64":/bin \
    ubuntu:22.04 \
    ldd /bin/test_first_frame
```

## 📤 部署到目标设备

### 1. 复制文件

```bash
# 创建部署包
cd /Users/jiadiy/Workspace/miot_camera_bridge
tar czf miot-camera-arm64.tar.gz \
    build-arm64/test_first_frame \
    build-arm64/miot_discovery_with_cloud \
    libs/linux/arm64/libmiot_camera_lite.so \
    token.txt

# 传输到ARM64设备
scp miot-camera-arm64.tar.gz user@your-device:~/
```

### 2. 在目标设备上解压运行

```bash
# 在ARM64设备上
cd ~
tar xzf miot-camera-arm64.tar.gz

# 设置库路径
export LD_LIBRARY_PATH=$PWD/libs/linux/arm64:$LD_LIBRARY_PATH

# 运行
./build-arm64/test_first_frame \
    -f token.txt \
    -d YOUR_DID \
    -m YOUR_MODEL \
    -p YOUR_PIN
```

## 🐛 常见问题

### 1. Docker buildx不支持ARM64

```bash
# 安装buildx
docker buildx install

# 创建builder
docker buildx create --name multiarch --use
docker buildx inspect --bootstrap
```

### 2. 找不到libmiot_camera_lite.so

确保已经复制了ARM64版本的库：

```bash
cp /Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/libs/linux/arm64/libmiot_camera_lite.so \
   /Users/jiadiy/Workspace/miot_camera_bridge/libs/linux/arm64/
```

### 3. 在目标设备上运行时找不到库

```bash
# 方法1: 设置LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/libs/linux/arm64:$LD_LIBRARY_PATH

# 方法2: 复制到系统路径
sudo cp libs/linux/arm64/libmiot_camera_lite.so /usr/local/lib/
sudo ldconfig
```

## 📊 性能对比

| 平台 | 编译时间 | 可执行文件大小 | 运行性能 |
|------|---------|--------------|---------|
| Native ARM64 | 基准 | 基准 | 最佳 |
| Docker交叉编译 | 1.2x | 相同 | 相同 |
| QEMU模拟 | 3-5x | 相同 | 相同 |

## 🎯 推荐方案

对于您的场景，推荐使用**Docker方法**：

1. ✅ 简单易用
2. ✅ 不需要配置复杂的工具链
3. ✅ 依赖管理简单
4. ✅ 可重复构建
5. ✅ 适合CI/CD

```bash
# 一键编译
./build-arm64-docker.sh
```

---

**需要帮助？** 查看Docker官方文档或提交Issue。

