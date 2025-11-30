# 🚀 快速编译和运行指南

## ⚠️ 重要提示

由于集成了小米云API客户端，现在项目需要额外的依赖库。

## 📦 安装依赖

### macOS

```bash
# 安装依赖
brew install cmake openssl curl

# 设置OpenSSL路径（如果CMake找不到）
export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
export OPENSSL_INCLUDE_DIR=$(brew --prefix openssl)/include
export OPENSSL_CRYPTO_LIBRARY=$(brew --prefix openssl)/lib/libcrypto.dylib
export OPENSSL_SSL_LIBRARY=$(brew --prefix openssl)/lib/libssl.dylib
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y cmake libssl-dev libcurl4-openssl-dev build-essential
```

### Linux (CentOS/RHEL)

```bash
sudo yum install -y cmake openssl-devel libcurl-devel gcc-c++
```

## 🔨 编译项目

### 方式1: 使用构建脚本（推荐）

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge
chmod +x build.sh
./build.sh
```

### 方式2: 手动编译

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge

# 创建构建目录
mkdir -p build
cd build

# 配置项目
cmake ..

# 如果找不到OpenSSL，使用：
# cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) ..

# 编译
make -j4

# 查看生成的可执行文件
ls -lh miot_*
```

## 📝 准备Access Token

在运行完整版之前，需要准备access_token：

```bash
# 创建token文件
echo "your_access_token_here" > /Users/jiadiy/Workspace/miot_camera_bridge/token.txt

# 注意：将 your_access_token_here 替换为真实的token
```

## 🎯 运行程序

### 选项1: 运行完整版（推荐）

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge/build

# 使用token文件
./miot_discovery_with_cloud -f ../token.txt

# 指定网络接口
./miot_discovery_with_cloud -f ../token.txt -i en0

# 直接指定token（不推荐，命令行历史会保存token）
./miot_discovery_with_cloud -t "your_token" -i en0
```

### 选项2: 运行基础版（不需要token）

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge/build

# 自动检测所有网络接口
./miot_lan_discovery_demo

# 指定网络接口
./miot_lan_discovery_demo -i en0
```

## 🐛 常见问题

### 1. CMake找不到OpenSSL

**错误信息**: `Could NOT find OpenSSL`

**解决方案**:
```bash
# macOS
export OPENSSL_ROOT_DIR=$(brew --prefix openssl)
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) ..

# 或者重新安装OpenSSL
brew reinstall openssl
```

### 2. CMake找不到CURL

**错误信息**: `Could NOT find CURL`

**解决方案**:
```bash
# macOS
brew install curl

# Linux
sudo apt-get install libcurl4-openssl-dev
```

### 3. 编译错误：undefined reference to OpenSSL functions

**解决方案**:
```bash
# 清理并重新编译
cd /Users/jiadiy/Workspace/miot_camera_bridge
rm -rf build
mkdir build && cd build
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) ..
make -j4
```

### 4. 运行时错误：dyld: Library not loaded

**错误信息**: 找不到libssl或libcrypto

**解决方案**:
```bash
# macOS - 设置库路径
export DYLD_LIBRARY_PATH=$(brew --prefix openssl)/lib:$DYLD_LIBRARY_PATH

# 或者重新链接
install_name_tool -change @rpath/libssl.1.1.dylib $(brew --prefix openssl)/lib/libssl.1.1.dylib ./miot_discovery_with_cloud
```

### 5. Access token无效

**错误信息**: `HTTP error code: 401` 或 `Invalid access token`

**解决方案**:
- 检查token是否正确（无多余空格）
- Token可能已过期（通常30天有效期）
- 确认云服务器区域正确（-s cn/de/us）

## 📊 验证编译成功

编译成功后，应该看到以下可执行文件：

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge/build
ls -lh

# 应该显示：
# miot_lan_discovery_demo        - 基础版（仅LAN发现）
# miot_discovery_with_cloud      - 完整版（LAN + 云API）
# libmiot_lan_device.a          - LAN发现库
# libmiot_cloud_client.a        - 云API库
```

## 🎓 测试步骤

### 1. 测试基础版（不需要token）

```bash
cd build
./miot_lan_discovery_demo -i en0

# 应该能看到局域网内的小米设备
# 按Ctrl+C停止
```

### 2. 测试完整版（需要token）

```bash
cd build
./miot_discovery_with_cloud -f ../token.txt -i en0

# 应该能看到设备的详细信息，包括：
# - 用户自定义的设备名称
# - 设备型号
# - IP地址
# - WiFi信号强度
# - 固件版本
# 等等
```

## 📖 更多信息

- 基础功能说明: [README.md](README.md)
- 云API功能说明: [README_CLOUD.md](README_CLOUD.md)
- 快速开始指南: [QUICKSTART.md](QUICKSTART.md)

## 🔗 下一步

1. ✅ 成功编译并运行程序
2. ✅ 验证能发现局域网设备
3. ✅ 验证能获取云端设备信息
4. 🔲 集成摄像头视频流功能
5. 🔲 实现设备控制功能

---

**需要帮助?** 请查看 [README_CLOUD.md](README_CLOUD.md) 的故障排查部分。

