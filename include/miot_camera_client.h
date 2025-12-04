/**
 * MIoT Camera Client - C++ Wrapper for libmiot_camera_lite
 * 
 * This wraps the Xiaomi camera library to get video streams from MIoT cameras.
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#ifndef MIOT_CAMERA_CLIENT_H
#define MIOT_CAMERA_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>
#include <cstring>

namespace miot {

// Forward declarations
typedef void* MIoTCameraInstancePtr;

/**
 * @brief Camera codec types
 */
enum class CameraCodec {
    VIDEO_H264 = 4,
    VIDEO_H265 = 5,
    VIDEO_HEVC = 5,
    AUDIO_PCM = 1024,
    AUDIO_G711U = 1026,
    AUDIO_G711A = 1027,
    AUDIO_OPUS = 1032
};

/**
 * @brief Camera frame type
 */
enum class FrameType {
    P_FRAME = 0,  // P frame
    I_FRAME = 1   // I frame
};

/**
 * @brief Camera video quality
 */
enum class VideoQuality {
    LOW = 1,
    HIGH = 3
};

/**
 * @brief Camera status
 */
enum class CameraStatus {
    DISCONNECTED = 1,
    CONNECTING = 2,
    RE_CONNECTING = 3,
    CONNECTED = 4,
    ERROR = 5
};

/**
 * @brief Raw frame data structure
 */
struct RawFrameData {
    CameraCodec codec_id;
    uint32_t length;
    uint64_t timestamp;
    uint32_t sequence;
    FrameType frame_type;
    uint8_t channel;
    std::vector<uint8_t> data;
    
    RawFrameData() : codec_id(CameraCodec::VIDEO_H264), length(0), timestamp(0), 
                     sequence(0), frame_type(FrameType::P_FRAME), channel(0) {}
};

/**
 * @brief Callback types
 */
using RawVideoCallback = std::function<void(const std::string& did, const RawFrameData& frame)>;
using RawAudioCallback = std::function<void(const std::string& did, const RawFrameData& frame)>;
using StatusChangeCallback = std::function<void(const std::string& did, CameraStatus status)>;

/**
 * @brief MIoT Camera Client
 * 
 * Manages connection to Xiaomi IoT cameras and receives video/audio streams.
 */
class MIoTCameraClient {
public:
    /**
     * @brief Constructor
     * @param cloud_server Cloud server region (cn, de, us, etc.)
     * @param access_token OAuth2 access token
     * @param lib_path Optional path to libmiot_camera_lite library
     */
    explicit MIoTCameraClient(
        const std::string& cloud_server,
        const std::string& access_token,
        const std::string& lib_path = ""
    );
    
    /**
     * @brief Destructor
     */
    ~MIoTCameraClient();
    
    // Disable copy
    MIoTCameraClient(const MIoTCameraClient&) = delete;
    MIoTCameraClient& operator=(const MIoTCameraClient&) = delete;
    
    /**
     * @brief Initialize the client
     * @return true if initialized successfully
     */
    bool init();
    
    /**
     * @brief Create a camera instance
     * @param did Device ID
     * @param model Device model (e.g., "xiaomi.camera.082ac1")
     * @param channel_count Number of channels (usually 1 or 2)
     * @return true if created successfully
     */
    bool create_camera(const std::string& did, const std::string& model, int channel_count = 1);
    
    /**
     * @brief Start camera stream
     * @param did Device ID
     * @param pin_code 4-digit PIN code (optional, can be empty)
     * @param quality Video quality (LOW or HIGH)
     * @param enable_audio Enable audio stream
     * @return true if started successfully
     */
    bool start_camera(
        const std::string& did,
        const std::string& pin_code = "",
        VideoQuality quality = VideoQuality::LOW,
        bool enable_audio = false
    );
    
    /**
     * @brief Stop camera stream
     * @param did Device ID
     * @return true if stopped successfully
     */
    bool stop_camera(const std::string& did);
    
    /**
     * @brief Destroy camera instance
     * @param did Device ID
     */
    void destroy_camera(const std::string& did);
    
    /**
     * @brief Register callback for raw video frames
     * @param did Device ID
     * @param channel Channel number (0 or 1)
     * @param callback Callback function
     */
    void register_raw_video_callback(const std::string& did, int channel, RawVideoCallback callback);
    
    /**
     * @brief Register callback for raw audio frames
     * @param did Device ID
     * @param channel Channel number (0 or 1)
     * @param callback Callback function
     */
    void register_raw_audio_callback(const std::string& did, int channel, RawAudioCallback callback);
    
    /**
     * @brief Register callback for camera status changes
     * @param did Device ID
     * @param callback Callback function
     */
    void register_status_callback(const std::string& did, StatusChangeCallback callback);
    
    /**
     * @brief Get camera status
     * @param did Device ID
     * @return Camera status
     */
    CameraStatus get_status(const std::string& did);
    
    /**
     * @brief Get library version
     * @return Version string
     */
    std::string get_version();

private:
    // Library handle
    void* lib_handle_;
    
    // Configuration
    std::string cloud_server_;
    std::string access_token_;
    std::string lib_path_;
    std::string host_;
    
    // Camera instances
    struct CameraInstance {
        MIoTCameraInstancePtr ptr;
        std::string did;
        std::string model;
        int channel_count;
        std::map<int, RawVideoCallback> video_callbacks;
        std::map<int, RawAudioCallback> audio_callbacks;
        StatusChangeCallback status_callback;
    };
    
    std::map<std::string, CameraInstance> cameras_;
    mutable std::mutex cameras_mutex_;
    
    // Callback storage (to keep function pointers alive)
    std::map<std::string, void*> callback_refs_;
    
    // Library functions
    struct LibFunctions {
        // Initialization
        int (*miot_camera_init)(const char*, const char*, const char*);
        void (*miot_camera_deinit)();
        int (*miot_camera_update_access_token)(const char*);
        const char* (*miot_camera_version)();
        
        // Camera instance management
        MIoTCameraInstancePtr (*miot_camera_new)(const void*);
        void (*miot_camera_free)(MIoTCameraInstancePtr);
        int (*miot_camera_start)(MIoTCameraInstancePtr, const void*);
        int (*miot_camera_stop)(MIoTCameraInstancePtr);
        int (*miot_camera_status)(MIoTCameraInstancePtr);
        
        // Callbacks
        int (*miot_camera_register_raw_data)(MIoTCameraInstancePtr, void*, uint8_t);
        int (*miot_camera_unregister_raw_data)(MIoTCameraInstancePtr, uint8_t);
        int (*miot_camera_register_status_changed)(MIoTCameraInstancePtr, void*);
        int (*miot_camera_unregister_status_changed)(MIoTCameraInstancePtr);
        void (*miot_camera_set_log_handler)(void*);
        
        LibFunctions() { memset(this, 0, sizeof(LibFunctions)); }
    } lib_;
    
    // Private methods
    bool load_library();
    void unload_library();
    bool bind_functions();
    std::string find_library_path();
    
    // Static callbacks (bridge to member functions)
    static void log_callback(int level, const char* msg);
    static void raw_data_callback(const void* frame_header, const uint8_t* data);
    static void status_callback(int status);
    
    // Singleton instance for static callbacks
    static MIoTCameraClient* instance_;
    
    // Internal callback handlers
    void handle_raw_data(const std::string& did, const void* frame_header, const uint8_t* data);
    void handle_status_change(const std::string& did, int status);
};

} // namespace miot

#endif // MIOT_CAMERA_CLIENT_H

