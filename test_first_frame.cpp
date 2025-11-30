/**
 * Simple Camera Frame Test
 * 
 * Test program to get one raw frame from a MIoT camera.
 * 
 * Copyright (C) 2025
 */

#include "miot_camera_client.h"
#include "miot_cloud_client.h"
#include "miot_lan_device.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

// Global flag for graceful shutdown
std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    std::cout << "\n[Main] Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
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

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║           MIoT Camera - First Frame Test                               ║" << std::endl;
    std::cout << "║                    Copyright (C) 2025                                   ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    // Parse command line arguments
    std::string token_file;
    std::string did;
    std::string model;
    std::string pin_code;
    int channel_count = 1;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if ((arg == "-f" || arg == "--token-file") && i + 1 < argc) {
            token_file = argv[++i];
        } else if ((arg == "-d" || arg == "--did") && i + 1 < argc) {
            did = argv[++i];
        } else if ((arg == "-m" || arg == "--model") && i + 1 < argc) {
            model = argv[++i];
        } else if ((arg == "-p" || arg == "--pin") && i + 1 < argc) {
            pin_code = argv[++i];
        } else if ((arg == "-c" || arg == "--channels") && i + 1 < argc) {
            channel_count = std::stoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  -f, --token-file <path>   Access token file (required)\n"
                      << "  -d, --did <did>           Device ID (required)\n"
                      << "  -m, --model <model>       Device model (required)\n"
                      << "  -p, --pin <code>          4-digit PIN code (optional)\n"
                      << "  -c, --channels <count>    Channel count (default: 1)\n"
                      << "  -h, --help                Show this help\n\n"
                      << "Example:\n"
                      << "  " << argv[0] << " -f token.txt -d 123456789 -m xiaomi.camera.082ac1 -p 1234\n"
                      << std::endl;
            return 0;
        }
    }
    
    // Validate required arguments
    if (token_file.empty() || did.empty() || model.empty()) {
        std::cerr << "Error: Missing required arguments!" << std::endl;
        std::cerr << "Use --help for usage information." << std::endl;
        return 1;
    }
    
    // Load access token
    std::string access_token = load_access_token(token_file);
    if (access_token.empty()) {
        std::cerr << "Error: Failed to load access token from: " << token_file << std::endl;
        return 1;
    }
    std::cout << "[Main] Loaded access token" << std::endl;
    
    // Register signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create camera client
    std::cout << "\n[Main] Creating camera client..." << std::endl;
    miot::MIoTCameraClient camera_client("cn", access_token);
    
    if (!camera_client.init()) {
        std::cerr << "Error: Failed to initialize camera client" << std::endl;
        return 1;
    }
    
    // Create camera instance
    std::cout << "[Main] Creating camera: " << did << " (" << model << ")" << std::endl;
    if (!camera_client.create_camera(did, model, channel_count)) {
        std::cerr << "Error: Failed to create camera" << std::endl;
        return 1;
    }
    
    // Frame counter
    std::atomic<int> frame_count(0);
    std::atomic<int> total_bytes(0);
    auto start_time = std::chrono::steady_clock::now();
    
    // Register callback for raw video frames
    camera_client.register_raw_video_callback(did, 0, 
        [&](const std::string& device_id, const miot::RawFrameData& frame) {
            frame_count++;
            total_bytes += frame.data.size();
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            
            std::cout << "\r[Frame] #" << frame_count 
                      << " | Size: " << frame.data.size() << " bytes"
                      << " | Type: " << (frame.frame_type == miot::FrameType::I_FRAME ? "I" : "P")
                      << " | Codec: " << (frame.codec_id == miot::CameraCodec::VIDEO_H264 ? "H264" : "H265")
                      << " | Timestamp: " << frame.timestamp
                      << " | Seq: " << frame.sequence
                      << " | Elapsed: " << elapsed << "s"
                      << " | Total: " << (total_bytes / 1024) << "KB"
                      << std::flush;
            
            // For first frame only, save to file
            if (frame_count == 1) {
                std::string filename = "first_frame_" + device_id + ".h264";
                std::ofstream file(filename, std::ios::binary);
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(frame.data.data()), frame.data.size());
                    file.close();
                    std::cout << "\n[Main] Saved first frame to: " << filename << std::endl;
                }
            }
        }
    );
    
    // Register status callback
    camera_client.register_status_callback(did,
        [](const std::string& device_id, miot::CameraStatus status) {
            std::cout << "\n[Status] Camera " << device_id << " status changed to: ";
            switch (status) {
                case miot::CameraStatus::DISCONNECTED:
                    std::cout << "DISCONNECTED";
                    break;
                case miot::CameraStatus::CONNECTING:
                    std::cout << "CONNECTING";
                    break;
                case miot::CameraStatus::RE_CONNECTING:
                    std::cout << "RE_CONNECTING";
                    break;
                case miot::CameraStatus::CONNECTED:
                    std::cout << "CONNECTED";
                    break;
                case miot::CameraStatus::ERROR:
                    std::cout << "ERROR";
                    break;
            }
            std::cout << std::endl;
        }
    );
    
    // Start camera
    std::cout << "\n[Main] Starting camera..." << std::endl;
    if (pin_code.empty()) {
        std::cout << "[Main] Note: No PIN code provided. This may fail if camera requires PIN." << std::endl;
    }
    
    if (!camera_client.start_camera(did, pin_code, miot::VideoQuality::LOW, false)) {
        std::cerr << "Error: Failed to start camera" << std::endl;
        return 1;
    }
    
    std::cout << "[Main] Camera started! Waiting for frames..." << std::endl;
    std::cout << "[Main] Press Ctrl+C to stop\n" << std::endl;
    
    // Wait for frames
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check status
        auto status = camera_client.get_status(did);
        if (status == miot::CameraStatus::ERROR) {
            std::cerr << "\n[Main] Camera error detected, stopping..." << std::endl;
            break;
        }
    }
    
    std::cout << "\n\n[Main] Stopping camera..." << std::endl;
    camera_client.stop_camera(did);
    camera_client.destroy_camera(did);
    
    std::cout << "\n[Main] Summary:" << std::endl;
    std::cout << "  Total frames received: " << frame_count << std::endl;
    std::cout << "  Total data received: " << (total_bytes / 1024) << " KB" << std::endl;
    
    if (frame_count > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();
        if (elapsed > 0) {
            std::cout << "  Average FPS: " << (frame_count / elapsed) << std::endl;
        }
    }
    
    std::cout << "\n[Main] Done!" << std::endl;
    
    return 0;
}

