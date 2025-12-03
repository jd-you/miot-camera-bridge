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
    
    // 推送 H265 帧数据
    void push_frame(const std::vector<uint8_t>& data, uint64_t timestamp, bool is_keyframe);
    
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

    uint64_t base_timestamp_ = 0;       // 基准时间戳
    bool first_frame_ = true;           // 是否是第一帧
    
    // appsrc 相关
    GstElement* appsrc_ = nullptr;
    
    // 静态回调
    static void media_configure_callback(GstRTSPMediaFactory* factory, 
                                         GstRTSPMedia* media, 
                                         gpointer user_data);
    static void need_data_callback(GstElement* appsrc, guint unused, gpointer user_data);
    static void media_unprepared_callback(GstRTSPMedia* media, gpointer user_data);
};

} // namespace miot

#endif // GST_RTSP_SERVER_H