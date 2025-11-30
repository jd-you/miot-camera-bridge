/**
 * MIoT LAN Device Discovery - Demo Application
 * 
 * This demo shows how to use the MIoTLanDiscovery library to discover
 * Xiaomi IoT devices on the local network.
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#include "miot_lan_device.h"

#include <iostream>
#include <iomanip>
#include <signal.h>
#include <thread>
#include <chrono>

// Global discovery instance for signal handler
miot::MIoTLanDiscovery* g_discovery = nullptr;

// Signal handler for clean shutdown
void signal_handler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    if (g_discovery) {
        g_discovery->stop();
    }
}

// Print device information in a formatted table
void print_devices(const std::map<std::string, miot::DeviceInfo>& devices) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    Discovered MIoT Devices                              ║" << std::endl;
    std::cout << "╠════════════════════════════════════════════════════════════════════════╣" << std::endl;
    
    if (devices.empty()) {
        std::cout << "║  No devices found yet...                                               ║" << std::endl;
    } else {
        std::cout << "║ Status │ Device ID          │ IP Address       │ Interface           ║" << std::endl;
        std::cout << "╠════════════════════════════════════════════════════════════════════════╣" << std::endl;
        
        for (const auto& pair : devices) {
            const auto& dev = pair.second;
            std::cout << "║ " 
                      << (dev.online ? "🟢 ON " : "🔴 OFF")
                      << " │ "
                      << std::left << std::setw(18) << dev.did.substr(0, 18)
                      << " │ "
                      << std::left << std::setw(16) << dev.ip
                      << " │ "
                      << std::left << std::setw(19) << dev.interface
                      << " ║" << std::endl;
        }
    }
    
    std::cout << "╚════════════════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "\nTotal devices: " << devices.size() << std::endl;
}

// Device status change callback
void on_device_status_changed(const std::string& did, const miot::DeviceInfo& info) {
    std::cout << "\n[Callback] Device status changed:" << std::endl;
    std::cout << "  DID:       " << did << std::endl;
    std::cout << "  IP:        " << info.ip << std::endl;
    std::cout << "  Interface: " << info.interface << std::endl;
    std::cout << "  Status:    " << (info.online ? "ONLINE" : "OFFLINE") << std::endl;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -i, --interface <name>   Network interface to scan (e.g., en0, eth0)\n"
              << "                           Can be specified multiple times\n"
              << "                           If not specified, all interfaces will be used\n"
              << "  -d, --did <number>       Virtual device ID (default: random)\n"
              << "  -t, --timeout <seconds>  Device timeout in seconds (default: 100)\n"
              << "  --min-interval <secs>    Minimum scan interval (default: 5)\n"
              << "  --max-interval <secs>    Maximum scan interval (default: 45)\n"
              << "  -h, --help               Show this help message\n\n"
              << "Examples:\n"
              << "  " << program_name << "\n"
              << "  " << program_name << " -i en0\n"
              << "  " << program_name << " -i eth0 -i wlan0\n"
              << "  " << program_name << " --timeout 120 --min-interval 10\n\n"
              << "Press Ctrl+C to stop the discovery.\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         MIoT LAN Device Discovery - Xiaomi IoT Device Scanner          ║" << std::endl;
    std::cout << "║                        Copyright (C) 2025                               ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // Parse command line arguments
    std::vector<std::string> interfaces;
    uint64_t virtual_did = 0;
    double device_timeout = 100.0;
    double min_interval = 5.0;
    double max_interval = 45.0;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-i" || arg == "--interface") && i + 1 < argc) {
            interfaces.push_back(argv[++i]);
        } else if ((arg == "-d" || arg == "--did") && i + 1 < argc) {
            virtual_did = std::stoull(argv[++i]);
        } else if ((arg == "-t" || arg == "--timeout") && i + 1 < argc) {
            device_timeout = std::stod(argv[++i]);
        } else if (arg == "--min-interval" && i + 1 < argc) {
            min_interval = std::stod(argv[++i]);
        } else if (arg == "--max-interval" && i + 1 < argc) {
            max_interval = std::stod(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Create discovery instance
    miot::MIoTLanDiscovery discovery(interfaces, virtual_did);
    g_discovery = &discovery;
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configure discovery
    discovery.set_device_timeout(device_timeout);
    discovery.set_scan_intervals(min_interval, max_interval);
    
    // Register callback for device status changes
    discovery.register_callback("main", on_device_status_changed);
    
    // Start discovery
    std::cout << "[Main] Starting device discovery..." << std::endl;
    std::cout << "[Main] Configuration:" << std::endl;
    std::cout << "  Device timeout:    " << device_timeout << " seconds" << std::endl;
    std::cout << "  Min scan interval: " << min_interval << " seconds" << std::endl;
    std::cout << "  Max scan interval: " << max_interval << " seconds" << std::endl;
    std::cout << std::endl;
    
    if (!discovery.start()) {
        std::cerr << "[Main] Failed to start discovery" << std::endl;
        return 1;
    }
    
    // Wait for initial devices to be discovered
    std::cout << "[Main] Waiting for devices (press Ctrl+C to stop)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Periodic status update
    int counter = 0;
    while (discovery.is_running()) {
        // Print device list every 10 seconds
        if (counter % 10 == 0) {
            auto devices = discovery.get_devices();
            print_devices(devices);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        counter++;
    }
    
    std::cout << "\n[Main] Discovery stopped" << std::endl;
    
    // Print final device list
    auto final_devices = discovery.get_devices();
    std::cout << "\n[Main] Final device list:" << std::endl;
    print_devices(final_devices);
    
    return 0;
}

