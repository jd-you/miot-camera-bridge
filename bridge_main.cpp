#include <memory>

#include "miot_lan_device.h"
#include "include/miot_oauth.h"
#include "miot_cloud_client.h"
#include "miot_camera_client.h"
#include "gst_rtsp_server.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace miot;

const std::string CLIENT_ID = "2882303761520431603";
const std::string REDIRECT_URI = "https://mico.api.mijia.tech/login_redirect";
const std::string CLOUD_SERVER = "cn";
const std::string TOKEN_FILE = "miot_token.json";

struct CameraBridgeContext {
    std::shared_ptr<MiotOAuth> oauth;
    std::shared_ptr<MIoTCloudClient> cloud_client;
    std::shared_ptr<MIoTLanDiscovery> discovery;
    std::shared_ptr<MIoTCameraClient> camera_client;
    std::map<std::string, std::shared_ptr<CloudDeviceInfo>> cloud_devices;
    std::map<std::string, std::shared_ptr<GstRtspServer>> rtsp_servers;
};
CameraBridgeContext camera_bridge_context;

// 添加到 miot_camera_client.h 或单独的工具文件

/**
 * @brief 检测 H265 帧是否是关键帧 (I帧)
 * 
 * H265 NAL unit type 编码在前两个字节中:
 * byte[0]: forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id高2位(1)
 * NAL type = (byte[0] >> 1) & 0x3F
 * 
 * 关键帧类型:
 *   16-21: BLA/IDR/CRA (关键帧)
 *   32-34: VPS/SPS/PPS (参数集，通常在I帧前)
 */
inline bool is_h265_keyframe(const uint8_t* data, size_t size) {
    if (size < 4) return false;
    
    // 查找 start code (0x00 0x00 0x01 或 0x00 0x00 0x00 0x01)
    size_t offset = 0;
    if (size >= 4 && data[0] == 0x00 && data[1] == 0x00) {
        if (data[2] == 0x01) {
            offset = 3;
        } else if (data[2] == 0x00 && data[3] == 0x01) {
            offset = 4;
        }
    }
    
    if (offset == 0 || offset >= size) {
        // 没有 start code，假设直接是 NAL unit
        offset = 0;
    }
    
    // 获取 NAL unit type
    uint8_t nal_unit_type = (data[offset] >> 1) & 0x3F;
    
    // H265 关键帧 NAL types:
    // 16 = BLA_W_LP
    // 17 = BLA_W_RADL  
    // 18 = BLA_N_LP
    // 19 = IDR_W_RADL (最常见的 IDR)
    // 20 = IDR_N_LP
    // 21 = CRA_NUT
    // 32 = VPS (视频参数集)
    // 33 = SPS (序列参数集)
    // 34 = PPS (图像参数集)
    
    return (nal_unit_type >= 16 && nal_unit_type <= 21) ||
           (nal_unit_type >= 32 && nal_unit_type <= 34);
}

/**
 * @brief 检测 H264 帧是否是关键帧 (I帧)
 */
inline bool is_h264_keyframe(const uint8_t* data, size_t size) {
    if (size < 4) return false;
    
    size_t offset = 0;
    if (size >= 4 && data[0] == 0x00 && data[1] == 0x00) {
        if (data[2] == 0x01) {
            offset = 3;
        } else if (data[2] == 0x00 && data[3] == 0x01) {
            offset = 4;
        }
    }
    
    if (offset == 0 || offset >= size) {
        offset = 0;
    }
    
    uint8_t nal_unit_type = data[offset] & 0x1F;
    
    // H264 关键帧 NAL types:
    // 5 = IDR slice
    // 7 = SPS
    // 8 = PPS
    return nal_unit_type == 5 || nal_unit_type == 7 || nal_unit_type == 8;
}

