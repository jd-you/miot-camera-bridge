/**
 * MIoT Discovery with Cloud - Integrated Demo
 * 
 * This demo combines LAN device discovery with cloud API queries
 * to get complete device information including user-defined names.
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#include "miot_lan_device.h"
#include "miot_cloud_client.h"

#include <iostream>
#include <iomanip>
#include <signal.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <map>

// Global discovery instance for signal handler
miot::MIoTLanDiscovery* g_discovery = nullptr;
miot::MIoTCloudClient* g_cloud_client = nullptr;

// Signal handler for clean shutdown
void signal_handler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    if (g_discovery) {
        g_discovery->stop();
    }
}

// Print enhanced device information with cloud data
void print_devices_with_cloud_info(
    const std::map<std::string, miot::DeviceInfo>& lan_devices,
    const std::map<std::string, miot::CloudDeviceInfo>& cloud_devices
) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                         Discovered MIoT Devices (With Cloud Info)                                  ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
    
    if (lan_devices.empty()) {
        std::cout << "║  No devices found yet...                                                                           ║" << std::endl;
    } else {
        std::cout << "║ Status │ Device Name             │ Model                    │ IP Address       │ Interface      ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
        
        for (const auto& pair : lan_devices) {
            const auto& dev = pair.second;
            
            // Try to get cloud info
            std::string device_name = "Unknown";
            std::string model = "Unknown";
            
            auto cloud_it = cloud_devices.find(dev.did);
            if (cloud_it != cloud_devices.end()) {
                device_name = cloud_it->second.name;
                model = cloud_it->second.model;
                
                // Truncate long names
                if (device_name.length() > 23) {
                    device_name = device_name.substr(0, 20) + "...";
                }
                if (model.length() > 24) {
                    model = model.substr(0, 21) + "...";
                }
            }
            
            std::cout << "║ " 
                      << (dev.online ? "🟢 ON " : "🔴 OFF")
                      << " │ "
                      << std::left << std::setw(23) << device_name
                      << " │ "
                      << std::left << std::setw(24) << model
                      << " │ "
                      << std::left << std::setw(16) << dev.ip
                      << " │ "
                      << std::left << std::setw(14) << dev.interface
                      << " │ "
                      << std::left << std::setw(14) << dev.did
                      << " ║" << std::endl;
        }
        
        std::cout << "╠════════════════════════════════════════════════════════════════════════════════════════════════════╣" << std::endl;
        std::cout << "║  Total: " << lan_devices.size() << " devices (LAN), " 
                  << cloud_devices.size() << " with cloud info" << std::string(58, ' ') << "║" << std::endl;
    }
    
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════════════════════════╝" << std::endl;
}

// Print detailed device info
void print_device_detail(const std::string& did, const miot::CloudDeviceInfo& info) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                        Device Details                                   ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  DID:          " << std::left << std::setw(57) << info.did << "║" << std::endl;
    std::cout << "║  Name:         " << std::left << std::setw(57) << info.name << "║" << std::endl;
    std::cout << "║  Model:        " << std::left << std::setw(57) << info.model << "║" << std::endl;
    std::cout << "║  Manufacturer: " << std::left << std::setw(57) << info.manufacturer << "║" << std::endl;
    std::cout << "║  Status:       " << std::left << std::setw(57) << (info.online ? "Online" : "Offline") << "║" << std::endl;
    
    if (!info.local_ip.empty()) {
        std::cout << "║  Local IP:     " << std::left << std::setw(57) << info.local_ip << "║" << std::endl;
    }
    if (!info.ssid.empty()) {
        std::cout << "║  SSID:         " << std::left << std::setw(57) << info.ssid << "║" << std::endl;
    }
    if (info.rssi != 0) {
        std::cout << "║  RSSI:         " << std::left << std::setw(57) << std::to_string(info.rssi) + " dBm" << "║" << std::endl;
    }
    if (!info.fw_version.empty()) {
        std::cout << "║  Firmware:     " << std::left << std::setw(57) << info.fw_version << "║" << std::endl;
    }
    if (!info.token.empty()) {
        std::string token_display = info.token.substr(0, 16) + "...";
        std::cout << "║  Token:        " << std::left << std::setw(57) << token_display << "║" << std::endl;
    }
    std::cout << "╚════════════════════════════════════════════════════════════════════════╝" << std::endl;
}

// Load access token from file
std::string load_access_token(const std::string& token_file) {
    std::ifstream file(token_file);
    if (!file.is_open()) {
        return "";
    }
    
    std::string token;
    std::getline(file, token);
    file.close();
    
    // Trim whitespace
    token.erase(0, token.find_first_not_of(" \t\n\r"));
    token.erase(token.find_last_not_of(" \t\n\r") + 1);
    
    return token;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -i, --interface <name>   Network interface to scan (e.g., en0, eth0)\n"
              << "  -t, --token <token>      Access token for cloud API\n"
              << "  -f, --token-file <path>  Load access token from file\n"
              << "  -s, --server <region>    Cloud server region (cn, de, us, etc.)\n"
              << "  --timeout <seconds>      Device timeout in seconds (default: 100)\n"
              << "  -h, --help               Show this help message\n\n"
              << "Examples:\n"
              << "  " << program_name << " -f token.txt\n"
              << "  " << program_name << " -i en0 -t \"your_access_token_here\"\n"
              << "  " << program_name << " -f token.txt -s cn\n\n"
              << "Note:\n"
              << "  You need an access_token to query cloud device information.\n"
              << "  Save your token to a file (e.g., token.txt) and use -f option.\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║    MIoT Discovery with Cloud - Complete Device Information             ║" << std::endl;
    std::cout << "║                        Copyright (C) 2025                               ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // Parse command line arguments
    std::vector<std::string> interfaces;
    std::string access_token;
    std::string cloud_server = "cn";
    double device_timeout = 100.0;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-i" || arg == "--interface") && i + 1 < argc) {
            interfaces.push_back(argv[++i]);
        } else if ((arg == "-t" || arg == "--token") && i + 1 < argc) {
            access_token = argv[++i];
        } else if ((arg == "-f" || arg == "--token-file") && i + 1 < argc) {
            std::string token_file = argv[++i];
            access_token = load_access_token(token_file);
            if (access_token.empty()) {
                std::cerr << "Error: Failed to load token from file: " << token_file << std::endl;
                return 1;
            }
            std::cout << "[Main] Loaded access token from: " << token_file << std::endl;
        } else if ((arg == "-s" || arg == "--server") && i + 1 < argc) {
            cloud_server = argv[++i];
        } else if (arg == "--timeout" && i + 1 < argc) {
            device_timeout = std::stod(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate access token
    if (access_token.empty()) {
        std::cerr << "Error: Access token is required!" << std::endl;
        std::cerr << "Use -t <token> or -f <token_file> to provide the token." << std::endl;
        std::cerr << "\nRun with --help for more information." << std::endl;
        return 1;
    }
    
    // Create LAN discovery instance
    miot::MIoTLanDiscovery discovery(interfaces);
    g_discovery = &discovery;
    
    // Create cloud client
    miot::MIoTCloudClient cloud_client(access_token, cloud_server);
    g_cloud_client = &cloud_client;
    
    // Initialize cloud client
    std::cout << "[Main] Initializing cloud client..." << std::endl;
    if (!cloud_client.init()) {
        std::cerr << "[Main] Failed to initialize cloud client" << std::endl;
        return 1;
    }
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configure discovery
    discovery.set_device_timeout(device_timeout);
    
    // Start discovery
    std::cout << "[Main] Starting LAN device discovery..." << std::endl;
    if (!discovery.start()) {
        std::cerr << "[Main] Failed to start discovery" << std::endl;
        return 1;
    }
    
    // Wait for initial devices
    std::cout << "[Main] Scanning for devices (press Ctrl+C to stop)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(8));
    
    // Get LAN devices
    auto lan_devices = discovery.get_devices();
    std::cout << "\n[Main] Found " << lan_devices.size() << " device(s) on LAN" << std::endl;
    
    // Query cloud information for discovered devices
    std::map<std::string, miot::CloudDeviceInfo> cloud_devices;
    
    if (!lan_devices.empty()) {
        std::cout << "[Main] Querying cloud for device information..." << std::endl;
        
        std::vector<std::string> dids;
        for (const auto& pair : lan_devices) {
            dids.push_back(pair.first);
        }
        
        cloud_devices = cloud_client.get_devices(dids);
        std::cout << "[Main] Retrieved information for " << cloud_devices.size() << " device(s)" << std::endl;
    }
    
    // Print combined information
    print_devices_with_cloud_info(lan_devices, cloud_devices);
    
    // Print detailed info for each device
    if (!cloud_devices.empty()) {
        std::cout << "\n[Main] Detailed device information:\n" << std::endl;
        for (const auto& pair : cloud_devices) {
            print_device_detail(pair.first, pair.second);
        }
    }
    
    // Keep monitoring
    std::cout << "\n[Main] Monitoring devices (updates every 15 seconds)..." << std::endl;
    std::cout << "[Main] Press Ctrl+C to stop" << std::endl;
    
    int counter = 0;
    while (discovery.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        counter++;
        
        // Update every 15 seconds
        if (counter % 15 == 0) {
            lan_devices = discovery.get_devices();
            
            // Re-query cloud if there are new devices
            std::vector<std::string> new_dids;
            for (const auto& pair : lan_devices) {
                if (cloud_devices.find(pair.first) == cloud_devices.end()) {
                    new_dids.push_back(pair.first);
                }
            }
            
            if (!new_dids.empty()) {
                auto new_cloud_devices = cloud_client.get_devices(new_dids);
                cloud_devices.insert(new_cloud_devices.begin(), new_cloud_devices.end());
            }
            
            print_devices_with_cloud_info(lan_devices, cloud_devices);
        }
    }
    
    std::cout << "\n[Main] Discovery stopped" << std::endl;
    
    return 0;
}

