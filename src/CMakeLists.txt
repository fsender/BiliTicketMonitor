cmake_minimum_required(VERSION 3.24)
project(BiliTicketMonitor)

add_definitions(-DCURL_STATICLIB=1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)  # cJSON需要C99标准

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 仅查找静态库
# 2. 强制优先使用静态库
# set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib) # 仅查找静态库
# set(CMAKE_FIND_LIBRARY_PREFERS_STATIC TRUE) # CMake 3.24+
# set(BUILD_SHARED_LIBS OFF) # 全局关闭共享库

# 添加可执行文件
add_executable(BiliTicketMonitor
    main.cpp
    cJSON.c   # 包含cJSON源代码
    ../resources/ico-out.o #文件图标 对于Linux用户可能没什么用
)

# 查找libcurl包
find_package(CURL REQUIRED)

# 包含头文件目录
target_include_directories(BiliTicketMonitor PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}  # 包含当前目录以找到cJSON.h
)

# 链接库
target_link_libraries(BiliTicketMonitor PRIVATE
    ${CURL_LIBRARIES} #CURL::libcurl

)

# Windows特定设置
if(WIN32)
    # Windows系统需要额外的网络库
    target_link_libraries(BiliTicketMonitor PRIVATE ws2_32 wininet pthread)
else()
    # 非Windows系统需要pthread
    target_link_libraries(BiliTicketMonitor PRIVATE pthread)
endif()

# 输出构建信息
message(STATUS "Build configuration complete")
message(STATUS "  Target: BiliTicketMonitor")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Libcurl found at: ${CURL_LIBRARIES}")

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s -static ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -static ${CMAKE_C_FLAGS_RELEASE}")
