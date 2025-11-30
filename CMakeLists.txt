cmake_minimum_required(VERSION 3.10)
project(miot_camera_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_DEBUG "-g")

# 查找依赖库
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenSSL REQUIRED)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

# 源文件
set(SOURCES
    src/main.cpp
    src/miot_oauth.cpp
    src/http_server.cpp
)

# 可执行文件
add_executable(miot_bridge ${SOURCES})

# 链接库
target_link_libraries(miot_bridge
    CURL::libcurl
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# 安装
install(TARGETS miot_bridge DESTINATION bin)

