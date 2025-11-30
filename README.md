# 小米IoT摄像头桥接程序 / Xiaomi IoT Camera Bridge

基于命令行的小米OAuth2认证客户端，实现token获取和自动刷新功能。

A command-line Xiaomi OAuth2 authentication client with automatic token refresh.

## ✨ 特性 / Features

- ✅ 完整的OAuth2授权流程
- ✅ 自动打开浏览器进行授权
- ✅ Token持久化存储（JSON格式）
- ✅ 自动检测token过期并刷新
- ✅ 命令行友好的交互界面
- ✅ 纯C++实现，无需Python依赖
- ✅ 跨平台支持（macOS/Linux）

## 📋 依赖 / Dependencies

- C++17编译器 (GCC 7+ / Clang 5+ / MSVC 2017+)
- CMake 3.10+
- libcurl (HTTP请求)
- nlohmann/json (JSON解析)
- OpenSSL (SHA1哈希)

## 🚀 安装依赖 / Install Dependencies

### macOS
```bash
brew install cmake curl nlohmann-json openssl
```

### Ubuntu/Debian
```bash
sudo apt install cmake libcurl4-openssl-dev nlohmann-json3-dev libssl-dev build-essential
```

### Fedora/RHEL
```bash
sudo dnf install cmake libcurl-devel json-devel openssl-devel gcc-c++
```

## 🔨 编译 / Build

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge
mkdir build && cd build
cmake ..
make
```

## 🎯 使用 / Usage

### 首次运行 / First Run

```bash
./miot_bridge
```

程序会：
1. 自动打开浏览器到小米授权页面
2. 引导您登录小米账号并授权
3. 启动本地HTTP服务器（端口8888）接收OAuth回调
4. 自动保存token到 `miot_token.json`

The program will:
1. Automatically open browser to Xiaomi authorization page
2. Guide you to login and authorize
3. Start local HTTP server (port 8888) to receive OAuth callback
4. Automatically save token to `miot_token.json`

### 后续运行 / Subsequent Runs

程序会：
1. 自动加载已保存的token
2. 检查token有效性
3. 必要时自动刷新token
4. 进入主循环，持续监控token状态

The program will:
1. Automatically load saved token
2. Check token validity
3. Auto-refresh if needed
4. Enter main loop to monitor token status

## 📁 Token文件格式 / Token File Format

`miot_token.json` 包含：

```json
{
    "access_token": "AgH7Yk...",
    "refresh_token": "AgGt3X...",
    "expires_at": 1735564800
}
```

- `access_token`: 访问令牌，用于API调用
- `refresh_token`: 刷新令牌，用于获取新的access_token
- `expires_at`: Unix时间戳，token过期时间

## 🔌 集成到您的项目 / Integration

### 方法1: 作为独立进程运行

运行 `miot_bridge` 并从 `miot_token.json` 读取token：

```cpp
#include <nlohmann/json.hpp>
#include <fstream>

std::string load_access_token() {
    std::ifstream file("miot_token.json");
    nlohmann::json token_json;
    file >> token_json;
    return token_json["access_token"];
}

// 使用token
std::string token = load_access_token();
miot_camera_init(..., token.c_str());
```

### 方法2: 集成OAuth库到您的代码

```cpp
#include "miot_oauth.h"

// 创建OAuth客户端
miot::MiotOAuth oauth(CLIENT_ID, REDIRECT_URI);

// 加载token
if (!oauth.init("miot_token.json")) {
    // 需要重新授权
    // ... 启动授权流程
}

// 获取access_token
std::string token = oauth.get_token().access_token;

// 使用token初始化摄像头库
miot_camera_init("mico.api.mijia.tech", CLIENT_ID, token.c_str());
```

## 🔄 自动刷新机制 / Auto-Refresh

程序会：
- 每分钟检查一次token状态
- 在token过期前10分钟自动刷新
- 刷新后自动保存到文件
- 失败时提示用户重新授权

The program will:
- Check token status every minute
- Auto-refresh 10 minutes before expiration
- Save automatically after refresh
- Prompt for re-authentication if refresh fails

## 📝 配置说明 / Configuration

可在 `src/main.cpp` 中修改以下常量：

```cpp
const std::string CLIENT_ID = "2882303761520431603";  // 小米OAuth2客户端ID
const std::string REDIRECT_URI = "http://localhost:8888/callback";  // 回调地址
const std::string CLOUD_SERVER = "cn";  // 服务器区域（cn/us/sg等）
const std::string TOKEN_FILE = "miot_token.json";  // Token文件路径
```

## 🐛 故障排除 / Troubleshooting

### 端口8888被占用

修改 `src/main.cpp` 中的端口号：
```cpp
miot::SimpleHttpServer server(9999);  // 改为其他端口
```

同时修改 REDIRECT_URI：
```cpp
const std::string REDIRECT_URI = "http://localhost:9999/callback";
```

### 浏览器未自动打开

手动复制URL到浏览器打开。

### Token刷新失败

1. 检查网络连接
2. 确认refresh_token未过期
3. 重新运行程序进行授权

## 📄 许可证 / License

遵循原项目许可证（Xiaomi Miloco License Agreement）

## 🙏 致谢 / Acknowledgments

基于 [xiaomi-miloco](https://github.com/XiaoMi/xiaomi-miloco) 项目开发。

Based on the [xiaomi-miloco](https://github.com/XiaoMi/xiaomi-miloco) project.

