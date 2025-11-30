# MIoT Camera Bridge

一个用C/C++实现的小米IoT摄像头桥接工具。

## 项目组件

### 1. 局域网设备发现器 ✅

基于小米OTU（One Touch）协议的设备发现工具。

**功能:**
- 自动发现局域网内的小米IoT设备
- 实时监控设备上线/下线状态
- 支持多网卡和跨平台（Linux/macOS/Windows）

**使用:**
```bash
./build.sh
cd build
./miot_lan_discovery_demo
```

详细文档：[README.md](README.md)

### 2. Token获取与刷新模块（待实现）

OAuth2.0授权流程实现，用于获取和自动刷新access_token。

### 3. 摄像头连接模块（待实现）

P2P连接和视频流获取。

## 快速开始

### 1. 克隆或下载项目

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge/
```

### 2. 编译

```bash
chmod +x build.sh
./build.sh
```

### 3. 运行设备发现

```bash
cd build
./miot_lan_discovery_demo -i en0
```

## 系统要求

- C++17或更高版本
- CMake 3.10+
- 支持的操作系统：
  - macOS 10.14+
  - Linux (Ubuntu 18.04+, CentOS 7+)
  - Windows 10+

## 项目结构

```
miot_camera_bridge/
├── miot_lan_device.h           # LAN发现头文件
├── miot_lan_device.cpp         # LAN发现实现
├── main.cpp                    # 示例程序
├── CMakeLists.txt              # CMake配置
├── build.sh                    # 构建脚本
├── README.md                   # LAN发现文档
└── PROJECT_README.md           # 本文件（项目总览）
```

## 开发路线图

- [x] 局域网设备发现（OTU协议）
- [ ] OAuth2.0 Token获取
- [ ] Token自动刷新
- [ ] P2P摄像头连接
- [ ] 视频流解码
- [ ] RTSP/RTMP推流

## 许可证

MIT License

## 参考

基于 [Xiaomi Miloco](https://github.com/MiEcosystem/xiaomi-miloco) 项目的Python实现。

