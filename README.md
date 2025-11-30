# MIoT LAN Device Discovery

一个用C++实现的小米IoT设备局域网发现工具，基于小米OTU（One Touch）协议。

## 功能特性

- ✅ **自动发现**: 自动发现局域网内的小米IoT设备
- ✅ **多网卡支持**: 支持指定多个网络接口进行扫描
- ✅ **设备状态监控**: 实时监控设备上线/下线状态
- ✅ **回调机制**: 支持注册回调函数，接收设备状态变化通知
- ✅ **跨平台**: 支持 Linux、macOS、Windows
- ✅ **线程安全**: 使用互斥锁保护共享数据
- ✅ **智能扫描**: 采用指数退避策略，从5秒到45秒动态调整扫描间隔

## 技术原理

### OTU协议（One Touch）

OTU是小米开发的局域网设备发现协议，基于UDP广播：

- **端口**: 54321
- **协议头**: `0x21 0x31` ("!1")
- **探测消息**: 32字节固定格式
- **响应消息**: 32字节或更长

### 消息格式

**探测消息（Probe Message）**:
```
Offset | Length | Description
-------|--------|-------------
0-1    | 2      | Header: 0x21 0x31 ("!1")
2-3    | 2      | Length: 0x00 0x20 (32 bytes)
4-15   | 12     | Unknown: 0xFF * 12
16-19  | 4      | Magic: "MDID"
20-27  | 8      | Virtual Device ID (big endian)
28-31  | 4      | Padding: 0x00 * 4
```

**响应消息（Response Message）**:
```
Offset | Length | Description
-------|--------|-------------
0-1    | 2      | Header: 0x21 0x31
2-3    | 2      | Length
4-11   | 8      | Real Device ID (DID, big endian)
12-15  | 4      | Timestamp (Unix timestamp, big endian)
16-... | var    | Additional device info (optional)
```

## 编译

### 前置要求

- C++17或更高版本
- CMake 3.10+
- 支持的编译器：
  - GCC 7.0+
  - Clang 5.0+
  - MSVC 2017+

### Linux / macOS

```bash
# 进入项目目录
cd /Users/jiadiy/Workspace/miot_camera_bridge/

# 创建构建目录
mkdir build && cd build

# 生成Makefile
cmake ..

# 编译
make

# 运行
./miot_lan_discovery_demo
```

### 编译选项

```bash
# Debug模式
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release模式（优化）
cmake -DCMAKE_BUILD_TYPE=Release ..

# 安装到系统
sudo make install
```

## 使用方法

### 基础用法

```bash
# 自动检测所有网络接口
./miot_lan_discovery_demo

# 指定网络接口
./miot_lan_discovery_demo -i en0

# 指定多个网络接口
./miot_lan_discovery_demo -i eth0 -i wlan0

# 设置设备超时时间为120秒
./miot_lan_discovery_demo --timeout 120

# 设置扫描间隔（最小10秒，最大60秒）
./miot_lan_discovery_demo --min-interval 10 --max-interval 60

# 指定虚拟设备ID
./miot_lan_discovery_demo --did 123456789012345678
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-i, --interface <name>` | 网络接口名称（可多次指定） | 所有接口 |
| `-d, --did <number>` | 虚拟设备ID | 随机生成 |
| `-t, --timeout <seconds>` | 设备超时时间（秒） | 100 |
| `--min-interval <secs>` | 最小扫描间隔（秒） | 5 |
| `--max-interval <secs>` | 最大扫描间隔（秒） | 45 |
| `-h, --help` | 显示帮助信息 | - |

## 编程接口

### 示例代码

```cpp
#include "miot_lan_device.h"
#include <iostream>

// 设备状态变化回调
void on_device_changed(const std::string& did, const miot::DeviceInfo& info) {
    std::cout << "Device " << did << " is " 
              << (info.online ? "ONLINE" : "OFFLINE") 
              << " at " << info.ip << std::endl;
}

int main() {
    // 创建发现器实例（扫描所有接口）
    miot::MIoTLanDiscovery discovery;
    
    // 或者指定接口
    // miot::MIoTLanDiscovery discovery({"en0", "en1"});
    
    // 配置参数
    discovery.set_device_timeout(120.0);
    discovery.set_scan_intervals(5.0, 45.0);
    
    // 注册回调
    discovery.register_callback("my_callback", on_device_changed);
    
    // 启动发现
    if (!discovery.start()) {
        std::cerr << "Failed to start discovery" << std::endl;
        return 1;
    }
    
    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    // 获取所有设备
    auto devices = discovery.get_devices();
    for (const auto& pair : devices) {
        const auto& dev = pair.second;
        std::cout << "DID: " << dev.did 
                  << ", IP: " << dev.ip 
                  << ", Status: " << (dev.online ? "Online" : "Offline")
                  << std::endl;
    }
    
    // 手动触发一次扫描
    discovery.ping();
    
    // 停止发现
    discovery.stop();
    
    return 0;
}
```

