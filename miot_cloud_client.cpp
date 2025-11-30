/**
 * MIoT Cloud API Client - Implementation
 * 
 * Copyright (C) 2025
 * Licensed under MIT License
 */

#include "miot_cloud_client.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <random>
#include <algorithm>

// OpenSSL includes
#include <openssl/aes.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/rand.h>
#include <openssl/err.h>

// libcurl for HTTP
#include <curl/curl.h>

namespace miot {

namespace {

// Callback for libcurl to capture response
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

} // anonymous namespace

MIoTCloudClient::MIoTCloudClient(const std::string& access_token, const std::string& cloud_server)
    : access_token_(access_token),
      cloud_server_(cloud_server),
      host_(OAUTH2_API_HOST_DEFAULT)
{
    if (cloud_server != "cn") {
        host_ = cloud_server + "." + OAUTH2_API_HOST_DEFAULT;
    }
    base_url_ = "https://" + host_;
    
    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

MIoTCloudClient::~MIoTCloudClient() {
    curl_global_cleanup();
}

bool MIoTCloudClient::init() {
    if (!generate_aes_key()) {
        std::cerr << "[MIoTCloudClient] Failed to generate AES key" << std::endl;
        return false;
    }
    
    if (!generate_client_secret()) {
        std::cerr << "[MIoTCloudClient] Failed to generate client secret" << std::endl;
        return false;
    }
    
    std::cout << "[MIoTCloudClient] Initialized successfully" << std::endl;
    return true;
}

bool MIoTCloudClient::generate_aes_key() {
    aes_key_.resize(16);
    if (RAND_bytes(aes_key_.data(), 16) != 1) {
        return false;
    }
    return true;
}

bool MIoTCloudClient::generate_client_secret() {
    // Load public key
    BIO* bio = BIO_new_mem_buf(MIHOME_HTTP_API_PUBKEY, -1);
    if (!bio) {
        return false;
    }
    
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    
    if (!pkey) {
        return false;
    }
    
    // RSA encrypt the AES key
    std::vector<uint8_t> encrypted = rsa_encrypt(aes_key_.data(), aes_key_.size());
    EVP_PKEY_free(pkey);
    
    if (encrypted.empty()) {
        return false;
    }
    
    // Base64 encode
    client_secret_b64_ = base64_encode(encrypted.data(), encrypted.size());
    return true;
}

std::vector<uint8_t> MIoTCloudClient::rsa_encrypt(const uint8_t* data, size_t len) {
    BIO* bio = BIO_new_mem_buf(MIHOME_HTTP_API_PUBKEY, -1);
    if (!bio) {
        return {};
    }
    
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    
    if (!pkey) {
        return {};
    }
    
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return {};
    }
    
    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return {};
    }
    
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return {};
    }
    
    size_t outlen;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outlen, data, len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return {};
    }
    
    std::vector<uint8_t> result(outlen);
    if (EVP_PKEY_encrypt(ctx, result.data(), &outlen, data, len) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pkey);
        return {};
    }
    
    result.resize(outlen);
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    
    return result;
}

std::string MIoTCloudClient::base64_encode(const uint8_t* data, size_t len) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data, len);
    BIO_flush(bio);
    
    BUF_MEM* buf_mem;
    BIO_get_mem_ptr(bio, &buf_mem);
    
    std::string result(buf_mem->data, buf_mem->length);
    BIO_free_all(bio);
    
    return result;
}

std::vector<uint8_t> MIoTCloudClient::base64_decode(const std::string& encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.data(), encoded.size());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    
    std::vector<uint8_t> result(encoded.size());
    int decoded_len = BIO_read(bio, result.data(), encoded.size());
    BIO_free_all(bio);
    
    if (decoded_len > 0) {
        result.resize(decoded_len);
    } else {
        result.clear();
    }
    
    return result;
}

std::vector<uint8_t> MIoTCloudClient::pkcs7_pad(const std::vector<uint8_t>& data, size_t block_size) {
    size_t padding = block_size - (data.size() % block_size);
    std::vector<uint8_t> padded = data;
    padded.insert(padded.end(), padding, static_cast<uint8_t>(padding));
    return padded;
}

std::vector<uint8_t> MIoTCloudClient::pkcs7_unpad(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return {};
    }
    
    uint8_t padding = data.back();
    if (padding > data.size() || padding > 16) {
        return data;
    }
    
    std::vector<uint8_t> unpadded(data.begin(), data.end() - padding);
    return unpadded;
}

