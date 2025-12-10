#include "miot_oauth.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <openssl/sha.h>
#include "http_server.h"

using json = nlohmann::json;

namespace miot {

#ifdef __APPLE__
#include <cstdlib>
#define OPEN_BROWSER(url) std::system(("open \"" + url + "\" 2>/dev/null").c_str())
#else
#include <cstdlib>
#define OPEN_BROWSER(url) std::system(("xdg-open \"" + url + "\" 2>/dev/null || firefox \"" + url + "\" 2>/dev/null || google-chrome \"" + url + "\" 2>/dev/null").c_str())
#endif

// CURL回调函数
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

MiotOAuth::MiotOAuth(const std::string& client_id, 
                     const std::string& redirect_uri,
                     const std::string& cloud_server,
                     const std::string& token_file)
    : client_id_(client_id)
    , redirect_uri_(redirect_uri)
    , cloud_server_(cloud_server)
    , token_file_(token_file) {
    
    // 设置OAuth服务器地址
    if (cloud_server == "cn") {
        oauth_host_ = "mico.api.mijia.tech";
    } else {
        oauth_host_ = cloud_server + ".mico.api.mijia.tech";
    }
    
    generate_ids();
    
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

MiotOAuth::~MiotOAuth() {
    curl_global_cleanup();
}

void MiotOAuth::generate_ids() {
    // 生成随机device_id
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    uint64_t random_id = dis(gen);
    
    std::stringstream ss;
    ss << "mico." << std::hex << random_id;
    device_id_ = ss.str();
    
    // 生成state（SHA1哈希）
    std::string state_input = "d=" + device_id_;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)state_input.c_str(), state_input.length(), hash);
    
    ss.str("");
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::setw(2) << (int)hash[i];
    }
    state_ = ss.str();
}

bool MiotOAuth::init(const std::string& token_file) {
    return load_token(token_file);
}

std::string MiotOAuth::url_encode(const std::string& value) const {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    
    char* encoded = curl_easy_escape(curl, value.c_str(), value.length());
    std::string result(encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);
    
    return result;
}

std::string MiotOAuth::generate_auth_url() const {
    std::stringstream url;
    url << "https://account.xiaomi.com/oauth2/authorize?"
        << "client_id=" << client_id_
        << "&redirect_uri=" << url_encode(redirect_uri_)
        << "&response_type=code"
        << "&device_id=" << device_id_
        << "&state=" << state_
        << "&skip_confirm=false";
    return url.str();
}

bool MiotOAuth::exchange_code_for_token(const std::string& code, const std::string& state) {
    // 验证state
    if (state != state_) {
        std::cerr << "State mismatch! Possible CSRF attack." << std::endl;
        return false;
    }
    
    // 构造请求数据
    json request_data = {
        {"client_id", client_id_},
        {"redirect_uri", redirect_uri_},
        {"code", code},
        {"device_id", device_id_}
    };
    
    // 发送请求
    std::string url = "https://" + oauth_host_ + "/app/v2/mico/oauth/get_token?data=" 
                    + url_encode(request_data.dump());
    
    std::cout << "Requesting token from: " << oauth_host_ << std::endl;
    std::string response = http_get(url);
    
    if (response.empty()) {
        std::cerr << "Empty response from server" << std::endl;
        return false;
    }
    
    try {
        auto response_json = json::parse(response);
        
        if (response_json["code"] != 0) {
            std::cerr << "Failed to get token: " << response << std::endl;
            return false;
        }
        
        auto result = response_json["result"];
        token_.access_token = result["access_token"];
        token_.refresh_token = result["refresh_token"];
        
        int expires_in = result["expires_in"];
        token_.expires_at = std::chrono::system_clock::now() 
                          + std::chrono::seconds(static_cast<int>(expires_in * 0.7));
        
        std::cout << "✓ Successfully obtained access token!" << std::endl;
        std::cout << "Token expires in: " << expires_in << " seconds" << std::endl;
        
        return save_token();
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing token response: " << e.what() << std::endl;
        std::cerr << "Response: " << response << std::endl;
        return false;
    }
}

bool MiotOAuth::refresh_token() {
    if (token_.refresh_token.empty()) {
        std::cerr << "No refresh token available" << std::endl;
        return false;
    }
    
    // 构造请求数据
    json request_data = {
        {"client_id", client_id_},
        {"redirect_uri", redirect_uri_},
        {"refresh_token", token_.refresh_token}
    };
    
    std::string url = "https://" + oauth_host_ + "/app/v2/mico/oauth/get_token?data=" 
                    + url_encode(request_data.dump());
    
    std::string response = http_get(url);
    
    if (response.empty()) {
        std::cerr << "Empty response from server" << std::endl;
        return false;
    }
    
    try {
        auto response_json = json::parse(response);
        
        if (response_json["code"] != 0) {
            std::cerr << "Failed to refresh token: " << response << std::endl;
            return false;
        }
        
        auto result = response_json["result"];
        token_.access_token = result["access_token"];
        token_.refresh_token = result["refresh_token"];
        
        int expires_in = result["expires_in"];
        token_.expires_at = std::chrono::system_clock::now() 
                          + std::chrono::seconds(static_cast<int>(expires_in * 0.7));
        
        std::cout << "✓ Token refreshed successfully!" << std::endl;
        
        return save_token();
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing refresh response: " << e.what() << std::endl;
        return false;
    }
}

