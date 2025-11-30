# MIoT Camera Bridge - 完整的小米IoT摄像头C++客户端

一个功能完整的C++实现，用于访问小米IoT摄像头并获取实时视频流。

## 🎉 项目特点

- ✅ **完整的设备发现**: 局域网OTU协议 + 云端API查询
- ✅ **安全认证**: OAuth2 + RSA+AES双重加密
- ✅ **视频流获取**: 原始H264/H265帧数据
- ✅ **跨平台支持**: macOS/Linux/Windows
- ✅ **零外部依赖**: 仅需OpenSSL、libcurl和系统库

## 🚀 快速开始

### 一键启动

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge
./quick_start_camera.sh
```

### 手动步骤

```bash
# 1. 编译
mkdir build && cd build
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) ..
make -j4

# 2. 获取设备信息
./miot_discovery_with_cloud -f ../token.txt -i en0

# 3. 获取视频帧
./test_first_frame \
    -f ../token.txt \
    -d YOUR_DEVICE_ID \
    -m YOUR_MODEL \
    -p YOUR_PIN_CODE
```

## 📦 功能模块

### 1️⃣ 局域网设备发现 (LAN Discovery)

使用小米OTU协议自动发现局域网内的IoT设备。

**核心文件**: `miot_lan_device.h/.cpp`

**功能**:
- UDP广播探测
- 设备在线监控
- 多网卡支持
- 自动重连

**使用**:
```bash
./miot_lan_discovery_demo -i en0
```

📖 详细文档: [README.md](README.md) | [QUICKSTART.md](QUICKSTART.md)

### 2️⃣ 云端设备信息 (Cloud API)

通过小米云API获取设备详细信息（名称、型号、Token等）。

**核心文件**: `miot_cloud_client.h/.cpp`

**功能**:
- RSA+AES加密通信
- OAuth2 Bearer认证
- 批量设备查询
- 完整设备信息

**使用**:
```bash
./miot_discovery_with_cloud -f token.txt -i en0
```

📖 详细文档: [README_CLOUD.md](README_CLOUD.md) | [BUILD_GUIDE.md](BUILD_GUIDE.md)

### 3️⃣ 摄像头视频流 (Camera Client)

连接摄像头并获取原始H264/H265视频帧。

**核心文件**: `miot_camera_client.h/.cpp`

**功能**:
- 动态加载libmiot_camera_lite
- P2P连接管理
- 原始视频/音频帧
- 实时状态监控

**使用**:
```bash
./test_first_frame \
    -f token.txt \
    -d 123456789... \
    -m xiaomi.camera.082ac1 \
    -p 1234
```

📖 详细文档: [CAMERA_TEST_GUIDE.md](CAMERA_TEST_GUIDE.md) | [CAMERA_IMPLEMENTATION.md](CAMERA_IMPLEMENTATION.md)

## 📊 完整流程

```
┌─────────────────────────────────────────────────────────┐
│  1. 局域网发现 (miot_lan_discovery_demo)                │
│     UDP广播 → 获取DID和IP                               │
└────────────────┬────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────────┐
│  2. 云端查询 (miot_discovery_with_cloud)                │
│     RSA+AES加密 → 获取Model、Token、Name等              │
└────────────────┬────────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────────┐
│  3. 摄像头连接 (test_first_frame)                       │
│     DID + Model + Token → P2P连接 → 视频流              │
└─────────────────────────────────────────────────────────┘
```

## 📁 项目结构

```
miot_camera_bridge/
├── 📂 libs/                        # 动态库
│   └── darwin/
│       ├── arm64/libmiot_camera_lite.dylib
│       └── x86_64/libmiot_camera_lite.dylib
│
├── 🔧 核心库
│   ├── miot_lan_device.h/.cpp      # LAN发现
│   ├── miot_cloud_client.h/.cpp    # 云API
│   └── miot_camera_client.h/.cpp   # 摄像头客户端
│
├── 🎮 可执行程序
│   ├── main.cpp                    # LAN发现demo
│   ├── main_with_cloud.cpp         # LAN+Cloud集成
│   └── test_first_frame.cpp        # 摄像头测试
│
├── 📚 文档
│   ├── README.md                   # 本文件
│   ├── QUICKSTART.md               # 快速开始
│   ├── BUILD_GUIDE.md              # 编译指南
│   ├── README_CLOUD.md             # 云API文档
│   ├── CAMERA_TEST_GUIDE.md        # 摄像头测试
│   └── CAMERA_IMPLEMENTATION.md    # 实现细节
│
└── 🔨 构建
    ├── CMakeLists.txt              # CMake配置
    ├── build.sh                    # 构建脚本
    ├── compile_test.sh             # 编译测试
    └── quick_start_camera.sh       # 快速启动
