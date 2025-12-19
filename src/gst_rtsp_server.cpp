#include "gst_rtsp_server.h"
#include <iostream>
#include <sstream>

namespace miot {

GstRtspServer::GstRtspServer(int port, const std::string& mount_point)
    : port_(port), mount_point_(mount_point) {
}

GstRtspServer::~GstRtspServer() {
    stop();
}

bool GstRtspServer::init() {
    // 初始化 GStreamer
    gst_init(nullptr, nullptr);
    
    // 创建 RTSP 服务器
    server_ = gst_rtsp_server_new();
    if (!server_) {
        std::cerr << "Failed to create RTSP server" << std::endl;
        return false;
    }
    
    // 设置端口
    gchar* port_str = g_strdup_printf("%d", port_);
    gst_rtsp_server_set_service(server_, port_str);
    g_free(port_str);
    
    // 创建 media factory
    GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();
    
    // 支持音视频的 RTSP pipeline
    // 视频: H265 
    // 音频: G711A (PCMA)
    const char* launch_str = 
        "( "
        // 视频流
        "appsrc name=videosrc is-live=true format=time "
        "  caps=video/x-h265,stream-format=byte-stream,alignment=au "
        "! h265parse "
        "! rtph265pay name=pay0 pt=96 config-interval=1 "
        // 音频流 (G711A/PCMA)
        "appsrc name=audiosrc is-live=true format=time "
        "  caps=audio/x-alaw,rate=8000,channels=1 "
        "! rtppcmapay name=pay1 pt=8 "
        ")";
    
    gst_rtsp_media_factory_set_launch(factory, launch_str);
    gst_rtsp_media_factory_set_shared(factory, TRUE);  // 允许多客户端
    
    // 连接 media-configure 信号
    g_signal_connect(factory, "media-configure", 
                     G_CALLBACK(media_configure_callback), this);
    
    // 获取 mount points 并添加 factory
    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server_);
    gst_rtsp_mount_points_add_factory(mounts, mount_point_.c_str(), factory);
    g_object_unref(mounts);
    
    std::cout << "RTSP Server (Audio+Video) initialized on port " << port_ << std::endl;
    return true;
}

bool GstRtspServer::start() {
    if (running_) {
        return true;
    }
    
    running_ = true;
    
    // 在新线程中运行 GMainLoop
    server_thread_ = std::thread([this]() {
        // 附加服务器到默认上下文
        gst_rtsp_server_attach(server_, nullptr);
        
        std::cout << "RTSP Server started: " << get_url() << std::endl;
        
        // 运行主循环
        loop_ = g_main_loop_new(nullptr, FALSE);
        g_main_loop_run(loop_);
        
        std::cout << "RTSP Server stopped" << std::endl;
    });
    
    return true;
}

void GstRtspServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (loop_) {
        g_main_loop_quit(loop_);
    }
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    if (loop_) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    
    if (server_) {
        g_object_unref(server_);
        server_ = nullptr;
    }
}

void GstRtspServer::push_video_frame(const std::vector<uint8_t>& data, 
                                uint64_t timestamp, 
                                bool is_keyframe) {
    if (!running_ || !video_appsrc_) {
        // 如果还没有客户端连接，暂存帧（可选）
        return;
    }

    if (first_video_frame_) {
        video_base_timestamp_ = timestamp;
        first_video_frame_ = false;
        std::cout << "First video frame timestamp (base): " << video_base_timestamp_ << std::endl;
    }
    
    // 创建 GstBuffer
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, data.size(), nullptr);
    
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    memcpy(map.data, data.data(), data.size());
    gst_buffer_unmap(buffer, &map);
    
    // 设置相对时间戳 (转换为纳秒)
    GST_BUFFER_PTS(buffer) = (timestamp - video_base_timestamp_) * GST_MSECOND;
    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer);
    GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;
    
    // 设置关键帧标志
    if (!is_keyframe) {
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    }
    
    // 推送到 appsrc
    GstFlowReturn ret;
    g_signal_emit_by_name(video_appsrc_, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);
    
    if (ret != GST_FLOW_OK) {
        if (ret == GST_FLOW_FLUSHING) {
            std::cout << "Video: Client disconnected (flushing)" << std::endl;
        } else {
            std::cerr << "Failed to push video buffer: " << ret << std::endl;
        }
    }
}

