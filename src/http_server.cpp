#include "http_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

namespace miot {

SimpleHttpServer::SimpleHttpServer(int port)
    : port_(port)
    , server_fd_(-1)
    , running_(false) {
}

SimpleHttpServer::~SimpleHttpServer() {
    stop();
}

bool SimpleHttpServer::start(CallbackHandler callback) {
    callback_ = callback;
    
    // 创建socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // 设置socket选项
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定端口
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << std::endl;
        close(server_fd_);
        return false;
    }
    
    // 监听
    if (listen(server_fd_, 3) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        close(server_fd_);
        return false;
    }
    
    std::cout << "✓ HTTP server listening on port " << port_ << std::endl;
    
    running_ = true;
    server_thread_ = std::thread(&SimpleHttpServer::server_loop, this);
    
    return true;
}

void SimpleHttpServer::stop() {
    // if (running_) {
        running_ = false;
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    // }
}

void SimpleHttpServer::server_loop() {
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        handle_request(client_fd);
        close(client_fd);
    }
}

std::string SimpleHttpServer::url_decode(const std::string& str) {
    std::string result;
    char ch;
    int i, ii;
    for (i = 0; i < str.length(); i++) {
        if (str[i] == '%') {
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            result += ch;
            i = i + 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

void SimpleHttpServer::handle_request(int client_fd) {
    char buffer[4096] = {0};
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        return;
    }
    
    std::string request(buffer);
    std::cout << "\n=== Received HTTP Request ===" << std::endl;
    
    // 解析请求行
    size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) {
        return;
    }
    
    std::string request_line = request.substr(0, line_end);
    std::cout << "Request: " << request_line << std::endl;
    
    // 提取路径和查询参数
    size_t path_start = request_line.find(' ') + 1;
    size_t path_end = request_line.find(' ', path_start);
    if (path_start == std::string::npos || path_end == std::string::npos) {
        return;
    }
    
    std::string path = request_line.substr(path_start, path_end - path_start);
    
    // 解析查询参数
    size_t query_start = path.find('?');
    if (query_start != std::string::npos) {
        std::string query = path.substr(query_start + 1);
        std::string code = parse_query_param(query, "code");
        std::string state = parse_query_param(query, "state");
        
        if (!code.empty() && !state.empty()) {
            std::cout << "✓ Received OAuth callback" << std::endl;
            std::cout << "  Code: " << code.substr(0, 20) << "..." << std::endl;
            std::cout << "  State: " << state.substr(0, 20) << "..." << std::endl;
            
            // 调用回调函数
            if (callback_) {
                callback_(code, state);
            }
            
            // 发送成功响应
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<!DOCTYPE html>"
                "<html><head>"
                "<meta charset='utf-8'>"
                "<title>授权成功</title>"
                "<style>"
                "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif; "
                "       text-align: center; padding: 50px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); "
                "       color: white; margin: 0; min-height: 100vh; display: flex; align-items: center; justify-content: center; }"
                ".container { background: rgba(255,255,255,0.1); padding: 40px; border-radius: 20px; "
                "             backdrop-filter: blur(10px); box-shadow: 0 8px 32px rgba(0,0,0,0.1); }"
                "h1 { font-size: 48px; margin: 20px 0; }"
                ".check { font-size: 80px; animation: scale 0.5s ease; }"
                "@keyframes scale { from { transform: scale(0); } to { transform: scale(1); } }"
                "p { font-size: 20px; margin: 20px 0; opacity: 0.9; }"
                "</style>"
                "</head><body>"
                "<div class='container'>"
                "<div class='check'>✓</div>"
                "<h1>授权成功</h1>"
                "<p>小米账号授权成功！</p>"
                "<p>您现在可以关闭此页面</p>"
                "</div>"
                "<script>setTimeout(function(){window.close();}, 3000);</script>"
                "</body></html>";
            
            write(client_fd, response.c_str(), response.length());
            
            // 停止服务器
            running_ = false;
            return;
        }
    }
    
    // 发送错误响应
    std::string response = 
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<html><body style='font-family:Arial; text-align:center; padding:50px;'>"
        "<h1 style='color:red;'>✗ Invalid Request</h1>"
        "<p>Missing required parameters</p>"
        "</body></html>";
    
    write(client_fd, response.c_str(), response.length());
}

std::string SimpleHttpServer::parse_query_param(const std::string& query, const std::string& key) {
    size_t pos = query.find(key + "=");
    if (pos == std::string::npos) {
        return "";
    }
    
    pos += key.length() + 1;
    size_t end = query.find('&', pos);
    
    std::string value;
    if (end == std::string::npos) {
        value = query.substr(pos);
    } else {
        value = query.substr(pos, end - pos);
    }
    
    return url_decode(value);
}

} // namespace miot

