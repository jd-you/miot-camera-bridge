# 快速开始指南 - MIoT LAN设备发现器

## 🚀 5分钟快速上手

### 步骤1: 编译项目

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge
chmod +x build.sh
./build.sh
```

编译成功后，您会看到：
```
✓ Build succeeded!
Executable location: /Users/jiadiy/Workspace/miot_camera_bridge/build/miot_lan_discovery_demo
```

### 步骤2: 运行设备扫描

```bash
cd build
./miot_lan_discovery_demo
```

或者指定网络接口：
```bash
./miot_lan_discovery_demo -i en0    # macOS Wi-Fi
./miot_lan_discovery_demo -i eth0   # Linux以太网
```

### 步骤3: 等待设备发现

程序会自动扫描局域网内的小米设备，输出类似：

```
╔════════════════════════════════════════════════════════════════════════╗
║                    Discovered MIoT Devices                              ║
╠════════════════════════════════════════════════════════════════════════╣
║ Status │ Device ID          │ IP Address       │ Interface           ║
╠════════════════════════════════════════════════════════════════════════╣
║ 🟢 ON  │ 123456789012345678 │ 192.168.1.100   │ en0                 ║
║ 🟢 ON  │ 234567890123456789 │ 192.168.1.101   │ en0                 ║
╚════════════════════════════════════════════════════════════════════════╝
```

### 常见问题

#### Q1: 没有发现任何设备？

1. **检查网络连接**: 确保电脑和设备在同一个局域网
2. **检查防火墙**: 确保UDP 54321端口没有被阻止
3. **指定正确的网络接口**:
   ```bash
   # macOS: 查看网络接口
   ifconfig
   
   # 常见接口：
   # en0 - Wi-Fi
   # en1 - 以太网（雷雳/USB）
   
   # 然后指定接口运行
   ./miot_lan_discovery_demo -i en0
   ```

4. **确保设备已开机**: 小米设备需要处于开机状态且连接到网络

#### Q2: 权限错误？

某些系统可能需要管理员权限：

```bash
sudo ./miot_lan_discovery_demo
```

#### Q3: 如何查看我的网络接口？

**macOS**:
```bash
ifconfig | grep -E "^[a-z]"
```

**Linux**:
```bash
ip addr show
```

输出示例：
```
en0: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500
en1: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500
```

### 高级用法

#### 调整扫描参数

```bash
# 设置更长的设备超时（120秒）
./miot_lan_discovery_demo --timeout 120

# 调整扫描间隔（最小10秒，最大60秒）
./miot_lan_discovery_demo --min-interval 10 --max-interval 60

# 同时使用多个参数
./miot_lan_discovery_demo -i en0 --timeout 120 --min-interval 10
```

#### 扫描特定网络

```bash
# 扫描多个接口
./miot_lan_discovery_demo -i en0 -i en1
```

## 📖 工作原理

这个工具使用**OTU（One Touch）协议**来发现小米IoT设备：

1. **广播探测消息**: 向局域网广播UDP包（端口54321）
2. **接收设备响应**: 设备收到后会回复包含DID、IP等信息
3. **维护设备列表**: 持续监控设备状态（在线/离线）
4. **智能扫描**: 采用指数退避策略，从5秒到45秒动态调整

## 🎯 下一步

成功发现设备后，您可以：

1. **记录设备DID**: 这是访问设备的唯一标识
2. **记录设备IP**: 用于后续直连访问
3. **结合Token**: 使用之前获取的access_token访问设备详细信息
4. **连接摄像头**: 使用DID和IP建立P2P连接获取视频流

## 💡 提示

- 第一次扫描可能需要5-10秒
- 设备会在100秒无响应后标记为离线
- 按 `Ctrl+C` 停止扫描
- 程序会每10秒更新一次设备列表

## 🔗 相关文档

- [完整文档](README.md) - 详细API和使用说明
- [项目概览](PROJECT_README.md) - 整体架构和开发路线图

## 📞 需要帮助？

如果遇到问题：

1. 查看 [README.md](README.md) 的"常见问题"部分
2. 确认设备和电脑在同一网络
3. 检查防火墙设置
4. 尝试使用 `sudo` 运行（某些系统需要）

---

**祝您使用愉快！** 🎉