```

## 🛠️ 系统要求

### 依赖

```bash
# macOS
brew install cmake openssl curl

# Ubuntu/Debian
sudo apt-get install cmake libssl-dev libcurl4-openssl-dev

# CentOS/RHEL
sudo yum install cmake openssl-devel libcurl-devel
```

### 编译器

- C++17或更高
- GCC 7.0+ / Clang 5.0+ / MSVC 2017+

## 📖 文档导航

### 新手入门
1. 🚀 [QUICKSTART.md](QUICKSTART.md) - 5分钟快速上手
2. 🔨 [BUILD_GUIDE.md](BUILD_GUIDE.md) - 详细编译指南

### 功能文档
3. 📡 [README.md](README.md) - LAN设备发现
4. ☁️ [README_CLOUD.md](README_CLOUD.md) - 云API使用
5. 🎥 [CAMERA_TEST_GUIDE.md](CAMERA_TEST_GUIDE.md) - 摄像头测试

### 技术文档
6. 🔧 [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - 云API实现
7. 📹 [CAMERA_IMPLEMENTATION.md](CAMERA_IMPLEMENTATION.md) - 摄像头实现

## 💡 使用示例

### 场景1: 我想看看局域网有哪些小米设备

```bash
cd build
./miot_lan_discovery_demo -i en0
```

### 场景2: 我想知道设备的详细信息（名称、型号等）

```bash
# 1. 准备token.txt文件
echo "your_access_token" > token.txt

# 2. 运行
cd build
./miot_discovery_with_cloud -f ../token.txt -i en0
```

### 场景3: 我想获取摄像头的视频帧

```bash
# 1. 先获取设备信息（步骤2）
# 2. 记录DID和Model
# 3. 运行摄像头测试
cd build
./test_first_frame \
    -f ../token.txt \
    -d 你的DID \
    -m 你的Model \
    -p 你的PIN码
```

## 🎯 下一步开发

当前状态：✅ 可以获取原始视频帧

可以继续实现：

### 1. 视频解码和显示
```cpp
// 使用FFmpeg解码H264
// 使用OpenCV/SDL显示
// 实时预览窗口
```

### 2. 视频录制
```cpp
// 保存为MP4文件
// 支持多种格式
// 时间戳同步
```

### 3. AI分析
```cpp
// 目标检测（YOLO）
// 人脸识别
// 行为分析
// 异常检测
```

### 4. 流媒体服务
```cpp
// RTSP服务器
// RTMP推流
// WebRTC传输
// HLS切片
```

## 🔒 安全说明

- ✅ 所有云API通信使用RSA+AES加密
- ✅ Access token安全存储（建议使用文件）
- ✅ 支持PIN码保护的摄像头
- ⚠️ 本地保存的帧数据请妥善保管
- ⚠️ 生产环境请加强token管理

## 🐛 问题排查

### 编译问题

查看 [BUILD_GUIDE.md](BUILD_GUIDE.md) 的"常见问题"章节

### LAN发现问题

查看 [QUICKSTART.md](QUICKSTART.md) 的"常见问题"章节

### 云API问题

查看 [README_CLOUD.md](README_CLOUD.md) 的"故障排查"章节

### 摄像头连接问题

查看 [CAMERA_TEST_GUIDE.md](CAMERA_TEST_GUIDE.md) 的"故障排查"章节

## 🤝 贡献

欢迎提交Issue和Pull Request！

## 📄 许可证

MIT License

## 🙏 致谢

- 基于 [Xiaomi Miloco](https://github.com/MiEcosystem/xiaomi-miloco) 项目
- libmiot_camera_lite 由小米提供

## 📮 联系

如有问题，请查看相关文档或提交Issue。

---

**Happy Coding!** 🎉

**项目完成度**:
- ✅ 局域网设备发现 (100%)
- ✅ 云端信息查询 (100%)
- ✅ 摄像头连接 (100%)
- ✅ 原始帧获取 (100%)
- 🔲 视频解码 (待实现)
- 🔲 实时显示 (待实现)

