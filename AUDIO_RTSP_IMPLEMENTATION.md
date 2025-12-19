# 音频 RTSP 推流实现说明

## 概述

已成功添加从小米摄像头获取音频数据并推送到 RTSP 流的功能，参考了 `/Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/camera.py` 的实现。

## 修改内容

### 1. `include/gst_rtsp_server.h`

**变更内容：**
- 添加 `AudioFrame` 结构体，用于表示音频帧数据
- 将 `push_frame()` 重命名为 `push_video_frame()` 以区分视频和音频
- 添加 `push_audio_frame()` 方法用于推送音频数据
- 分离视频和音频的时间戳基准值：
  - `video_base_timestamp_` 和 `first_video_frame_`
  - `audio_base_timestamp_` 和 `first_audio_frame_`
- 分离视频和音频的 appsrc 元素：
  - `video_appsrc_` (原 `appsrc_`)
  - `audio_appsrc_` (新增)

### 2. `src/gst_rtsp_server.cpp`

**变更内容：**

#### Pipeline 配置
从单一视频流改为音视频流：
```cpp
const char* launch_str = 
    "( "
    // 视频流: H265
    "appsrc name=videosrc is-live=true format=time "
    "  caps=video/x-h265,stream-format=byte-stream,alignment=au "
    "! h265parse "
    "! rtph265pay name=pay0 pt=96 config-interval=1 "
    // 音频流: G711A (PCMA)
    "appsrc name=audiosrc is-live=true format=time "
    "  caps=audio/x-alaw,rate=8000,channels=1 "
    "! rtppcmapay name=pay1 pt=8 "
    ")";
```

#### 视频推流方法
- `push_video_frame()`: 处理视频帧推送
  - 使用相对时间戳（减去基准值）
  - 支持关键帧标记

#### 音频推流方法
- `push_audio_frame()`: 处理音频帧推送
  - 使用独立的音频时间戳基准
  - 自动处理第一帧时间戳初始化

#### Media 配置回调
- 同时配置视频和音频 appsrc
- 分别处理视频和音频流的初始化

#### Media 清理回调
- 客户端断开时重置视频和音频状态
- 清理视频和音频 appsrc 引用

### 3. `src/bridge_main.cpp`

**变更内容：**

#### 音频回调注册
```cpp
camera_bridge_context.camera_client->register_raw_audio_callback(did, 0, 
    [](const std::string& did, const RawFrameData& frame) {
        static int audio_frame_count = 0;
        if (audio_frame_count++ < 5) {
            std::cout << "[RawAudioCallback] Received audio frame: " 
                      << frame.data.size() << " bytes, codec: " 
                      << static_cast<int>(frame.codec_id)
                      << ", timestamp: " << frame.timestamp << std::endl;
        }
        
        // 推送音频帧到 RTSP
        camera_bridge_context.rtsp_servers["/xiaomi_camera"]->push_audio_frame(
            frame.data, frame.timestamp);
    });
```

#### 启用音频
```cpp
// 将第五个参数从 false 改为 true
camera_bridge_context.camera_client->start_camera(did, "", VideoQuality::HIGH, true);
```

#### 视频回调更新
```cpp
// 从 push_frame() 改为 push_video_frame()
camera_bridge_context.rtsp_servers["/xiaomi_camera"]->push_video_frame(
    frame.data, frame.timestamp, is_keyframe);
```

## 音频编码格式

当前实现支持 **G711A (PCMA)** 音频编码格式：
- 采样率: 8000 Hz
- 声道数: 1 (单声道)
- Payload Type: 8 (RTP 标准)

根据 SDK 返回的音频编解码器类型，可能需要调整 pipeline：

| SDK Codec | GStreamer Pipeline | RTP Payload Type |
|-----------|-------------------|------------------|
| `AUDIO_G711A` (1027) | `audio/x-alaw ! rtppcmapay` | PT=8 |
| `AUDIO_G711U` (1026) | `audio/x-mulaw ! rtppcmupay` | PT=0 |
| `AUDIO_OPUS` (1032) | `audio/x-opus ! rtpopuspay` | PT=96+ |
| `AUDIO_PCM` (1024) | `audio/x-raw ! opusenc ! rtpopuspay` | PT=96+ |

## 时间戳处理

### 相对时间戳
为了确保流能正确播放，使用相对时间戳而非绝对时间戳：

```cpp
// 视频
GST_BUFFER_PTS(buffer) = (timestamp - video_base_timestamp_) * GST_MSECOND;

// 音频
GST_BUFFER_PTS(buffer) = (timestamp - audio_base_timestamp_) * GST_MSECOND;
```

### 时间戳单位
假设 SDK 返回的 `timestamp` 单位是**毫秒 (ms)**，因此乘以 `GST_MSECOND` (10^6 纳秒)。

如果实际单位不同，需要调整转换系数：
- 微秒: `* GST_USECOND` (10^3 纳秒)
- 90kHz: `gst_util_uint64_scale(timestamp, GST_SECOND, 90000)`

## 播放测试

### VLC
```bash
vlc rtsp://YOUR_IP:8554/xiaomi_camera
```

### ffplay
```bash
ffplay -rtsp_transport tcp rtsp://YOUR_IP:8554/xiaomi_camera
```

### GStreamer
```bash
gst-launch-1.0 rtspsrc location=rtsp://YOUR_IP:8554/xiaomi_camera \
    ! rtph265depay ! h265parse ! avdec_h265 ! autovideosink
```

## 注意事项

1. **音频编解码器检测**: 当前硬编码为 G711A。如果摄像头使用其他格式，需要在运行时检测并调整 pipeline。

2. **首次音频帧**: 前 5 帧音频会打印调试信息，用于确认音频流是否正常工作。

3. **客户端断开**: 当 RTSP 客户端断开时，会自动重置视频和音频的时间戳基准，以便下次连接从头开始。

4. **错误处理**: 如果推送失败且错误码为 `GST_FLOW_FLUSHING`，表示客户端已断开，会静默处理。

## 编译

确保安装了 GStreamer 开发包：

**Ubuntu/Debian (ARM64 Docker):**
```bash
apt-get install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev \
    gstreamer1.0-plugins-good
```

**编译命令:**
```bash
./build-arm64-docker.sh
```

## 参考

- 小米相机 Python 实现: `/Users/jiadiy/Workspace/xiaomi-miloco/miot_kit/miot/camera.py:470-479`
- GStreamer RTSP Server 文档: https://gstreamer.freedesktop.org/documentation/rtsp-server/
- RTP Payload Types: https://en.wikipedia.org/wiki/RTP_payload_formats

