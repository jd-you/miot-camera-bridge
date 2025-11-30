// miot_camera_lite.h
#ifndef MIOT_CAMERA_LITE_H
#define MIOT_CAMERA_LITE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============ 类型定义 ============

// 摄像头实例句柄
typedef void* miot_camera_instance_t;

// 摄像头信息结构
typedef struct {
    const char* did;           // 设备ID
    const char* model;         // 设备型号
    uint8_t channel_count;     // 通道数
} miot_camera_info_t;

// 摄像头配置结构
typedef struct {
    uint8_t* video_qualities;  // 视频质量数组
    bool enable_audio;         // 是否启用音频
    const char* pin_code;      // 密码（可选）
} miot_camera_config_t;

// 帧头结构
typedef struct {
    uint32_t codec_id;         // 编码格式ID
    uint32_t length;           // 数据长度
    uint64_t timestamp;        // 时间戳
    uint32_t sequence;         // 序列号
    uint32_t frame_type;       // 帧类型
    uint8_t channel;           // 通道
} miot_camera_frame_header_t;

// ============ 回调函数类型 ============

// 日志回调：level, message
typedef void (*miot_camera_log_handler_t)(int level, const char* message);

// 状态变化回调：status
typedef void (*miot_camera_status_changed_t)(int status);

// 原始数据回调：frame_header, data
typedef void (*miot_camera_raw_data_t)(
    const miot_camera_frame_header_t* frame_header,
    const uint8_t* data
);

// ============ API函数声明 ============

/**
 * 设置日志回调
 * @param handler 日志处理函数
 */
void miot_camera_set_log_handler(miot_camera_log_handler_t handler);

/**
 * 初始化库
 * @param host 服务器地址（如 "api.io.mi.com"）
 * @param client_id OAuth2 客户端ID
 * @param access_token 访问令牌
 * @return 0表示成功，非0表示失败
 */
int miot_camera_init(
    const char* host,
    const char* client_id,
    const char* access_token
);

/**
 * 释放库资源
 */
void miot_camera_deinit(void);

/**
 * 更新访问令牌
 * @param access_token 新的访问令牌
 * @return 0表示成功，非0表示失败
 */
int miot_camera_update_access_token(const char* access_token);

/**
 * 创建摄像头实例
 * @param camera_info 摄像头信息
 * @return 摄像头实例句柄，失败返回NULL
 */
miot_camera_instance_t miot_camera_new(const miot_camera_info_t* camera_info);

/**
 * 释放摄像头实例
 * @param instance 摄像头实例句柄
 */
void miot_camera_free(miot_camera_instance_t instance);

/**
 * 启动摄像头
 * @param instance 摄像头实例句柄
 * @param config 摄像头配置
 * @return 0表示成功，非0表示失败
 */
int miot_camera_start(
    miot_camera_instance_t instance,
    const miot_camera_config_t* config
);

/**
 * 停止摄像头
 * @param instance 摄像头实例句柄
 * @return 0表示成功，非0表示失败
 */
int miot_camera_stop(miot_camera_instance_t instance);

/**
 * 获取摄像头状态
 * @param instance 摄像头实例句柄
 * @return 状态码
 */
int miot_camera_status(miot_camera_instance_t instance);

/**
 * 获取库版本
 * @return 版本字符串
 */
const char* miot_camera_version(void);

/**
 * 注册状态变化回调
 * @param instance 摄像头实例句柄
 * @param callback 回调函数
 * @return 0表示成功，非0表示失败
 */
int miot_camera_register_status_changed(
    miot_camera_instance_t instance,
    miot_camera_status_changed_t callback
);

/**
 * 注销状态变化回调
 * @param instance 摄像头实例句柄
 * @return 0表示成功，非0表示失败
 */
int miot_camera_unregister_status_changed(miot_camera_instance_t instance);

/**
 * 注册原始数据回调
 * @param instance 摄像头实例句柄
 * @param callback 回调函数
 * @param channel 通道号
 * @return 0表示成功，非0表示失败
 */
int miot_camera_register_raw_data(
    miot_camera_instance_t instance,
    miot_camera_raw_data_t callback,
    uint8_t channel
);

/**
 * 注销原始数据回调
 * @param instance 摄像头实例句柄
 * @param channel 通道号
 * @return 0表示成功，非0表示失败
 */
int miot_camera_unregister_raw_data(
    miot_camera_instance_t instance,
    uint8_t channel
);

#ifdef __cplusplus
}
#endif

#endif // MIOT_CAMERA_LITE_H