### API参考

#### 构造函数

```cpp
MIoTLanDiscovery(
    const std::vector<std::string>& interfaces = {},
    uint64_t virtual_did = 0
)
```

- `interfaces`: 要扫描的网络接口列表，空则自动检测
- `virtual_did`: 虚拟设备ID，0则随机生成

#### 主要方法

```cpp
// 启动设备发现
bool start();

// 停止设备发现
void stop();

// 检查是否正在运行
bool is_running() const;

// 手动发送探测消息
void ping(const std::string& interface_name = "", const std::string& target_ip = "");

// 获取所有设备
std::map<std::string, DeviceInfo> get_devices() const;

// 获取指定设备
std::shared_ptr<DeviceInfo> get_device(const std::string& did) const;

// 注册回调
void register_callback(const std::string& key, DeviceStatusCallback callback);

// 注销回调
void unregister_callback(const std::string& key);

// 设置扫描间隔
void set_scan_intervals(double min_interval, double max_interval);

// 设置设备超时
void set_device_timeout(double timeout);
```

#### DeviceInfo结构

```cpp
struct DeviceInfo {
    std::string did;           // 设备ID
    std::string ip;            // IP地址
    std::string interface;     // 网络接口
    bool online;               // 在线状态
    int64_t timestamp_offset;  // 时间偏移
    std::chrono::steady_clock::time_point last_seen; // 最后发现时间
};
```

## 常见问题

### 1. 没有发现任何设备？

**检查项：**
- 确保设备和电脑在同一局域网
- 确保防火墙没有阻止UDP 54321端口
- 尝试指定网络接口：`-i en0` 或 `-i eth0`
- 确保设备已开机且连接到网络

### 2. 编译错误：找不到网络相关头文件

**Linux**: 安装开发工具
```bash
sudo apt-get install build-essential
```

**macOS**: 安装Xcode命令行工具
```bash
xcode-select --install
```

### 3. 权限错误

某些系统可能需要管理员权限才能绑定到特定网络接口：

```bash
# Linux/macOS
sudo ./miot_lan_discovery_demo

# 或者添加capability（Linux）
sudo setcap cap_net_raw,cap_net_admin=eip ./miot_lan_discovery_demo
```

### 4. 如何查看网络接口名称？

**Linux**:
```bash
ip addr show
# 或
ifconfig
```

**macOS**:
```bash
ifconfig
# 常见接口：en0（Wi-Fi）, en1（以太网）
```

**Windows**:
```bash
ipconfig
```

## 项目结构

```
miot_camera_bridge/
├── CMakeLists.txt          # CMake构建配置
├── miot_lan_device.h       # 头文件
├── miot_lan_device.cpp     # 实现文件
├── main.cpp                # 示例程序
├── README.md               # 本文档
└── build/                  # 构建目录（生成）
    ├── miot_lan_discovery_demo  # 可执行文件
    └── libmiot_lan_device.a     # 静态库
```

## 技术细节

### 线程模型

- **主线程**: 用户应用程序
- **发现线程**: 周期性发送探测消息并接收响应
- **超时检查线程**: 周期性检查设备是否超时离线

### 扫描策略

采用**指数退避**策略：
1. 首次扫描：5秒间隔
2. 第二次扫描：10秒间隔
3. 第三次扫描：20秒间隔
4. 最大间隔：45秒

这样可以在设备启动时快速发现，稳定运行后降低网络负载。

### 设备超时

- 默认超时：100秒
- 如果100秒内未收到设备响应，标记为离线
- 设备重新响应后，自动标记为在线

## 集成到您的项目

### 方式1: 作为库使用

```cmake
# 在您的CMakeLists.txt中
add_subdirectory(path/to/miot_camera_bridge)
target_link_libraries(your_app miot_lan_device)
```

### 方式2: 直接包含源文件

```cmake
add_executable(your_app
    your_app.cpp
    path/to/miot_camera_bridge/miot_lan_device.cpp
)
target_include_directories(your_app PRIVATE
    path/to/miot_camera_bridge
)
```

## 下一步

完成了局域网设备发现后，您可能还需要：

1. **获取设备详细信息**: 调用小米云API获取设备名称、型号等
2. **摄像头连接**: 使用P2P协议连接摄像头并获取视频流
3. **设备控制**: 通过MQTT或HTTP API控制设备

这些功能需要配合之前实现的Token获取功能使用。

## 许可证

MIT License

## 参考资料

- [小米IoT开发者平台](https://iot.mi.com/)
- [原项目：Xiaomi Miloco](https://github.com/MiEcosystem/xiaomi-miloco)

## 贡献

欢迎提交Issue和Pull Request！

---

**注意**: 本工具仅供学习和研究使用，请遵守小米IoT平台的使用条款。
