#ifndef MIOT_OAUTH_H
#define MIOT_OAUTH_H

#include <string>
#include <functional>
#include <chrono>

namespace miot {

struct TokenInfo {
    std::string access_token;
    std::string refresh_token;
    std::chrono::system_clock::time_point expires_at;
    
    bool is_expired() const {
        return std::chrono::system_clock::now() >= expires_at;
    }
    
    bool needs_refresh() const {
        // 提前10分钟刷新
        auto now = std::chrono::system_clock::now();
        auto refresh_threshold = expires_at - std::chrono::minutes(10);
        return now >= refresh_threshold;
    }
};

class MiotOAuth {
public:
    MiotOAuth(const std::string& client_id, 
              const std::string& redirect_uri,
              const std::string& cloud_server = "cn");
    ~MiotOAuth();
    
    // 初始化（从文件加载token）
    bool init(const std::string& token_file = "miot_token.json");
    
    // 生成授权URL
    std::string generate_auth_url() const;
    
    // 启动OAuth流程（会启动本地服务器接收回调）
    bool start_auth_flow();
    
    // 用授权码换取token
    bool exchange_code_for_token(const std::string& code, const std::string& state);
    
    // 刷新token
    bool refresh_token();
    
    // 获取当前token
    const TokenInfo& get_token() const { return token_; }
    
    // 保存token到文件
    bool save_token(const std::string& filename = "miot_token.json") const;
    
    // 从文件加载token
    bool load_token(const std::string& filename = "miot_token.json");
    
    // 检查token有效性
    bool is_token_valid() const;
    
private:
    std::string client_id_;
    std::string redirect_uri_;
    std::string cloud_server_;
    std::string oauth_host_;
    std::string state_;
    std::string device_id_;
    TokenInfo token_;
    
    // HTTP请求
    std::string http_get(const std::string& url) const;
    std::string http_post(const std::string& url, const std::string& data) const;
    
    // 生成state和device_id
    void generate_ids();
    
    // URL编码
    std::string url_encode(const std::string& value) const;
};

} // namespace miot

#endif // MIOT_OAUTH_H

