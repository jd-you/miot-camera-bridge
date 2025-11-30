#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>

namespace miot {

// 简单的HTTP服务器，用于接收OAuth回调
class SimpleHttpServer {
public:
    using CallbackHandler = std::function<void(const std::string& code, const std::string& state)>;
    
    SimpleHttpServer(int port = 8888);
    ~SimpleHttpServer();
    
    // 启动服务器
    bool start(CallbackHandler callback);
    
    // 停止服务器
    void stop();
    
    // 是否正在运行
    bool is_running() const { return running_; }
    
private:
    int port_;
    int server_fd_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    CallbackHandler callback_;
    
    void server_loop();
    void handle_request(int client_fd);
    std::string parse_query_param(const std::string& query, const std::string& key);
    std::string url_decode(const std::string& str);
};

} // namespace miot

#endif // HTTP_SERVER_H

