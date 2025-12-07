/**
 * MIoT LAN Device Discovery - Implementation
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#include "miot_lan_device.h"

#include <iostream>
#include <cstring>
#include <ctime>
#include <random>
#include <algorithm>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
    #define SOCKET_ERROR_CODE WSAGetLastError()
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <ifaddrs.h>
    #define SOCKET_ERROR_CODE errno
    #define CLOSE_SOCKET close
#endif

namespace miot {

namespace {

// Generate random 64-bit number
uint64_t generate_random_did() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

// Convert uint64 to network byte order (big endian)
void write_uint64_be(uint8_t* buf, uint64_t value) {
    buf[0] = (value >> 56) & 0xFF;
    buf[1] = (value >> 48) & 0xFF;
    buf[2] = (value >> 40) & 0xFF;
    buf[3] = (value >> 32) & 0xFF;
    buf[4] = (value >> 24) & 0xFF;
    buf[5] = (value >> 16) & 0xFF;
    buf[6] = (value >> 8) & 0xFF;
    buf[7] = value & 0xFF;
}

// Read uint64 from network byte order (big endian)
uint64_t read_uint64_be(const uint8_t* buf) {
    return ((uint64_t)buf[0] << 56) |
           ((uint64_t)buf[1] << 48) |
           ((uint64_t)buf[2] << 40) |
           ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) |
           ((uint64_t)buf[5] << 16) |
           ((uint64_t)buf[6] << 8) |
           (uint64_t)buf[7];
}

// Read uint32 from network byte order (big endian)
uint32_t read_uint32_be(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

// Get current Unix timestamp
int64_t get_current_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // anonymous namespace

MIoTLanDiscovery::MIoTLanDiscovery(
    const std::vector<std::string>& interfaces,
    uint64_t virtual_did
) : interfaces_(interfaces),
    virtual_did_(virtual_did ? virtual_did : generate_random_did()),
    running_(false),
    min_scan_interval_(5.0),
    max_scan_interval_(45.0),
    current_scan_interval_(5.0),
    device_timeout_(100.0)
{
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    
    init_probe_message();
    
    std::cout << "[MIoTLanDiscovery] Initialized with virtual DID: " 
              << virtual_did_ << std::endl;
}

MIoTLanDiscovery::~MIoTLanDiscovery() {
    stop();
    
#ifdef _WIN32
    WSACleanup();
#endif
}

void MIoTLanDiscovery::init_probe_message() {
    probe_msg_.resize(OT_PROBE_LEN, 0);
    
    // Header: !1
    probe_msg_[0] = 0x21;
    probe_msg_[1] = 0x31;
    
    // Length: 0x0020 (32 bytes)
    probe_msg_[2] = 0x00;
    probe_msg_[3] = 0x20;
    
    // Unknown bytes (0xFF * 12)
    for (int i = 4; i < 16; i++) {
        probe_msg_[i] = 0xFF;
    }
    
    // Magic: "MDID"
    probe_msg_[16] = 'M';
    probe_msg_[17] = 'D';
    probe_msg_[18] = 'I';
    probe_msg_[19] = 'D';
    
    // Virtual DID (8 bytes, big endian)
    write_uint64_be(&probe_msg_[20], virtual_did_);
    
    // Padding (4 bytes)
    probe_msg_[28] = 0x00;
    probe_msg_[29] = 0x00;
    probe_msg_[30] = 0x00;
    probe_msg_[31] = 0x00;
}

bool MIoTLanDiscovery::start() {
    if (running_) {
        std::cerr << "[MIoTLanDiscovery] Already running" << std::endl;
        return false;
    }
    
    // Get available network interfaces if not specified
    if (interfaces_.empty()) {
#ifndef _WIN32
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                    // Skip loopback
                    if (!(ifa->ifa_flags & IFF_LOOPBACK)) {
                        interfaces_.push_back(ifa->ifa_name);
                    }
                }
            }
            freeifaddrs(ifaddr);
        }
#else
        // On Windows, use default interface
        interfaces_.push_back("");
#endif
    }
    
    if (interfaces_.empty()) {
        std::cerr << "[MIoTLanDiscovery] No network interfaces found" << std::endl;
        return false;
    }
    
    std::cout << "[MIoTLanDiscovery] Using interfaces: ";
    for (const auto& iface : interfaces_) {
        std::cout << iface << " ";
    }
    std::cout << std::endl;
    
    // Create sockets for each interface
    for (const auto& iface : interfaces_) {
        if (!create_socket(iface)) {
            std::cerr << "[MIoTLanDiscovery] Failed to create socket for interface: " 
                      << iface << std::endl;
        }
    }
    
    if (sockets_.empty()) {
        std::cerr << "[MIoTLanDiscovery] No sockets created" << std::endl;
        return false;
    }
    
    running_ = true;
    
    // Start discovery thread
    discovery_thread_ = std::thread([this]() { discovery_loop(); });
    
    // Start timeout checker thread
    timeout_checker_thread_ = std::thread([this]() { timeout_checker_loop(); });
    
    std::cout << "[MIoTLanDiscovery] Started" << std::endl;
    return true;
}

void MIoTLanDiscovery::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Wait for threads to finish
    if (discovery_thread_.joinable()) {
        discovery_thread_.join();
    }
    if (timeout_checker_thread_.joinable()) {
        timeout_checker_thread_.join();
    }
    
    close_all_sockets();
    
    std::cout << "[MIoTLanDiscovery] Stopped" << std::endl;
}

bool MIoTLanDiscovery::create_socket(const std::string& interface_name) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        std::cerr << "[MIoTLanDiscovery] Failed to create socket: " 
                  << SOCKET_ERROR_CODE << std::endl;
        return false;
    }
    
    // Set socket options
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, 
                   reinterpret_cast<const char*>(&broadcast), sizeof(broadcast)) < 0) {
        std::cerr << "[MIoTLanDiscovery] Failed to set SO_BROADCAST: " 
                  << SOCKET_ERROR_CODE << std::endl;
        CLOSE_SOCKET(sock);
        return false;
    }
    
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) < 0) {
        std::cerr << "[MIoTLanDiscovery] Failed to set SO_REUSEADDR: " 
                  << SOCKET_ERROR_CODE << std::endl;
        CLOSE_SOCKET(sock);
        return false;
    }
    
#ifndef _WIN32
    // Bind to specific interface (Linux/macOS)
    if (!interface_name.empty()) {
        if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, 
                       interface_name.c_str(), interface_name.length()) < 0) {
            std::cerr << "[MIoTLanDiscovery] Failed to bind to device " 
                      << interface_name << ": " << SOCKET_ERROR_CODE << std::endl;
            // Continue anyway - will bind to all interfaces
        }
    }
#endif
    
    // Bind to port
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;  // Let system choose port
    
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[MIoTLanDiscovery] Failed to bind socket: " 
                  << SOCKET_ERROR_CODE << std::endl;
        CLOSE_SOCKET(sock);
        return false;
    }
    
    // Set non-blocking mode
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    
    // Get bound port
    socklen_t addr_len = sizeof(addr);
    getsockname(sock, reinterpret_cast<struct sockaddr*>(&addr), &addr_len);
    
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    sockets_[interface_name] = {sock, interface_name};
    
    std::cout << "[MIoTLanDiscovery] Created socket for interface " 
              << interface_name << " on port " << ntohs(addr.sin_port) << std::endl;
    
    return true;
}

void MIoTLanDiscovery::close_all_sockets() {
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    for (auto& pair : sockets_) {
        CLOSE_SOCKET(pair.second.fd);
    }
    sockets_.clear();
}

void MIoTLanDiscovery::discovery_loop() {
    std::cout << "[MIoTLanDiscovery] Discovery loop started" << std::endl;
    
    // Initial random delay (0-3 seconds)
    std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 3000));
    
    while (running_) {
        // Send probe message
        send_probe();
        
        // Wait for scan interval
        auto scan_interval = get_next_scan_interval();
        auto start = std::chrono::steady_clock::now();
        
        while (running_) {
            // Check for received data
            std::vector<uint8_t> buffer(OT_MSG_LEN);
            struct sockaddr_in from_addr;
            socklen_t from_len = sizeof(from_addr);
            
            std::lock_guard<std::mutex> lock(sockets_mutex_);
            for (const auto& pair : sockets_) {
                ssize_t recv_len = recvfrom(
                    pair.second.fd, 
                    reinterpret_cast<char*>(buffer.data()), 
                    buffer.size(),
                    0,
                    reinterpret_cast<struct sockaddr*>(&from_addr),
                    &from_len
                );
                
                if (recv_len > 0) {
                    // Check if from OT port
                    if (ntohs(from_addr.sin_port) == OT_PORT) {
                        char from_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &from_addr.sin_addr, from_ip, INET_ADDRSTRLEN);
                        handle_received_data(
                            buffer.data(), 
                            recv_len, 
                            from_ip, 
                            pair.second.interface
                        );
                    }
                }
            }
            
            // Check if it's time for next scan
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            
            if (elapsed >= scan_interval) {
                break;
            }
            
            // Sleep for a short time
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    std::cout << "[MIoTLanDiscovery] Discovery loop stopped" << std::endl;
}

void MIoTLanDiscovery::timeout_checker_loop() {
    std::cout << "[MIoTLanDiscovery] Timeout checker started" << std::endl;
    
    while (running_) {
        check_device_timeouts();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    std::cout << "[MIoTLanDiscovery] Timeout checker stopped" << std::endl;
}

void MIoTLanDiscovery::send_probe(const std::string& interface_name, const std::string& target_ip) {
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(OT_PORT);
    
    if (target_ip.empty()) {
        addr.sin_addr.s_addr = INADDR_BROADCAST;
    } else {
        inet_pton(AF_INET, target_ip.c_str(), &addr.sin_addr);
    }
    
    std::lock_guard<std::mutex> lock(sockets_mutex_);
    
    if (interface_name.empty()) {
        // Broadcast to all interfaces
        for (const auto& pair : sockets_) {
            sendto(
                pair.second.fd,
                reinterpret_cast<const char*>(probe_msg_.data()),
                probe_msg_.size(),
                0,
                reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)
            );
        }
    } else {
        // Send to specific interface
        auto it = sockets_.find(interface_name);
        if (it != sockets_.end()) {
            sendto(
                it->second.fd,
                reinterpret_cast<const char*>(probe_msg_.data()),
                probe_msg_.size(),
                0,
                reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)
            );
        }
    }
}

void MIoTLanDiscovery::ping(const std::string& interface_name, const std::string& target_ip) {
    send_probe(interface_name, target_ip);
}

void MIoTLanDiscovery::handle_received_data(
    const uint8_t* data, 
    size_t len, 
    const std::string& from_ip,
    const std::string& interface_name
) {
    // Check minimum length
    if (len < 16) {
        return;
    }
    
    // Check header
    if (data[0] != OT_HEADER[0] || data[1] != OT_HEADER[1]) {
        return;
    }
    
    // Parse DID (bytes 4-11, big endian)
    uint64_t did_num = read_uint64_be(&data[4]);
    std::string did = std::to_string(did_num);
    
    // Parse timestamp (bytes 12-15, big endian)
    uint32_t device_timestamp = read_uint32_be(&data[12]);
    int64_t current_timestamp = get_current_timestamp();
    int64_t timestamp_offset = current_timestamp - device_timestamp;
    
    // Check if this is a probe response (32 bytes)
    if (len == OT_PROBE_LEN) {
        update_device(did, from_ip, interface_name, timestamp_offset);
    }
}

void MIoTLanDiscovery::update_device(
    const std::string& did,
    const std::string& ip,
    const std::string& interface_name,
    int64_t timestamp_offset
) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(did);
    bool is_new = (it == devices_.end());
    bool status_changed = false;
    
    if (is_new) {
        // New device
        auto device = std::make_shared<DeviceInfo>();
        device->did = did;
        device->ip = ip;
        device->interface = interface_name;
        device->online = true;
        device->timestamp_offset = timestamp_offset;
        device->last_seen = std::chrono::steady_clock::now();
        device->status_changed_type = DeviceStatusChangedType::NEW;
        
        devices_[did] = device;
        status_changed = true;
    } else {
        // Existing device
        auto& device = it->second;
        
        if (!device->online) {
            device->online = true;
            status_changed = true;
            device->status_changed_type = DeviceStatusChangedType::ONLINE;
        }
        
        if (device->ip != ip) {
            device->ip = ip;
            status_changed = true;
            device->status_changed_type = DeviceStatusChangedType::IP_CHANGED;
        }
        
        if (device->interface != interface_name) {
            device->interface = interface_name;
            status_changed = true;
            device->status_changed_type = DeviceStatusChangedType::INTERFACE_CHANGED;
        }
        
        device->timestamp_offset = timestamp_offset;
        device->last_seen = std::chrono::steady_clock::now();
    }
    
    if (status_changed) {
        notify_callbacks(did, *devices_[did]);
    }
}

void MIoTLanDiscovery::check_device_timeouts() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : devices_) {
        auto& device = pair.second;
        
        if (device->online) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - device->last_seen
            ).count();
            
            if (elapsed >= device_timeout_) {
                device->online = false;
                device->status_changed_type = DeviceStatusChangedType::OFFLINE;
                std::cout << "[MIoTLanDiscovery] Device offline (timeout): " 
                          << device->did << std::endl;
                notify_callbacks(device->did, *device);
            }
        }
    }
}

void MIoTLanDiscovery::notify_callbacks(const std::string& did, const DeviceInfo& info) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    
    for (const auto& pair : callbacks_) {
        try {
            pair.second(did, info);
        } catch (const std::exception& e) {
            std::cerr << "[MIoTLanDiscovery] Callback error: " << e.what() << std::endl;
        }
    }
}

std::map<std::string, DeviceInfo> MIoTLanDiscovery::get_devices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::map<std::string, DeviceInfo> result;
    for (const auto& pair : devices_) {
        result[pair.first] = *pair.second;
    }
    return result;
}

std::shared_ptr<DeviceInfo> MIoTLanDiscovery::get_device(const std::string& did) const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(did);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

void MIoTLanDiscovery::register_callback(const std::string& key, DeviceStatusCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_[key] = callback;
}

void MIoTLanDiscovery::unregister_callback(const std::string& key) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_.erase(key);
}

void MIoTLanDiscovery::set_scan_intervals(double min_interval, double max_interval) {
    min_scan_interval_ = min_interval;
    max_scan_interval_ = max_interval;
    current_scan_interval_ = min_interval;
}

void MIoTLanDiscovery::set_device_timeout(double timeout) {
    device_timeout_ = timeout;
}

double MIoTLanDiscovery::get_next_scan_interval() {
    current_scan_interval_ = std::min(current_scan_interval_ * 2.0, max_scan_interval_);
    return current_scan_interval_;
}

std::string MIoTLanDiscovery::get_local_ip(const std::string& interface_name) {
#ifndef _WIN32
    struct ifaddrs* ifaddr = nullptr;
    std::string result;
    
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                if (interface_name.empty() || interface_name == ifa->ifa_name) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, 
                             &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr,
                             ip, INET_ADDRSTRLEN);
                    result = ip;
                    break;
                }
            }
        }
        freeifaddrs(ifaddr);
    }
    
    return result;
#else
    return "";
#endif
}

} // namespace miot