std::vector<uint8_t> MIoTCloudClient::aes_cbc_encrypt(const std::vector<uint8_t>& plaintext) {
    // Pad the plaintext
    std::vector<uint8_t> padded = pkcs7_pad(plaintext, AES_BLOCK_SIZE);
    
    std::vector<uint8_t> ciphertext(padded.size());
    
    AES_KEY aes_key;
    AES_set_encrypt_key(aes_key_.data(), 128, &aes_key);
    
    // Use AES key as IV (same as Python implementation)
    uint8_t iv[AES_BLOCK_SIZE];
    std::memcpy(iv, aes_key_.data(), AES_BLOCK_SIZE);
    
    AES_cbc_encrypt(padded.data(), ciphertext.data(), padded.size(), &aes_key, iv, AES_ENCRYPT);
    
    return ciphertext;
}

std::vector<uint8_t> MIoTCloudClient::aes_cbc_decrypt(const std::vector<uint8_t>& ciphertext) {
    std::vector<uint8_t> plaintext(ciphertext.size());
    
    AES_KEY aes_key;
    AES_set_decrypt_key(aes_key_.data(), 128, &aes_key);
    
    // Use AES key as IV
    uint8_t iv[AES_BLOCK_SIZE];
    std::memcpy(iv, aes_key_.data(), AES_BLOCK_SIZE);
    
    AES_cbc_encrypt(ciphertext.data(), plaintext.data(), ciphertext.size(), &aes_key, iv, AES_DECRYPT);
    
    return pkcs7_unpad(plaintext);
}

std::string MIoTCloudClient::aes_encrypt_with_b64(const std::string& json_data) {
    std::vector<uint8_t> plaintext(json_data.begin(), json_data.end());
    std::vector<uint8_t> encrypted = aes_cbc_encrypt(plaintext);
    return base64_encode(encrypted.data(), encrypted.size());
}

std::string MIoTCloudClient::aes_decrypt_with_b64(const std::string& encrypted_b64) {
    std::vector<uint8_t> encrypted = base64_decode(encrypted_b64);
    if (encrypted.empty()) {
        return "";
    }
    
    std::vector<uint8_t> decrypted = aes_cbc_decrypt(encrypted);
    return std::string(decrypted.begin(), decrypted.end());
}

std::map<std::string, std::string> MIoTCloudClient::get_api_headers() {
    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "text/plain";
    headers["User-Agent"] = MIHOME_HTTP_USER_AGENT;
    headers["X-Client-BizId"] = MIHOME_HTTP_X_CLIENT_BIZID;
    headers["X-Encrypt-Type"] = MIHOME_HTTP_X_ENCRYPT_TYPE;
    headers["X-Client-AppId"] = OAUTH2_CLIENT_ID;
    headers["X-Client-Secret"] = client_secret_b64_;
    headers["Host"] = host_;
    headers["Authorization"] = "Bearer" + access_token_;
    return headers;
}

std::string MIoTCloudClient::http_post(const std::string& url_path, const std::string& encrypted_data) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[MIoTCloudClient] Failed to initialize CURL" << std::endl;
        return "";
    }
    
    std::string url = base_url_ + url_path;
    std::string response;
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encrypted_data.c_str());
    
    // Set headers
    struct curl_slist* headers = nullptr;
    auto api_headers = get_api_headers();
    for (const auto& pair : api_headers) {
        std::string header = pair.first + ": " + pair.second;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[MIoTCloudClient] HTTP request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }
    
    if (http_code != 200) {
        std::cerr << "[MIoTCloudClient] HTTP error code: " << http_code << std::endl;
        return "";
    }
    
    return response;
}

std::map<std::string, CloudDeviceInfo> MIoTCloudClient::get_devices(const std::vector<std::string>& dids) {
    if (dids.empty()) {
        return {};
    }
    
    // Build request JSON
    std::string request_json = SimpleJson::build_device_list_request(dids);
    
    // Encrypt request
    std::string encrypted = aes_encrypt_with_b64(request_json);
    
    // Send HTTP POST
    std::string response = http_post("/app/v2/home/device_list_page", encrypted);
    if (response.empty()) {
        std::cerr << "[MIoTCloudClient] Empty response from server" << std::endl;
        return {};
    }
    
    // Decrypt response
    std::string decrypted = aes_decrypt_with_b64(response);
    if (decrypted.empty()) {
        std::cerr << "[MIoTCloudClient] Failed to decrypt response" << std::endl;
        return {};
    }
    
    // Parse response JSON
    return SimpleJson::parse_device_list_response(decrypted);
}

