# Camera Frame Test Guide

## 🎥 获取摄像头第一帧

现在您可以使用C++获取小米摄像头的原始视频帧了！

## 📝 准备工作

### 1. 获取设备信息

首先运行完整版发现程序获取设备信息：

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge/build
./miot_discovery_with_cloud -f ../token.txt -i en0
```

从输出中记录：
- **DID** (Device ID): 例如 `123456789012345678`
- **Model**: 例如 `xiaomi.camera.082ac1`
- **Channel Count**: 通常是 1 或 2

### 2. 准备 PIN 码（如果需要）

某些摄像头需要4位数字的PIN码。您可以在米家APP中查看或设置。

## 🚀 编译和运行

### 编译项目

```bash
cd /Users/jiadiy/Workspace/miot_camera_bridge

# 清理旧的构建
rm -rf build

# 创建构建目录
mkdir build && cd build

# 配置并编译
cmake -DOPENSSL_ROOT_DIR=$(brew --prefix openssl) ..
make -j4
```

### 运行测试程序

```bash
cd build

# 基础用法（无PIN码）
./test_first_frame \
    -f ../token.txt \
    -d YOUR_DEVICE_ID \
    -m YOUR_MODEL

# 带PIN码
./test_first_frame \
    -f ../token.txt \
    -d 123456789012345678 \
    -m xiaomi.camera.082ac1 \
    -p 1234

# 双通道摄像头
./test_first_frame \
    -f ../token.txt \
    -d 123456789012345678 \
    -m chuangmi.camera.068ac1 \
    -c 2 \
    -p 1234
```

## 📊 预期输出

成功连接后，您会看到：

```
╔════════════════════════════════════════════════════════════════════════╗
║           MIoT Camera - First Frame Test                               ║
║                    Copyright (C) 2025                                   ║
╚════════════════════════════════════════════════════════════════════════╝

[Main] Loaded access token
[Main] Creating camera client...
[MIoTCameraClient] Loading library: libs/darwin/arm64/libmiot_camera_lite.dylib
[MIoTCameraClient] Library loaded successfully
[MIoTCameraClient] All functions bound successfully
[MIoTCameraClient] Initialized successfully
[MIoTCameraClient] Library version: 1.0.0

[Main] Creating camera: 123456789012345678 (xiaomi.camera.082ac1)
[MIoTCameraClient] Camera created: 123456789012345678 (xiaomi.camera.082ac1)

[Main] Starting camera...
[MIoTCameraClient] Camera started: 123456789012345678

[Status] Camera 123456789012345678 status changed to: CONNECTING
[Status] Camera 123456789012345678 status changed to: CONNECTED

[Frame] #1 | Size: 45678 bytes | Type: I | Codec: H264 | Timestamp: 1234567890 | Seq: 1 | Elapsed: 2s | Total: 44KB
[Main] Saved first frame to: first_frame_123456789012345678.h264

[Frame] #2 | Size: 3456 bytes | Type: P | Codec: H264 | Timestamp: 1234567920 | Seq: 2 | Elapsed: 2s | Total: 47KB
[Frame] #3 | Size: 2345 bytes | Type: P | Codec: H264 | Timestamp: 1234567950 | Seq: 3 | Elapsed: 2s | Total: 49KB
...

^C
[Main] Received signal 2, shutting down...
[Main] Stopping camera...

[Main] Summary:
  Total frames received: 150
  Total data received: 3456 KB
  Average FPS: 25

[Main] Done!
```

## 📁 输出文件

程序会保存第一帧到文件：
- 文件名：`first_frame_<DID>.h264`
- 格式：原始H264或H265数据
- 位置：当前运行目录

## 🔍 查看保存的帧

### 使用 ffmpeg 查看

```bash
# 查看文件信息
ffprobe first_frame_123456789012345678.h264

# 将H264转换为图片
ffmpeg -i first_frame_123456789012345678.h264 -frames:v 1 first_frame.jpg

# 播放视频流（如果保存了多帧）
ffplay -f h264 first_frame_123456789012345678.h264
```

### 使用 VLC 播放

```bash
vlc first_frame_123456789012345678.h264
```

## ⚙️ 命令行参数

| 参数 | 说明 | 必需 | 示例 |
|------|------|------|------|
| `-f, --token-file` | Access token文件路径 | ✅ | `-f token.txt` |
| `-d, --did` | 设备ID | ✅ | `-d 123456789...` |
| `-m, --model` | 设备型号 | ✅ | `-m xiaomi.camera.082ac1` |
| `-p, --pin` | 4位PIN码 | ❌ | `-p 1234` |
| `-c, --channels` | 通道数量 | ❌ | `-c 2` (默认: 1) |
| `-h, --help` | 显示帮助 | ❌ | `-h` |

## 🐛 故障排查

### 1. 找不到lib库

**错误**: `Failed to load library`

**解决方案**:
```bash
# 检查lib库是否存在
ls -la /Users/jiadiy/Workspace/miot_camera_bridge/libs/darwin/arm64/

# 如果不存在，从原项目复制
cp -r /Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/libs/darwin \
      /Users/jiadiy/Workspace/miot_camera_bridge/libs/
```

### 2. 摄像头连接失败

**错误**: `Failed to start camera`

**可能原因**:
- PIN码错误或缺失
- 摄像头不在线
- Access token过期
- 设备model不正确

**解决方案**:
```bash
# 1. 重新获取设备信息
./miot_discovery_with_cloud -f ../token.txt -i en0

# 2. 确认设备在线（Status: Online）

# 3. 尝试添加PIN码
./test_first_frame -f ../token.txt -d YOUR_DID -m YOUR_MODEL -p YOUR_PIN
```

### 3. Token过期

**错误**: `HTTP error code: 401`

**解决方案**:
重新获取access_token并更新token.txt文件

### 4. 没有收到帧

**可能原因**:
- 摄像头未成功连接（检查状态输出）
- 网络问题
- PIN码需要但未提供

**解决方案**:
```bash
# 增加日志，观察连接过程
./test_first_frame -f ../token.txt -d YOUR_DID -m YOUR_MODEL -p YOUR_PIN

# 查看状态变化：
# CONNECTING -> CONNECTED 表示成功
# CONNECTING -> ERROR 表示失败
```

## 💡 技术细节

### 视频编码格式

- **H.264**: 大部分小米摄像头使用
- **H.265/HEVC**: 部分新款摄像头使用
- **帧类型**:
  - **I帧**: 完整帧，较大（通常30-50KB）
  - **P帧**: 差分帧，较小（通常2-5KB）

### 数据流程

```
libmiot_camera_lite.dylib
    ↓
[P2P连接到摄像头]
    ↓
[接收原始H264/H265数据]
    ↓
raw_data_callback()
    ↓
RawFrameData结构
    ↓
您的处理函数
```

### 回调机制

- **线程**: 回调在lib库的线程中执行
- **频率**: 通常25-30 FPS
- **缓冲**: 建议使用队列缓冲处理

## 🔜 下一步

获取到原始帧后，您可以：

1. **解码为图像**: 使用FFmpeg或其他H264解码器
2. **实时显示**: 集成OpenCV或其他GUI库
3. **保存视频**: 写入MP4或其他容器格式
4. **AI分析**: 送入目标检测/识别模型

## 📚 相关文档

- [构建指南](BUILD_GUIDE.md)
- [云API文档](README_CLOUD.md)
- [LAN发现文档](README.md)

---

**祝您测试成功！** 🎉

