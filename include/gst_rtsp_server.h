#ifndef GST_RTSP_SERVER_H
#define GST_RTSP_SERVER_H

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <string>
#include <mutex>
#include <queue>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace miot {

struct VideoFrame {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_keyframe;
};

struct AudioFrame {
    std::vector<uint8_t> data;
    uint64_t timestamp;
};

class GstRtspServer {
public:
    GstRtspServer(int port = 8554, const std::string& mount_point = "/camera");
    ~GstRtspServer();

    // 初始化 GStreamer
    bool init();
    
    // 启动 RTSP 服务器
    bool start();
    
    // 停止 RTSP 服务器
    void stop();
    
    // 推送视频帧数据
    void push_video_frame(const std::vector<uint8_t>& data, uint64_t timestamp, bool is_keyframe);
    
    // 推送音频帧数据
    void push_audio_frame(const std::vector<uint8_t>& data, uint64_t timestamp);
    
    // 获取 RTSP URL
    std::string get_url() const;

private:
    int port_;
    std::string mount_point_;
    
    GstRTSPServer* server_ = nullptr;
    GMainLoop* loop_ = nullptr;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    
    // 帧队列
    std::queue<VideoFrame> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    uint64_t video_base_timestamp_ = 0;  // 视频基准时间戳
    uint64_t audio_base_timestamp_ = 0;  // 音频基准时间戳
    bool first_video_frame_ = true;      // 是否是第一个视频帧
    bool first_audio_frame_ = true;      // 是否是第一个音频帧
    
    // appsrc 相关
    GstElement* video_appsrc_ = nullptr;
    GstElement* audio_appsrc_ = nullptr;
    
    // 静态回调
    static void media_configure_callback(GstRTSPMediaFactory* factory, 
                                         GstRTSPMedia* media, 
                                         gpointer user_data);
    static void need_data_callback(GstElement* appsrc, guint unused, gpointer user_data);
    static void media_unprepared_callback(GstRTSPMedia* media, gpointer user_data);
};

} // namespace miot

#endif // GST_RTSP_SERVER_H