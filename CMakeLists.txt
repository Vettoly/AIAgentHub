cmake_minimum_required(VERSION 3.15)
project(AIAgentHub)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 设置 vcpkg 安装目录
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/vcpkg_installed/x64-windows")
if(NOT EXISTS "${VCPKG_INSTALLED_DIR}")
    set(VCPKG_INSTALLED_DIR "D:/vcpkg/installed/x64-windows")
endif()

#   自动查找 src 目录下所有的 .cpp 文件
file(GLOB SOURCES "src/*.cpp")

# 寻找第三方库
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)

#   使用所有找到的 .cpp 文件生成可执行文件
add_executable(AIAgentHub ${SOURCES})

#   强制扩容大对象支持
target_compile_options(AIAgentHub PRIVATE /bigobj)

#   包含本地的 include 目录和 vcpkg 的 include 目录
target_include_directories(AIAgentHub PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VCPKG_INSTALLED_DIR}/include
)

# 链接库
target_link_libraries(AIAgentHub PRIVATE 
    nlohmann_json::nlohmann_json 
    CURL::libcurl 
    unofficial::mysql-connector-cpp::connector
)