bool MiotOAuth::save_token(const std::string& filename) const {
    try {
        json token_json = {
            {"access_token", token_.access_token},
            {"refresh_token", token_.refresh_token},
            {"expires_at", std::chrono::system_clock::to_time_t(token_.expires_at)}
        };
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return false;
        }
        
        file << token_json.dump(4);
        file.close();
        
        std::cout << "✓ Token saved to: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving token: " << e.what() << std::endl;
        return false;
    }
}

bool MiotOAuth::load_token(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "No existing token file found at: " << filename << std::endl;
            return false;
        }
        
        json token_json;
        file >> token_json;
        file.close();
        
        token_.access_token = token_json["access_token"];
        token_.refresh_token = token_json["refresh_token"];
        
        time_t expires_time = token_json["expires_at"];
        token_.expires_at = std::chrono::system_clock::from_time_t(expires_time);
        
        std::cout << "✓ Token loaded from: " << filename << std::endl;
        
        // 检查是否需要刷新
        if (token_.is_expired()) {
            std::cout << "⚠ Token has expired, need to re-authenticate" << std::endl;
            return false;
        } else if (token_.needs_refresh()) {
            std::cout << "Token needs refresh, refreshing..." << std::endl;
            return refresh_token();
        }
        
        std::cout << "✓ Token is valid" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading token: " << e.what() << std::endl;
        return false;
    }
}

bool MiotOAuth::is_token_valid() const {
    return !token_.access_token.empty() && !token_.is_expired();
}

std::string MiotOAuth::http_get(const std::string& url) const {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_easy_cleanup(curl);
    }
    
    return response;
}

std::string MiotOAuth::http_post(const std::string& url, const std::string& data) const {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_easy_cleanup(curl);
    }
    
    return response;
}

bool MiotOAuth::start_auth_flow() {

    should_exit_ = false;
    token_refresh_thread_ = std::thread([this]() { token_refresh_loop(); });


    return true;
}

bool MiotOAuth::stop_auth_flow() {
    should_exit_ = true;
    token_refresh_thread_.join();
    return true;
}

void MiotOAuth::token_refresh_loop() {

    std::cout << "[MiotOAuth] Token refresh loop started" << std::endl;
    // 尝试加载已有token
    std::cout << "Checking for existing token..." << std::endl;
    std::cout << "═══════════════════════════════════════════════════════════\n\n";
    
    if (!init(token_file_)) {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "需要进行小米账号授权 / Xiaomi Account Authorization Required\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "\n";
        
        // 创建HTTP服务器接收回调
        miot::SimpleHttpServer server(8888);
        
        bool auth_success = false;
        
        // 设置回调处理函数
        auto callback = [&](const std::string& code, const std::string& state) {
            std::cout << "\n";
            std::cout << "═══════════════════════════════════════════════════════════\n";
            std::cout << "Exchanging authorization code for token...\n";
            std::cout << "═══════════════════════════════════════════════════════════\n";
            auth_success = exchange_code_for_token(code, state);
        };
        
        // 生成授权URL
        std::string auth_url = generate_auth_url();
        
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

        // 启动HTTP服务器
        if (!server.start(callback)) {
            std::cerr << "✗ Failed to start HTTP server" << std::endl;
            std::cerr << "  Please check if port 8888 is already in use" << std::endl;
            return;
        }
        
        // 等待授权完成
        while (server.is_running() && !should_exit_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (should_exit_) {
            std::cout << "\nAuthorization cancelled by user\n";
            return;
        }
        
        if (!auth_success) {
            std::cerr << "\n✗ Authorization failed\n";
            return;
        }

        std::cout << "Authorization successful\n";
    }
    
    // 验证token
    if (!is_token_valid()) {
        std::cerr << "\n✗ Token is invalid\n";
        return;
    }
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              ✓ Authorization Successful                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Access Token: " << token_.access_token.substr(0, 30) << "...\n";
    std::cout << "\n";
    
    // 计算token过期时间
    auto now = std::chrono::system_clock::now();
    auto expires_at = token_.expires_at;
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
    while (!should_exit_) {
        if (token_.needs_refresh()) {
            std::cout << "\n[" << ++check_count << "] Token expiring soon, refreshing...\n";
            if (!refresh_token()) {
                std::cerr << "✗ Failed to refresh token\n";
                std::cerr << "  Please re-run the program to re-authenticate\n";
                return;
            }
            
            // 显示新的过期时间
            now = std::chrono::system_clock::now();
            expires_at = token_.expires_at;
            duration = std::chrono::duration_cast<std::chrono::minutes>(expires_at - now);
            std::cout << "New token expires in: " << duration.count() << " minutes\n";
        }
        
        // 每分钟检查一次
        for (int i = 0; i < 60 && !should_exit_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "\n✓ Program exited gracefully\n\n";
}

bool MiotOAuth::get_token(TokenInfo& token,std::chrono::seconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + timeout;
    
    while (std::chrono::steady_clock::now() < end_time) {
        if (is_token_valid()) {
            token = token_;
            return true;
        }
        
        // 每100ms检查一次
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 超时，最后再检查一次
    if (is_token_valid()) {
        token = token_;
        return true;
    }
    
    return false;
}
} // namespace miot

