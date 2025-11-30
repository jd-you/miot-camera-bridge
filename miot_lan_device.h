/**
 * MIoT LAN Device Discovery
 * 
 * A C++ implementation of Xiaomi IoT device discovery using OTU (One Touch) protocol.
 * This library scans for Xiaomi devices on the local network using UDP broadcast.
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#ifndef MIOT_LAN_DEVICE_H
#define MIOT_LAN_DEVICE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace miot {

/**
 * @brief Device information structure
 */
struct DeviceInfo {
    std::string did;           // Device ID
    std::string ip;            // IP address
    std::string interface;     // Network interface name
    bool online;               // Online status
    int64_t timestamp_offset;  // Time offset from device
    std::chrono::steady_clock::time_point last_seen; // Last seen time
    
    DeviceInfo() : online(false), timestamp_offset(0) {}
};

/**
 * @brief Device status change callback
 * Parameters: did, device_info
 */
using DeviceStatusCallback = std::function<void(const std::string&, const DeviceInfo&)>;

/**
 * @brief MIoT LAN Device Discovery Class
 * 
 * This class implements Xiaomi's OTU (One Touch) protocol for local network device discovery.
 * It broadcasts probe messages and listens for responses from Xiaomi devices.
 * 
 * Protocol Details:
 * - Header: 0x21 0x31 ("!1")
 * - Port: 54321 (UDP)
 * - Probe Message: 32 bytes
 * - Response Message: Variable length (up to 1400 bytes)
 */
class MIoTLanDiscovery {
public:
    /**
     * @brief Constructor
     * @param interfaces Network interface names to scan (e.g., "en0", "eth0")
     * @param virtual_did Virtual device ID (0 = auto-generate random ID)
     */
    explicit MIoTLanDiscovery(const std::vector<std::string>& interfaces = {}, uint64_t virtual_did = 0);
    
    /**
     * @brief Destructor
     */
    ~MIoTLanDiscovery();
    
    // Disable copy
    MIoTLanDiscovery(const MIoTLanDiscovery&) = delete;
    MIoTLanDiscovery& operator=(const MIoTLanDiscovery&) = delete;
    
    /**
     * @brief Start device discovery
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop device discovery
     */
    void stop();
    
    /**
     * @brief Check if discovery is running
     * @return true if running
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief Send a probe message to discover devices
     * @param interface_name Network interface (empty = all interfaces)
     * @param target_ip Target IP address (empty = broadcast 255.255.255.255)
     */
    void ping(const std::string& interface_name = "", const std::string& target_ip = "");
    
    /**
     * @brief Get all discovered devices
     * @return Map of device ID to device info
     */
    std::map<std::string, DeviceInfo> get_devices() const;
    
    /**
     * @brief Get device by DID
     * @param did Device ID
     * @return Device info (nullptr if not found)
     */
    std::shared_ptr<DeviceInfo> get_device(const std::string& did) const;
    
    /**
     * @brief Register callback for device status changes
     * @param key Unique identifier for this callback
     * @param callback Callback function
     */
    void register_callback(const std::string& key, DeviceStatusCallback callback);
    
    /**
     * @brief Unregister callback
     * @param key Unique identifier
     */
    void unregister_callback(const std::string& key);
    
    /**
     * @brief Set scan intervals
     * @param min_interval Minimum interval in seconds (default: 5s)
     * @param max_interval Maximum interval in seconds (default: 45s)
     */
    void set_scan_intervals(double min_interval, double max_interval);
    
    /**
     * @brief Set device timeout
     * @param timeout Timeout in seconds (default: 100s)
     */
    void set_device_timeout(double timeout);

private:
    // OTU Protocol Constants
    static constexpr uint16_t OT_PORT = 54321;
    static constexpr size_t OT_PROBE_LEN = 32;
    static constexpr size_t OT_MSG_LEN = 1400;
    static constexpr uint8_t OT_HEADER[2] = {0x21, 0x31};  // "!1"
    
    struct SocketInfo {
        int fd;
        std::string interface;
    };
    
    // Configuration
    std::vector<std::string> interfaces_;
    uint64_t virtual_did_;
    std::vector<uint8_t> probe_msg_;
    
    // Runtime state
    std::atomic<bool> running_;
    std::thread discovery_thread_;
    std::thread timeout_checker_thread_;
    
    // Sockets
    std::map<std::string, SocketInfo> sockets_;
    mutable std::mutex sockets_mutex_;
    
    // Devices
    std::map<std::string, std::shared_ptr<DeviceInfo>> devices_;
    mutable std::mutex devices_mutex_;
    
    // Callbacks
    std::map<std::string, DeviceStatusCallback> callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // Scan configuration
    double min_scan_interval_;
    double max_scan_interval_;
    double current_scan_interval_;
    double device_timeout_;
    
    // Private methods
    void init_probe_message();
    bool create_socket(const std::string& interface_name);
    void close_all_sockets();
    void discovery_loop();
    void timeout_checker_loop();
    void send_probe(const std::string& interface_name = "", const std::string& target_ip = "");
    void handle_received_data(const uint8_t* data, size_t len, const std::string& from_ip, const std::string& interface_name);
    void update_device(const std::string& did, const std::string& ip, const std::string& interface_name, int64_t timestamp_offset);
    void check_device_timeouts();
    void notify_callbacks(const std::string& did, const DeviceInfo& info);
    double get_next_scan_interval();
    std::string get_local_ip(const std::string& interface_name);
};

} // namespace miot

#endif // MIOT_LAN_DEVICE_H

