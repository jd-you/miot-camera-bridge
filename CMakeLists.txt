cmake_minimum_required(VERSION 3.10)
project(miot_lan_discovery VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Release)

# Debug configuration
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
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED 
    gstreamer-1.0
    gstreamer-rtsp-server-1.0
    gstreamer-app-1.0
)


include_directories(${CMAKE_SOURCE_DIR}/include)


set(SOURCES
    src/bridge_main.cpp
    src/gst_rtsp_server.cpp
    src/http_server.cpp
    src/miot_camera_client.cpp
    src/miot_cloud_client.cpp
    src/miot_lan_device.cpp
    src/miot_oauth.cpp
)

add_executable(miot_camera_bridge ${SOURCES})

target_include_directories(miot_camera_bridge PRIVATE
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(miot_camera_bridge
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    ${GST_LIBRARIES}
)


install(TARGETS miot_camera_bridge DESTINATION bin)
