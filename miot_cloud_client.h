/**
 * MIoT Cloud API Client
 * 
 * A C++ implementation of Xiaomi IoT Cloud API client with RSA+AES encryption.
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#ifndef MIOT_CLOUD_CLIENT_H
#define MIOT_CLOUD_CLIENT_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace miot {

// Xiaomi API Constants
constexpr const char* OAUTH2_CLIENT_ID = "2882303761520431603";
constexpr const char* OAUTH2_API_HOST_DEFAULT = "mico.api.mijia.tech";
constexpr const char* MIHOME_HTTP_USER_AGENT = "mico/docker";
constexpr const char* MIHOME_HTTP_X_CLIENT_BIZID = "micoapi";
constexpr const char* MIHOME_HTTP_X_ENCRYPT_TYPE = "1";

// Xiaomi API Public Key for RSA encryption
constexpr const char* MIHOME_HTTP_API_PUBKEY = 
"-----BEGIN PUBLIC KEY-----\n"
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAzH220YGgZOlXJ4eSleFb\n"
"Beylq4qHsVNzhPTUTy/caDb4a3GzqH6SX4GiYRilZZZrjjU2ckkr8GM66muaIuJw\n"
"r8ZB9SSY3Hqwo32tPowpyxobTN1brmqGK146X6JcFWK/QiUYVXZlcHZuMgXLlWyn\n"
"zTMVl2fq7wPbzZwOYFxnSRh8YEnXz6edHAqJqLEqZMP00bNFBGP+yc9xmc7ySSyw\n"
"OgW/muVzfD09P2iWhl3x8N+fBBWpuI5HjvyQuiX8CZg3xpEeCV8weaprxMxR0epM\n"
"3l7T6rJuPXR1D7yhHaEQj2+dyrZTeJO8D8SnOgzV5j4bp1dTunlzBXGYVjqDsRhZ\n"
"qQIDAQAB\n"
"-----END PUBLIC KEY-----";

/**
 * @brief Device information from cloud
 */
struct CloudDeviceInfo {
    std::string did;                // Device ID
    std::string name;               // Device name (user-defined)
    std::string model;              // Device model (e.g., "xiaomi.camera.082ac1")
    std::string urn;                // Device URN
    std::string manufacturer;       // Manufacturer
    std::string token;              // Device token
    std::string uid;                // User ID
    bool online;                    // Online status
    
    // Network info
    std::string local_ip;           // Local IP address
    std::string ssid;               // WiFi SSID
    std::string bssid;              // WiFi BSSID
    int rssi;                       // WiFi signal strength
    
    // Extra info
    std::string fw_version;         // Firmware version
    std::string mcu_version;        // MCU version
    std::string platform;           // Platform
    int is_set_pincode;             // Is pincode set
    int pincode_type;               // Pincode type
    
    // Owner info
    std::string owner_id;           // Owner user ID
    std::string owner_nickname;     // Owner nickname
    
    // Home info
    std::string home_id;            // Home ID
    std::string home_name;          // Home name
    std::string room_id;            // Room ID
    std::string room_name;          // Room name
    
    CloudDeviceInfo() : online(false), rssi(0), is_set_pincode(0), pincode_type(0) {}
};

/**
 * @brief MIoT Cloud API Client
 * 
 * Provides access to Xiaomi IoT Cloud services with RSA+AES encryption.
 * Requires access_token from OAuth2 authentication.
 */
class MIoTCloudClient {
public:
    /**
     * @brief Constructor
     * @param access_token OAuth2 access token
     * @param cloud_server Cloud server region (cn, de, us, etc.)
     */
    explicit MIoTCloudClient(const std::string& access_token, const std::string& cloud_server = "cn");
    
    /**
     * @brief Destructor
     */
    ~MIoTCloudClient();
    
    // Disable copy
    MIoTCloudClient(const MIoTCloudClient&) = delete;
    MIoTCloudClient& operator=(const MIoTCloudClient&) = delete;
    
    /**
     * @brief Initialize the client (must call before use)
     * @return true if initialized successfully
     */
    bool init();
    
    /**
     * @brief Get device information by DID list
     * @param dids List of device IDs
     * @return Map of DID to device info
     */
    std::map<std::string, CloudDeviceInfo> get_devices(const std::vector<std::string>& dids);
    
    /**
     * @brief Get device information by single DID
     * @param did Device ID
     * @return Device info (empty if not found)
     */
    CloudDeviceInfo get_device(const std::string& did);
    
    /**
     * @brief Update access token
     * @param access_token New access token
     */
    void set_access_token(const std::string& access_token);
    
    /**
     * @brief Set cloud server region
     * @param cloud_server Server region (cn, de, us, etc.)
     */
    void set_cloud_server(const std::string& cloud_server);

private:
    // Configuration
    std::string access_token_;
    std::string cloud_server_;
    std::string host_;
    std::string base_url_;
    
    // Encryption keys
    std::vector<uint8_t> aes_key_;
    std::string client_secret_b64_;
    
    // Private methods
    bool generate_aes_key();
    bool generate_client_secret();
    std::string aes_encrypt_with_b64(const std::string& json_data);
    std::string aes_decrypt_with_b64(const std::string& encrypted_b64);
    std::string http_post(const std::string& url_path, const std::string& encrypted_data);
    std::map<std::string, std::string> get_api_headers();
    
    // Helper methods
    std::string base64_encode(const uint8_t* data, size_t len);
    std::vector<uint8_t> base64_decode(const std::string& encoded);
    std::vector<uint8_t> rsa_encrypt(const uint8_t* data, size_t len);
    std::vector<uint8_t> aes_cbc_encrypt(const std::vector<uint8_t>& plaintext);
    std::vector<uint8_t> aes_cbc_decrypt(const std::vector<uint8_t>& ciphertext);
    std::vector<uint8_t> pkcs7_pad(const std::vector<uint8_t>& data, size_t block_size);
    std::vector<uint8_t> pkcs7_unpad(const std::vector<uint8_t>& data);
};

/**
 * @brief Simple JSON parser/builder for API communication
 */
class SimpleJson {
public:
    static std::string build_device_list_request(const std::vector<std::string>& dids, int limit = 200);
    static std::map<std::string, CloudDeviceInfo> parse_device_list_response(const std::string& json);
    
private:
    static std::string escape_json_string(const std::string& str);
    static std::string extract_string(const std::string& json, const std::string& key);
    static int extract_int(const std::string& json, const std::string& key);
    static bool extract_bool(const std::string& json, const std::string& key);
};

} // namespace miot

#endif // MIOT_CLOUD_CLIENT_H

