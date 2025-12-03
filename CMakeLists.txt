cmake_minimum_required(VERSION 3.10)
project(miot_lan_discovery VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Debug configuration
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED 
    gstreamer-1.0
    gstreamer-rtsp-server-1.0
    gstreamer-app-1.0
)

find_package(nlohmann_json 3.2.0 REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Library: MIoT LAN Device Discovery
add_library(miot_lan_device
    miot_lan_device.cpp
)

target_include_directories(miot_lan_device PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(miot_lan_device
    Threads::Threads
)

# Library: MIoT Cloud API Client
add_library(miot_cloud_client
    miot_cloud_client.cpp
)

# Library: MIoT Camera Client
add_library(miot_camera_client
    miot_camera_client.cpp
)

target_include_directories(miot_cloud_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIR}
)

target_link_libraries(miot_cloud_client
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
)


# Executable: LAN Discovery Demo (original)
add_executable(miot_lan_discovery_demo
    bridge_main.cpp
    src/miot_oauth.cpp
    src/http_server.cpp
    gst_rtsp_server.cpp    
)
target_include_directories(miot_lan_discovery_demo PRIVATE
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(miot_lan_discovery_demo
    miot_lan_device
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    miot_cloud_client
    miot_camera_client
    ${GST_LIBRARIES}
)

# Executable: Integrated Demo (LAN Discovery + Cloud API)
add_executable(miot_discovery_with_cloud
    main_with_cloud.cpp
)

target_link_libraries(miot_discovery_with_cloud
    miot_lan_device
    miot_cloud_client
    Threads::Threads
)



target_include_directories(miot_camera_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(miot_camera_client
    ${CMAKE_DL_LIBS}
)

# Executable: Camera First Frame Test
add_executable(test_first_frame
    test_first_frame.cpp
)

target_link_libraries(test_first_frame
    miot_camera_client
    miot_cloud_client
    miot_lan_device
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# Install targets
install(TARGETS miot_lan_device miot_cloud_client miot_camera_client 
        miot_lan_discovery_demo miot_discovery_with_cloud test_first_frame
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES miot_lan_device.h miot_cloud_client.h miot_camera_client.h
    DESTINATION include
)

# # 包含目录
# include_directories(${CMAKE_SOURCE_DIR}/include)
# include_directories(${OPENSSL_INCLUDE_DIR})

# # 源文件
# set(SOURCES
#     src/main.cpp
#     src/miot_oauth.cpp
#     src/http_server.cpp
# )

# # 可执行文件
# add_executable(miot_bridge ${SOURCES})

# # 链接库
# target_link_libraries(miot_bridge
#     CURL::libcurl
#     nlohmann_json::nlohmann_json
#     OpenSSL::SSL
#     OpenSSL::Crypto
#     pthread
# )

# # 安装
# install(TARGETS miot_bridge DESTINATION bin)
