#include <memory>

#include "miot_lan_device.h"
#include "include/miot_oauth.h"
#include "miot_cloud_client.h"
#include "miot_camera_client.h"

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
};
CameraBridgeContext camera_bridge_context;

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
                    std::cout << "[RawVideoCallback] Received frame: " << frame.data.size() << " bytes" << std::endl;
                });

                camera_bridge_context.camera_client->register_status_callback(did, [](const std::string& did, CameraStatus status) {
                    std::cout << "[StatusChangeCallback] Camera status changed: " << did << " -> " << static_cast<int>(status) << std::endl;
                });

                camera_bridge_context.camera_client->start_camera(did, "", VideoQuality::HIGH, true);

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