/**
 * MIoT Camera Client - Implementation
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#include "miot_camera_client.h"

#include <iostream>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

#ifdef __APPLE__
    #include <mach-o/dyld.h>
    #include <sys/sysctl.h>
#elif defined(__linux__)
    #include <sys/utsname.h>
#endif

namespace miot {

// Xiaomi API constants
constexpr const char* OAUTH2_CLIENT_ID = "2882303761520431603";
constexpr const char* OAUTH2_API_HOST_DEFAULT = "mico.api.mijia.tech";

// C structures matching the library API
#pragma pack(push, 1)
struct CameraFrameHeaderC {
    uint32_t codec_id;
    uint32_t length;
    uint64_t timestamp;
    uint32_t sequence;
    uint32_t frame_type;
    uint8_t channel;
};

struct CameraInfoC {
    const char* did;
    const char* model;
    uint8_t channel_count;
};

struct CameraConfigC {
    const uint8_t* video_qualities;
    bool enable_audio;
    const char* pin_code;
};
#pragma pack(pop)

// Static instance for callbacks
MIoTCameraClient* MIoTCameraClient::instance_ = nullptr;

MIoTCameraClient::MIoTCameraClient(
    const std::string& cloud_server,
    const std::string& access_token,
    const std::string& lib_path
) : lib_handle_(nullptr),
    cloud_server_(cloud_server),
    access_token_(access_token),
    lib_path_(lib_path)
{
    host_ = OAUTH2_API_HOST_DEFAULT;
    if (cloud_server != "cn") {
        host_ = cloud_server + "." + host_;
    }
    
    instance_ = this;
}

MIoTCameraClient::~MIoTCameraClient() {
    // Stop and destroy all cameras
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    for (auto& pair : cameras_) {
        if (lib_.miot_camera_stop) {
            lib_.miot_camera_stop(pair.second.ptr);
        }
        if (lib_.miot_camera_free) {
            lib_.miot_camera_free(pair.second.ptr);
        }
    }
    cameras_.clear();
    
    // Deinit library
    if (lib_.miot_camera_deinit) {
        lib_.miot_camera_deinit();
    }
    
    unload_library();
    instance_ = nullptr;
}

std::string MIoTCameraClient::find_library_path() {
    if (!lib_path_.empty()) {
        return lib_path_;
    }
    
    // Get current executable directory
    char exe_path[1024] = {0};
    
#ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        std::string path(exe_path);
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            path = path.substr(0, last_slash);
        }
        
        // Check for library in multiple locations
        std::vector<std::string> possible_paths = {
            path + "/libmiot_camera_lite.dylib",
            path + "/../libs/darwin/arm64/libmiot_camera_lite.dylib",
            path + "/../libs/darwin/x86_64/libmiot_camera_lite.dylib",
            "/usr/local/lib/libmiot_camera_lite.dylib"
        };
        
        for (const auto& p : possible_paths) {
            if (access(p.c_str(), R_OK) == 0) {
                return p;
            }
        }
    }
    
    // Default paths based on architecture
    struct utsname un;
    uname(&un);
    std::string machine(un.machine);
    
    if (machine == "arm64" || machine == "aarch64") {
        return "libs/darwin/arm64/libmiot_camera_lite.dylib";
    } else {
        return "libs/darwin/x86_64/libmiot_camera_lite.dylib";
    }
    
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        std::string path(exe_path);
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            path = path.substr(0, last_slash);
        }
        
        std::vector<std::string> possible_paths = {
            path + "/libmiot_camera_lite.so",
            path + "/../libs/linux/x86_64/libmiot_camera_lite.so",
            path + "/../libs/linux/arm64/libmiot_camera_lite.so",
            "/usr/local/lib/libmiot_camera_lite.so"
        };
        
        for (const auto& p : possible_paths) {
            if (access(p.c_str(), R_OK) == 0) {
                return p;
            }
        }
    }
    
    struct utsname un;
    uname(&un);
    std::string machine(un.machine);
    
    if (machine == "x86_64" || machine == "amd64") {
        return "libs/linux/x86_64/libmiot_camera_lite.so";
    } else if (machine == "aarch64" || machine == "arm64") {
        return "libs/linux/arm64/libmiot_camera_lite.so";
    }
    
#endif
    
    return "libmiot_camera_lite.so";
}

bool MIoTCameraClient::load_library() {
    std::string lib_path = find_library_path();
    
    std::cout << "[MIoTCameraClient] Loading library: " << lib_path << std::endl;
    
    lib_handle_ = dlopen(lib_path.c_str(), RTLD_LAZY);
    if (!lib_handle_) {
        std::cerr << "[MIoTCameraClient] Failed to load library: " << dlerror() << std::endl;
        return false;
    }
    
    std::cout << "[MIoTCameraClient] Library loaded successfully" << std::endl;
    return true;
}

void MIoTCameraClient::unload_library() {
    if (lib_handle_) {
        dlclose(lib_handle_);
        lib_handle_ = nullptr;
    }
}

bool MIoTCameraClient::bind_functions() {
    if (!lib_handle_) {
        return false;
    }
    
    // Bind all functions
    #define BIND_FUNC(name) \
        lib_.name = reinterpret_cast<decltype(lib_.name)>(dlsym(lib_handle_, #name)); \
        if (!lib_.name) { \
            std::cerr << "[MIoTCameraClient] Failed to bind function: " #name << std::endl; \
            return false; \
        }
    
    BIND_FUNC(miot_camera_init);
    BIND_FUNC(miot_camera_deinit);
    BIND_FUNC(miot_camera_update_access_token);
    BIND_FUNC(miot_camera_version);
    BIND_FUNC(miot_camera_new);
    BIND_FUNC(miot_camera_free);
    BIND_FUNC(miot_camera_start);
    BIND_FUNC(miot_camera_stop);
    BIND_FUNC(miot_camera_status);
    BIND_FUNC(miot_camera_register_raw_data);
    BIND_FUNC(miot_camera_unregister_raw_data);
    BIND_FUNC(miot_camera_register_status_changed);
    BIND_FUNC(miot_camera_unregister_status_changed);
    BIND_FUNC(miot_camera_set_log_handler);
    
    #undef BIND_FUNC
    
    std::cout << "[MIoTCameraClient] All functions bound successfully" << std::endl;
    return true;
}

bool MIoTCameraClient::init() {
    if (!load_library()) {
        return false;
    }
    
    if (!bind_functions()) {
        return false;
    }
    
    // Set log handler
    using LogCallback = void(*)(int, const char*);
    lib_.miot_camera_set_log_handler(reinterpret_cast<void*>(
        static_cast<LogCallback>(log_callback)
    ));
    
    // Initialize library
    int result = lib_.miot_camera_init(
        host_.c_str(),
        OAUTH2_CLIENT_ID,
        access_token_.c_str()
    );
    
    if (result != 0) {
        std::cerr << "[MIoTCameraClient] Failed to initialize library: " << result << std::endl;
        return false;
    }
    
    std::cout << "[MIoTCameraClient] Initialized successfully" << std::endl;
    std::cout << "[MIoTCameraClient] Library version: " << get_version() << std::endl;
    
    return true;
}

bool MIoTCameraClient::create_camera(const std::string& did, const std::string& model, int channel_count) {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    // Check if already exists
    if (cameras_.find(did) != cameras_.end()) {
        std::cerr << "[MIoTCameraClient] Camera already exists: " << did << std::endl;
        return false;
    }
    
    // Create camera info structure
    CameraInfoC info;
    info.did = did.c_str();
    info.model = model.c_str();
    info.channel_count = static_cast<uint8_t>(channel_count);
    
    // Create camera instance
    MIoTCameraInstancePtr ptr = lib_.miot_camera_new(&info);
    if (!ptr) {
        std::cerr << "[MIoTCameraClient] Failed to create camera: " << did << std::endl;
        return false;
    }
    
    // Store camera instance
    CameraInstance instance;
    instance.ptr = ptr;
    instance.did = did;
    instance.model = model;
    instance.channel_count = channel_count;
    
    cameras_[did] = instance;
    
    std::cout << "[MIoTCameraClient] Camera created: " << did << " (" << model << ")" << std::endl;
    return true;
}

bool MIoTCameraClient::start_camera(
    const std::string& did,
    const std::string& pin_code,
    VideoQuality quality,
    bool enable_audio
) {
    void* camera_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(cameras_mutex_);
        auto it = cameras_.find(did);
        if (it == cameras_.end()) {
            std::cerr << "[MIoTCameraClient] Camera not found: " << did << std::endl;
            return false;
        }
        camera_ptr = it->second.ptr;
    }
    
    // Prepare quality array (terminated with 0)
    uint8_t qualities[3] = {static_cast<uint8_t>(quality), 0, 0};
    
    // Create config structure
    CameraConfigC config;
    config.video_qualities = qualities;
    config.enable_audio = enable_audio;
    config.pin_code = pin_code.empty() ? nullptr : pin_code.c_str();
    
    // Start camera
    int result = lib_.miot_camera_start(camera_ptr, &config);
    if (result != 0) {
        std::cerr << "[MIoTCameraClient] Failed to start camera: " << did 
                  << ", error: " << result << std::endl;
        return false;
    }
    
    std::cout << "[MIoTCameraClient] Camera started: " << did << std::endl;
    return true;
}

bool MIoTCameraClient::stop_camera(const std::string& did) {
    void* camera_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(cameras_mutex_);
        auto it = cameras_.find(did);
        if (it == cameras_.end()) {
            std::cerr << "[MIoTCameraClient] Camera not found: " << did << std::endl;
            return false;
        }
        camera_ptr = it->second.ptr;
    }
    
    int result = lib_.miot_camera_stop(camera_ptr);
    if (result != 0) {
        std::cerr << "[MIoTCameraClient] Failed to stop camera: " << did << std::endl;
        return false;
    }
    
    std::cout << "[MIoTCameraClient] Camera stopped: " << did << std::endl;
    return true;
}

void MIoTCameraClient::destroy_camera(const std::string& did) {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        return;
    }
    
    lib_.miot_camera_free(it->second.ptr);
    cameras_.erase(it);
    
    std::cout << "[MIoTCameraClient] Camera destroyed: " << did << std::endl;
}

void MIoTCameraClient::register_raw_video_callback(const std::string& did, int channel, RawVideoCallback callback) {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        std::cerr << "[MIoTCameraClient] Camera not found: " << did << std::endl;
        return;
    }
    
    it->second.video_callbacks[channel] = callback;
    
    // Register raw data callback with library
    using RawDataCallback = void(*)(const void*, const uint8_t*);
    lib_.miot_camera_register_raw_data(
        it->second.ptr,
        reinterpret_cast<void*>(static_cast<RawDataCallback>(raw_data_callback)),
        static_cast<uint8_t>(channel)
    );
    
    std::cout << "[MIoTCameraClient] Registered video callback: " << did 
              << ", channel: " << channel << std::endl;
}

void MIoTCameraClient::register_raw_audio_callback(const std::string& did, int channel, RawAudioCallback callback) {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        std::cerr << "[MIoTCameraClient] Camera not found: " << did << std::endl;
        return;
    }
    
    it->second.audio_callbacks[channel] = callback;
    std::cout << "[MIoTCameraClient] Registered audio callback: " << did 
              << ", channel: " << channel << std::endl;
}

void MIoTCameraClient::register_status_callback(const std::string& did, StatusChangeCallback callback) {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        std::cerr << "[MIoTCameraClient] Camera not found: " << did << std::endl;
        return;
    }
    
    it->second.status_callback = callback;
    
    // Register status callback with library
    using StatusCallback = void(*)(int);
    lib_.miot_camera_register_status_changed(
        it->second.ptr,
        reinterpret_cast<void*>(static_cast<StatusCallback>(status_callback))
    );
    
    std::cout << "[MIoTCameraClient] Registered status callback: " << did << std::endl;
}

CameraStatus MIoTCameraClient::get_status(const std::string& did) {
    std::lock_guard<std::mutex> lock(cameras_mutex_);
    
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        return CameraStatus::DISCONNECTED;
    }
    
    int status = lib_.miot_camera_status(it->second.ptr);
    return static_cast<CameraStatus>(status);
}

std::string MIoTCameraClient::get_version() {
    if (!lib_.miot_camera_version) {
        return "unknown";
    }
    
    const char* version = lib_.miot_camera_version();
    return version ? std::string(version) : "unknown";
}

// Static callback implementations
void MIoTCameraClient::log_callback(int level, const char* msg) {
    const char* level_str = "INFO";
    switch (level) {
        case 0: level_str = "DEBUG"; break;
        case 1: level_str = "INFO"; break;
        case 2: level_str = "WARN"; break;
        case 3: level_str = "ERROR"; break;
    }
    std::cout << "[libmiot_camera][" << level_str << "] " << msg << std::endl;
}

void MIoTCameraClient::raw_data_callback(const void* frame_header, const uint8_t* data) {
    if (!instance_) {
        return;
    }
    
    // This is called from library thread, so we need to be careful
    // For now, we'll process it synchronously
    // In production, you might want to queue this for processing in another thread
    
    // We need to find which camera this belongs to
    // The library doesn't pass the camera instance, so we'll process for all cameras
    // This is a limitation - in practice you might need a more sophisticated approach
    
    std::lock_guard<std::mutex> lock(instance_->cameras_mutex_);
    for (auto& pair : instance_->cameras_) {
        instance_->handle_raw_data(pair.first, frame_header, data);
    }
}

void MIoTCameraClient::status_callback(int status) {
    if (!instance_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(instance_->cameras_mutex_);
    for (auto& pair : instance_->cameras_) {
        instance_->handle_status_change(pair.first, status);
    }
}

void MIoTCameraClient::handle_raw_data(const std::string& did, const void* frame_header_ptr, const uint8_t* data) {
    auto* header = static_cast<const CameraFrameHeaderC*>(frame_header_ptr);
    
    RawFrameData frame;
    frame.codec_id = static_cast<CameraCodec>(header->codec_id);
    frame.length = header->length;
    frame.timestamp = header->timestamp;
    frame.sequence = header->sequence;
    frame.frame_type = static_cast<FrameType>(header->frame_type);
    frame.channel = header->channel;
    frame.data.assign(data, data + header->length);
    
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        return;
    }
    
    // Check if it's video or audio
    if (frame.codec_id == CameraCodec::VIDEO_H264 || frame.codec_id == CameraCodec::VIDEO_H265) {
        // Video frame
        auto callback_it = it->second.video_callbacks.find(frame.channel);
        if (callback_it != it->second.video_callbacks.end() && callback_it->second) {
            callback_it->second(did, frame);
        }
    } else {
        // Audio frame
        auto callback_it = it->second.audio_callbacks.find(frame.channel);
        if (callback_it != it->second.audio_callbacks.end() && callback_it->second) {
            callback_it->second(did, frame);
        }
    }
}

void MIoTCameraClient::handle_status_change(const std::string& did, int status) {
    auto it = cameras_.find(did);
    if (it == cameras_.end()) {
        return;
    }
    
    if (it->second.status_callback) {
        it->second.status_callback(did, static_cast<CameraStatus>(status));
    }
}

} // namespace miot

