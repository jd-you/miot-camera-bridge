#include "miot_oauth.h"
#include "http_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#ifdef __APPLE__
#include <cstdlib>
#define OPEN_BROWSER(url) std::system(("open \"" + url + "\" 2>/dev/null").c_str())
#elif defined(_WIN32)
#include <windows.h>
#define OPEN_BROWSER(url) ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL)
#else
#include <cstdlib>
#define OPEN_BROWSER(url) std::system(("xdg-open \"" + url + "\" 2>/dev/null || firefox \"" + url + "\" 2>/dev/null || google-chrome \"" + url + "\" 2>/dev/null").c_str())
#endif

const std::string CLIENT_ID = "2882303761520431603";
const std::string REDIRECT_URI = "https://mico.api.mijia.tech/login_redirect";
const std::string CLOUD_SERVER = "cn";
const std::string TOKEN_FILE = "miot_token.json";

std::atomic<bool> should_exit(false);

void signal_handler(int signal) {
    std::cout << "\n\nReceived interrupt signal, exiting..." << std::endl;
    should_exit = true;
}

void print_banner() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║        小米IoT摄像头桥接程序 / Xiaomi IoT Camera Bridge       ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║        OAuth2 Token Manager with Auto-Refresh              ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

int main() {
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    print_banner();
    
    // 创建OAuth客户端
    miot::MiotOAuth oauth(CLIENT_ID, REDIRECT_URI, CLOUD_SERVER);
    
    // 尝试加载已有token
    std::cout << "Checking for existing token..." << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════\n\n";
    
    if (!oauth.init(TOKEN_FILE)) {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "需要进行小米账号授权 / Xiaomi Account Authorization Required\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "\n";
        
        // 创建HTTP服务器接收回调
        miot::SimpleHttpServer server(8000);
        
        bool auth_success = false;
        
        // 设置回调处理函数
        auto callback = [&](const std::string& code, const std::string& state) {
            std::cout << "\n";
            std::cout << "═══════════════════════════════════════════════════════════\n";
            std::cout << "Exchanging authorization code for token...\n";
            std::cout << "═══════════════════════════════════════════════════════════\n";
            auth_success = oauth.exchange_code_for_token(code, state);
        };
        
        // 启动HTTP服务器
        if (!server.start(callback)) {
            std::cerr << "✗ Failed to start HTTP server" << std::endl;
            std::cerr << "  Please check if port 8888 is already in use" << std::endl;
            return 1;
        }
        
        // 生成授权URL
        std::string auth_url = oauth.generate_auth_url();
        
        std::cout << "\n请在浏览器中打开以下URL进行授权：\n";
        std::cout << "Please open the following URL in your browser:\n\n";
        std::cout << "┌────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << auth_url << "\n";
        std::cout << "└────────────────────────────────────────────────────────────┘\n";
        std::cout << "\n";
        
        // 尝试自动打开浏览器
        std::cout << "Attempting to open browser automatically...\n";
        int open_result = OPEN_BROWSER(auth_url);
        if (open_result == 0) {
            std::cout << "✓ Browser opened successfully\n";
        } else {
            std::cout << "⚠ Please manually open the URL above\n";
        }
        
        std::cout << "\n";
        std::cout << "Waiting for authorization...\n";
        std::cout << "(Press Ctrl+C to cancel)\n";
        std::cout << "\n";
        
        // 等待授权完成
        while (server.is_running() && !should_exit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (should_exit) {
            std::cout << "\nAuthorization cancelled by user\n";
            return 1;
        }
        
        if (!auth_success) {
            std::cerr << "\n✗ Authorization failed\n";
            return 1;
        }

        std::cout << "Authorization successful\n";
    }
    
    // 验证token
    if (!oauth.is_token_valid()) {
        std::cerr << "\n✗ Token is invalid\n";
        return 1;
    }
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              ✓ Authorization Successful                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Access Token: " << oauth.get_token().access_token.substr(0, 30) << "...\n";
    std::cout << "\n";
    
    // 计算token过期时间
    auto now = std::chrono::system_clock::now();
    auto expires_at = oauth.get_token().expires_at;
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(expires_at - now);
    std::cout << "Token expires in: " << duration.count() << " minutes\n";
    std::cout << "\n";
    
    // 主循环：自动刷新token
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "Entering main loop - Token will auto-refresh when needed\n";
    std::cout << "Press Ctrl+C to exit\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "\n";
    
    int check_count = 0;
    while (!should_exit) {
        if (oauth.get_token().needs_refresh()) {
            std::cout << "\n[" << ++check_count << "] Token expiring soon, refreshing...\n";
            if (!oauth.refresh_token()) {
                std::cerr << "✗ Failed to refresh token\n";
                std::cerr << "  Please re-run the program to re-authenticate\n";
                return 1;
            }
            
            // 显示新的过期时间
            now = std::chrono::system_clock::now();
            expires_at = oauth.get_token().expires_at;
            duration = std::chrono::duration_cast<std::chrono::minutes>(expires_at - now);
            std::cout << "New token expires in: " << duration.count() << " minutes\n";
        }
        
        // 每分钟检查一次
        for (int i = 0; i < 60 && !should_exit; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "\n✓ Program exited gracefully\n\n";
    return 0;
}