void GstRtspServer::push_audio_frame(const std::vector<uint8_t>& data, 
                                      uint64_t timestamp) {
    if (!running_ || !audio_appsrc_) {
        // 如果还没有客户端连接或音频未启用
        return;
    }

    if (first_audio_frame_) {
        audio_base_timestamp_ = timestamp;
        first_audio_frame_ = false;
        std::cout << "First audio frame timestamp (base): " << audio_base_timestamp_ << std::endl;
    }
    
    // 创建 GstBuffer
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, data.size(), nullptr);
    
    GstMapInfo map;
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    memcpy(map.data, data.data(), data.size());
    gst_buffer_unmap(buffer, &map);
    
    // 设置相对时间戳 (转换为纳秒)
    GST_BUFFER_PTS(buffer) = (timestamp - audio_base_timestamp_) * GST_MSECOND;
    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer);
    GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;
    
    // 推送到 appsrc
    GstFlowReturn ret;
    g_signal_emit_by_name(audio_appsrc_, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);
    
    if (ret != GST_FLOW_OK) {
        if (ret == GST_FLOW_FLUSHING) {
            std::cout << "Audio: Client disconnected (flushing)" << std::endl;
        } else {
            std::cerr << "Failed to push audio buffer: " << ret << std::endl;
        }
    }
}

std::string GstRtspServer::get_url() const {
    std::stringstream ss;
    ss << "rtsp://0.0.0.0:" << port_ << mount_point_;
    return ss.str();
}

void GstRtspServer::media_configure_callback(GstRTSPMediaFactory* factory,
                                              GstRTSPMedia* media,
                                              gpointer user_data) {
    GstRtspServer* self = static_cast<GstRtspServer*>(user_data);
    
    GstElement* element = gst_rtsp_media_get_element(media);
    
    // 获取视频 appsrc
    self->video_appsrc_ = gst_bin_get_by_name_recurse_up(GST_BIN(element), "videosrc");
    
    if (self->video_appsrc_) {
        // 配置视频 appsrc
        g_object_set(self->video_appsrc_,
                     "stream-type", 0,  // GST_APP_STREAM_TYPE_STREAM
                     "format", GST_FORMAT_TIME,
                     "is-live", TRUE,
                     nullptr);
        
        std::cout << "RTSP client connected, video appsrc configured" << std::endl;
    }
    
    // 获取音频 appsrc
    self->audio_appsrc_ = gst_bin_get_by_name_recurse_up(GST_BIN(element), "audiosrc");
    
    if (self->audio_appsrc_) {
        // 配置音频 appsrc
        g_object_set(self->audio_appsrc_,
                     "stream-type", 0,  // GST_APP_STREAM_TYPE_STREAM
                     "format", GST_FORMAT_TIME,
                     "is-live", TRUE,
                     nullptr);
        
        std::cout << "RTSP client connected, audio appsrc configured" << std::endl;
    }
    
    g_signal_connect(media, "unprepared", G_CALLBACK(media_unprepared_callback), self);
    g_object_unref(element);
}

void GstRtspServer::need_data_callback(GstElement* appsrc, 
                                        guint unused, 
                                        gpointer user_data) {
    // 可选：当 appsrc 需要数据时的回调
    // 这里我们使用 push 模式，所以不需要实现
}

void GstRtspServer::media_unprepared_callback(GstRTSPMedia* media, gpointer user_data) {
    GstRtspServer* self = static_cast<GstRtspServer*>(user_data);

    // 重置状态
    self->first_video_frame_ = true;
    self->first_audio_frame_ = true;
    self->video_base_timestamp_ = 0;
    self->audio_base_timestamp_ = 0;

    std::cout << "RTSP client disconnected, clearing appsrc" << std::endl;

    if (self->video_appsrc_) {
        g_object_unref(self->video_appsrc_);
        self->video_appsrc_ = nullptr;
    }
    
    if (self->audio_appsrc_) {
        g_object_unref(self->audio_appsrc_);
        self->audio_appsrc_ = nullptr;
    }
}

} // namespace miot