void device_status_changed_callback(const std::string& did, const DeviceInfo& info) {
    std::shared_ptr<CloudDeviceInfo> cloud_device_info;
    switch (info.status_changed_type) {
        case DeviceStatusChangedType::NEW:{
            

            auto cloud_device_info = camera_bridge_context.cloud_client->get_device(did);
            camera_bridge_context.cloud_devices[did] = std::make_shared<CloudDeviceInfo>(cloud_device_info);

            std::cout << "[DeviceStatusChangedCallback] Device is new " << did << " " << cloud_device_info.model << std::endl;

            if (cloud_device_info.model == "chuangmi.camera.029a02") {
                camera_bridge_context.camera_client->create_camera(did, cloud_device_info.model, 1);
            

                camera_bridge_context.camera_client->register_raw_video_callback(did, 0, [](const std::string& did, const RawFrameData& frame) {
                    
                    bool is_keyframe = false;
        
                    if (frame.codec_id == CameraCodec::VIDEO_H265) {
                        is_keyframe = is_h265_keyframe(frame.data.data(), frame.data.size());
                    } else if (frame.codec_id == CameraCodec::VIDEO_H264) {
                        is_keyframe = is_h264_keyframe(frame.data.data(), frame.data.size());
                    }

                    // std::cout << "[RawVideoCallback] Received frame: " << frame.data.size() << " bytes, timestamp: " << frame.timestamp << ", frame_type: " << is_keyframe << std::endl;
                    camera_bridge_context.rtsp_servers["/xiaomi_camera"]->push_frame(frame.data, frame.timestamp, is_keyframe);
                });

                camera_bridge_context.camera_client->register_status_callback(did, [](const std::string& did, CameraStatus status) {
                    std::cout << "[StatusChangeCallback] Camera status changed: " << did << " -> " << static_cast<int>(status) << std::endl;
                });

                camera_bridge_context.camera_client->start_camera(did, "", VideoQuality::HIGH, false);

                std::cout << "[DeviceStatusChangedCallback] Camera started" << std::endl;   
            }
            break;
        }
        case DeviceStatusChangedType::ONLINE:
            std::cout << "[DeviceStatusChangedCallback] Device is online" << std::endl;
            break;
        case DeviceStatusChangedType::OFFLINE:
            std::cout << "[DeviceStatusChangedCallback] Device is offline" << std::endl;
            break;
        case DeviceStatusChangedType::IP_CHANGED:
            std::cout << "[DeviceStatusChangedCallback] Device IP changed" << std::endl;
            break;
        case DeviceStatusChangedType::INTERFACE_CHANGED:
            std::cout << "[DeviceStatusChangedCallback] Device interface changed" << std::endl;
            break;
        default:
            break;
    }
}



int main() {
    // 创建一个MiotOAuth实例，并且进行token有效性的监控
    std::shared_ptr<MiotOAuth> oauth = std::make_shared<MiotOAuth>(CLIENT_ID, REDIRECT_URI, CLOUD_SERVER, TOKEN_FILE);

    camera_bridge_context.oauth = oauth;

    oauth->start_auth_flow();

    TokenInfo token;
    bool success = oauth->get_token(token);
    if (!success) {
        std::cerr << "Failed to get token" << std::endl;
        return 1;
    }

    // 创建一个MIoTCloudClient实例，完成初始化，并且能够更新token
    std::shared_ptr<MIoTCloudClient> cloud_client = std::make_shared<MIoTCloudClient>(token.access_token);
    cloud_client->init();
    camera_bridge_context.cloud_client = cloud_client;

    std::shared_ptr<MIoTCameraClient> camera_client = std::make_shared<MIoTCameraClient>("cn", token.access_token);
    camera_client->init();
    camera_bridge_context.camera_client = camera_client;

    std::shared_ptr<GstRtspServer> rtsp_server = std::make_shared<GstRtspServer>(8554, "/xiaomi_camera");
    rtsp_server->init();
    camera_bridge_context.rtsp_servers["/xiaomi_camera"] = rtsp_server;
    rtsp_server->start();
    std::cout << "RTSP Stream URL: " << rtsp_server->get_url() << std::endl;
    
    // 创建一个MIoTLanDiscovery
    std::shared_ptr<MIoTLanDiscovery> discovery = std::make_shared<MIoTLanDiscovery>();
    camera_bridge_context.discovery = discovery;
    discovery->register_callback("device_status_changed_callback", device_status_changed_callback);
    discovery->start();

    // 循环执行，等待结束
    while (1) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}