CloudDeviceInfo MIoTCloudClient::get_device(const std::string& did) {
    std::vector<std::string> dids = {did};
    auto devices = get_devices(dids);
    
    auto it = devices.find(did);
    if (it != devices.end()) {
        return it->second;
    }
    
    return CloudDeviceInfo();
}

void MIoTCloudClient::set_access_token(const std::string& access_token) {
    access_token_ = access_token;
}

void MIoTCloudClient::set_cloud_server(const std::string& cloud_server) {
    cloud_server_ = cloud_server;
    if (cloud_server != "cn") {
        host_ = cloud_server + "." + std::string(OAUTH2_API_HOST_DEFAULT);
    } else {
        host_ = OAUTH2_API_HOST_DEFAULT;
    }
    base_url_ = "https://" + host_;
}

// SimpleJson implementation
std::string SimpleJson::escape_json_string(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::string SimpleJson::build_device_list_request(const std::vector<std::string>& dids, int limit) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"limit\":" << limit << ",";
    oss << "\"get_split_device\":true,";
    oss << "\"dids\":[";
    
    for (size_t i = 0; i < dids.size(); ++i) {
        oss << "\"" << escape_json_string(dids[i]) << "\"";
        if (i < dids.size() - 1) {
            oss << ",";
        }
    }
    
    oss << "]}";
    return oss.str();
}

std::string SimpleJson::extract_string(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":\"";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return "";
    }
    
    pos += search_key.length();
    size_t end_pos = json.find("\"", pos);
    if (end_pos == std::string::npos) {
        return "";
    }
    
    return json.substr(pos, end_pos - pos);
}

int SimpleJson::extract_int(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return 0;
    }
    
    pos += search_key.length();
    size_t end_pos = pos;
    while (end_pos < json.length() && (std::isdigit(json[end_pos]) || json[end_pos] == '-')) {
        end_pos++;
    }
    
    if (end_pos == pos) {
        return 0;
    }
    
    try {
        return std::stoi(json.substr(pos, end_pos - pos));
    } catch (...) {
        return 0;
    }
}

bool SimpleJson::extract_bool(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return false;
    }
    
    pos += search_key.length();
    return json.substr(pos, 4) == "true";
}

std::map<std::string, CloudDeviceInfo> SimpleJson::parse_device_list_response(const std::string& json) {
    std::map<std::string, CloudDeviceInfo> devices;
    
    // Find the "list" array
    size_t list_pos = json.find("\"list\":[");
    if (list_pos == std::string::npos) {
        std::cerr << "[SimpleJson] No 'list' found in response" << std::endl;
        return devices;
    }
    
    size_t pos = list_pos + 8;  // Skip "\"list\":["
    
    // Parse each device object
    while (pos < json.length()) {
        // Find next device object
        size_t obj_start = json.find("{", pos);
        if (obj_start == std::string::npos) {
            break;
        }
        
        // Find matching closing brace
        int brace_count = 1;
        size_t obj_end = obj_start + 1;
        while (obj_end < json.length() && brace_count > 0) {
            if (json[obj_end] == '{') brace_count++;
            else if (json[obj_end] == '}') brace_count--;
            obj_end++;
        }
        
        if (brace_count != 0) {
            break;
        }
        
        std::string device_json = json.substr(obj_start, obj_end - obj_start);
        
        CloudDeviceInfo info;
        info.did = extract_string(device_json, "did");
        info.name = extract_string(device_json, "name");
        info.model = extract_string(device_json, "model");
        info.urn = extract_string(device_json, "spec_type");
        info.token = extract_string(device_json, "token");
        info.uid = extract_string(device_json, "uid");
        info.online = extract_bool(device_json, "isOnline");
        info.local_ip = extract_string(device_json, "local_ip");
        info.ssid = extract_string(device_json, "ssid");
        info.bssid = extract_string(device_json, "bssid");
        info.rssi = extract_int(device_json, "rssi");
        
        if (!info.did.empty() && !info.model.empty()) {
            // Extract manufacturer from model (e.g., "xiaomi.camera.082ac1" -> "xiaomi")
            size_t dot_pos = info.model.find('.');
            if (dot_pos != std::string::npos) {
                info.manufacturer = info.model.substr(0, dot_pos);
            }
            
            devices[info.did] = info;
        }
        
        pos = obj_end;
    }
    
    return devices;
}

} // namespace miot

